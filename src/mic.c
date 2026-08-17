#include "mic.h"

#include <stdlib.h>
#include "esp_check.h"
#include "esp_timer.h"
#include "esp_adc/adc_continuous.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "speaker.h"
#include "streams.h"
#include "event_group.h"
#include "diagnostics.h"
#include "esp_log.h"

static const char *TAG = "MIC";

static adc_oneshot_unit_handle_t mic_adc_handle;
static float mic_baseline = 0.0f;

static int mic_abs(int value)
{
    return value < 0 ? -value : value;
}

static int16_t mic_clamp16(int32_t value)
{
    if (value > 32767)
    {
        return 32767;
    }
    if (value < -32768)
    {
        return -32768;
    }
    return (int16_t)value;
}

esp_err_t mic_init(void)
{
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = MIC_ADC_UNIT,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&init_cfg, &mic_adc_handle), "MIC", "adc unit");

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = MIC_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(
        adc_oneshot_config_channel(mic_adc_handle, MIC_ADC_CHANNEL, &chan_cfg),
        "MIC",
        "adc channel"
    );

    int64_t sum = 0;
    for (int i = 0; i < MIC_CALIB_SAMPLES; i++)
    {
        int raw = 0;
        ESP_RETURN_ON_ERROR(
            adc_oneshot_read(mic_adc_handle, MIC_ADC_CHANNEL, &raw),
            "MIC",
            "calib read"
        );
        sum += raw;
    }

    mic_baseline = (float)sum / MIC_CALIB_SAMPLES;
    return ESP_OK;
}

esp_err_t mic_read_raw(int *raw)
{
    return adc_oneshot_read(mic_adc_handle, MIC_ADC_CHANNEL, raw);
}

esp_err_t mic_record(int16_t *buffer, size_t buffer_bytes, uint32_t duration_ms)
{
    const size_t max_samples = buffer_bytes / sizeof(int16_t);
    const size_t target_samples = (size_t)SAMPLE_RATE * duration_ms / 1000;
    const size_t num_samples = target_samples < max_samples ? target_samples : max_samples;
    const int64_t period_us = 1000000 / SAMPLE_RATE;
    int64_t next_us = esp_timer_get_time();

    for (size_t i = 0; i < num_samples; i++)
    {
        int raw = 0;
        ESP_RETURN_ON_ERROR(
            adc_oneshot_read(mic_adc_handle, MIC_ADC_CHANNEL, &raw),
            "MIC",
            "record read"
        );

        mic_baseline = mic_baseline * 0.995f + (float)raw * 0.005f;
        int delta = raw - (int)mic_baseline;
        buffer[i] = mic_clamp16((int32_t)delta * MIC_GAIN);

        next_us += period_us;
        while (esp_timer_get_time() < next_us)
        {
            // czekamy do nastepnej probki
        }
    }

    return ESP_OK;
}

esp_err_t mic_read_samples(int16_t *buffer, size_t num_samples)
{
    const int64_t period_us = 1'000'000 / SAMPLE_RATE;
    int64_t next_us = esp_timer_get_time();

    for (size_t i = 0; i < num_samples; i++)
    {
        int raw = 0;
        ESP_RETURN_ON_ERROR( mic_read_raw(&raw), "MIC", "stream read");

        mic_baseline = mic_baseline * 0.995f + (float)raw * 0.005f;
        int delta = raw - (int)mic_baseline;
        buffer[i] = mic_clamp16((int32_t)delta * MIC_GAIN);

        next_us += period_us;
        while (esp_timer_get_time() < next_us)
        {
        }
    }

    return ESP_OK;
}

int mic_measure_level(void)
{
    int peak = 0;

    for (int i = 0; i < MIC_LEVEL_SAMPLES; i++)
    {
        int raw = 0;
        if (adc_oneshot_read(mic_adc_handle, MIC_ADC_CHANNEL, &raw) != ESP_OK)
        {
            continue;
        }

        int delta = mic_abs(raw - (int)mic_baseline);
        if (delta > peak)
        {
            peak = delta;
        }
    }

    return peak;
}

int mic_get_offset(void)
{
    return (int)mic_baseline;
}



static adc_continuous_handle_t mic_adc_handle_cont;
static float mic_baseline_cont = 0.0f;
static bool mic_baseline_primed = false;

#define MIC_CONT_ADC_FREQ_HZ  ( SAMPLE_RATE * MIC_CONT_DECIMATION )

_Static_assert(
    MIC_CONT_ADC_FREQ_HZ >= SOC_ADC_SAMPLE_FREQ_THRES_LOW &&
    MIC_CONT_ADC_FREQ_HZ <= SOC_ADC_SAMPLE_FREQ_THRES_HIGH,
    "SAMPLE_RATE * MIC_CONT_DECIMATION poza zakresem ADC continuous"
);

esp_err_t init_mic_cont(void)
{
    adc_continuous_handle_cfg_t handle_cfg = {
        .max_store_buf_size = MIC_CONT_STORE_SIZE,
        .conv_frame_size = MIC_CONT_FRAME_SIZE,
    };
    ESP_RETURN_ON_ERROR(
        adc_continuous_new_handle(&handle_cfg, &mic_adc_handle_cont),
        "MIC",
        "adc handle"
    );

    adc_digi_pattern_config_t pattern = {
        .atten = MIC_ADC_ATTEN,
        .channel = MIC_ADC_CHANNEL,
        .unit = MIC_ADC_UNIT,
        .bit_width = SOC_ADC_DIGI_MAX_BITWIDTH,
    };

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = MIC_CONT_ADC_FREQ_HZ,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
        .pattern_num = 1,
        .adc_pattern = &pattern,
    };
    ESP_RETURN_ON_ERROR(
        adc_continuous_config(mic_adc_handle_cont, &dig_cfg),
        "MIC",
        "adc config"
    );

    // ADC startujemy dopiero w trybie TRANSMIT - patrz TASK_cont_mic_stream
    return ESP_OK;
}

static int32_t mic_gain_q = MIC_GAIN * MIC_GAIN_ONE;
static int32_t mic_block_peak = 0;
static bool mic_squelch_open = false;

// Wolane raz na ramke ADC (~5 ms), a nie raz na probke - zmiana wzmocnienia w srodku
// bloku wprowadzalaby wlasne zniekształcenie.
static void mic_agc_update(void)
{
    const int32_t resting_gain = MIC_GAIN * MIC_GAIN_ONE;
    static int64_t hold_until_us = 0;
    static int64_t squelch_close_at_us = 0;

    int32_t peak = mic_block_peak;
    int64_t now_us = esp_timer_get_time();
    mic_block_peak = 0;

    if (peak < MIC_AGC_NOISE_FLOOR)
    {
        if (now_us >= squelch_close_at_us)
        {
            mic_squelch_open = false;
        }

        if (now_us < hold_until_us)
        {
            return; // pauza w zdaniu - trzymamy poziom dobrany na glosie
        }

        if (mic_gain_q > resting_gain)
        {
            mic_gain_q -= (mic_gain_q - resting_gain) >> MIC_AGC_DECAY_SHIFT;
        }
        return;
    }

    mic_squelch_open = true;
    squelch_close_at_us = now_us + MIC_SQUELCH_HANG_MS * 1000;
    hold_until_us = now_us + MIC_AGC_HOLD_MS * 1000;

    int32_t wanted = (MIC_AGC_TARGET_PEAK * MIC_GAIN_ONE) / peak;
    if (wanted > MIC_AGC_GAIN_MAX)
    {
        wanted = MIC_AGC_GAIN_MAX;
    }
    if (wanted < MIC_AGC_GAIN_MIN)
    {
        wanted = MIC_AGC_GAIN_MIN;
    }

    int32_t diff = wanted - mic_gain_q;
    mic_gain_q += (diff < 0)
        ? (diff >> MIC_AGC_ATTACK_SHIFT)
        : (diff >> MIC_AGC_RISE_SHIFT);
}

#if AUDIO_DIAGNOSTICS

static int32_t stat_peak_delta = 0;   // szczyt PRZED wzmocnieniem
static uint32_t stat_clipped = 0;
static uint32_t stat_samples = 0;

static void mic_note_sample(int32_t delta, int32_t scaled)
{
    int32_t magnitude = delta < 0 ? -delta : delta;
    if (magnitude > stat_peak_delta)
    {
        stat_peak_delta = magnitude;
    }
    if (scaled > 32767 || scaled < -32768)
    {
        stat_clipped++;
    }
    stat_samples++;
}

static void mic_report_levels(void)
{
    static int64_t next_report_us = 0;
    int64_t now_us = esp_timer_get_time();

    if (now_us < next_report_us)
    {
        return;
    }
    next_report_us = now_us + 1000000;

    if (stat_samples > 0)
    {
        ESP_LOGI(TAG, "poziom: szczyt %ld (agc x%ld.%02ld -> %ld), przester %lu/%lu probek",
                 (long)stat_peak_delta,
                 (long)(mic_gain_q / MIC_GAIN_ONE),
                 (long)((mic_gain_q % MIC_GAIN_ONE) * 100 / MIC_GAIN_ONE),
                 (long)(stat_peak_delta * mic_gain_q / MIC_GAIN_ONE),
                 (unsigned long)stat_clipped,
                 (unsigned long)stat_samples);
    }

    stat_peak_delta = 0;
    stat_clipped = 0;
    stat_samples = 0;
}

#else
#define mic_note_sample(delta, scaled) ((void)0)
#define mic_report_levels()            ((void)0)
#endif

static size_t process_recorded_audio(uint8_t *raw, uint32_t got, int16_t *out)
{
    static int32_t acc = 0;
    static int acc_count = 0;
    size_t n = 0;

    for (uint32_t i = 0; i + SOC_ADC_DIGI_RESULT_BYTES <= got; i += SOC_ADC_DIGI_RESULT_BYTES)
    {
        adc_digi_output_data_t *p = (adc_digi_output_data_t *)&raw[i];
        if (p->type1.channel != MIC_ADC_CHANNEL)
        {
            continue;
        }

        int sample = p->type1.data;

        if (! mic_baseline_primed)
        {
            mic_baseline_cont = (float)sample;
            mic_baseline_primed = true;
            acc = 0;
            acc_count = 0;
        }

        mic_baseline_cont = mic_baseline_cont * 0.995f + (float)sample * 0.005f;
        acc += sample - (int)mic_baseline_cont;

        if (++acc_count < MIC_CONT_DECIMATION)
        {
            continue;
        }

        int32_t delta = acc / MIC_CONT_DECIMATION;
        int32_t scaled = delta * mic_gain_q / MIC_GAIN_ONE;

        int32_t magnitude = delta < 0 ? -delta : delta;
        if (magnitude > mic_block_peak)
        {
            mic_block_peak = magnitude;
        }

        mic_note_sample(delta, scaled);

        out[n++] = mic_squelch_open ? mic_clamp16(scaled) : 0;
        acc = 0;
        acc_count = 0;
    }

    return n;
}

void TASK_cont_mic_stream(void *arg)
{
    (void)arg;
    uint8_t raw[MIC_CONT_FRAME_SIZE];
    int16_t pcm[MIC_CONT_FRAME_SIZE / SOC_ADC_DIGI_RESULT_BYTES];
    uint32_t got = 0;

    while (1)
    {
        blockWaitForTransmitMode();

        // Filtr DC startuje od wartosci z poprzedniej sesji, wiec pierwsze probki
        // mialyby ogromne delta i wchodzilyby w przester - primujemy go pierwsza probka
        mic_baseline_primed = false;

        adc_continuous_flush_pool(mic_adc_handle_cont);
        adc_continuous_start(mic_adc_handle_cont);

        while (isTransmitModeStillActive())
        {
            if (adc_continuous_read(mic_adc_handle_cont, raw, sizeof(raw), &got, MIC_CONT_READ_TIMEOUT_MS) != ESP_OK)
            {
                continue;
            }

            size_t n = process_recorded_audio(raw, got, pcm);
            if (n > 0)
            {
                xStreamBufferSend(mic_to_en_crypto_stream, pcm, n * sizeof(int16_t), common_timeout);
            }

            mic_agc_update();
            mic_report_levels();
        }

        adc_continuous_stop(mic_adc_handle_cont);
    }
}
