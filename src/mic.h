#ifndef MIC_H
#define MIC_H

#include <stdint.h>
#include <stdlib.h>
#include "esp_err.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_continuous.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "speaker.h"
#include "mic_stream.h"

#define MIC_GPIO            36
#define MIC_ADC_UNIT        ADC_UNIT_1
#define MIC_ADC_CHANNEL     ADC_CHANNEL_0
#define MIC_ADC_ATTEN       ADC_ATTEN_DB_12
#define MIC_CALIB_SAMPLES   256
#define MIC_LEVEL_SAMPLES   128
#define MIC_GAIN            35

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

static esp_err_t mic_init(void)
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

static esp_err_t mic_read_raw(int *raw)
{
    return adc_oneshot_read(mic_adc_handle, MIC_ADC_CHANNEL, raw);
}

static esp_err_t mic_record(int16_t *buffer, size_t buffer_bytes, uint32_t duration_ms)
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

static esp_err_t mic_read_samples(int16_t *buffer, size_t num_samples)
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

static int mic_measure_level(void)
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

static int mic_get_offset(void)
{
    return (int)mic_baseline;
}



static adc_continuous_handle_t mic_adc_handle_cont;
static float mic_baseline_cont = 0.0f;

#define MIC_CONT_FRAME_SIZE   ( 512 )
#define MIC_CONT_STORE_SIZE   ( 2048 )

static esp_err_t mic_init_cont(void)
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
        .bit_width = ADC_BITWIDTH_DEFAULT,
    };

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 8'000,                  // MIC sampling rate
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

    ESP_RETURN_ON_ERROR(
        adc_continuous_start(mic_adc_handle_cont),
        "MIC",
        "adc start"
    );

    return ESP_OK;
}

static size_t process_recorded_audio(uint8_t *raw, uint32_t got, int16_t *out)
{
    size_t n = 0;

    for (uint32_t i = 0; i + SOC_ADC_DIGI_RESULT_BYTES <= got; i += SOC_ADC_DIGI_RESULT_BYTES)
    {
        adc_digi_output_data_t *p = (adc_digi_output_data_t *)&raw[i];
        if (p->type1.channel != MIC_ADC_CHANNEL)
        {
            continue;
        }

        int sample = p->type1.data;
        mic_baseline_cont = mic_baseline_cont * 0.995f + (float)sample * 0.005f;
        int delta = sample - (int)mic_baseline_cont;
        out[n++] = mic_clamp16((int32_t)delta * MIC_GAIN);
    }

    return n;
}

static void cont_mic_stream_task(void *arg)
{
    (void)arg;
    uint8_t raw[MIC_CONT_FRAME_SIZE];
    int16_t pcm[MIC_CONT_FRAME_SIZE / SOC_ADC_DIGI_RESULT_BYTES];
    uint32_t got = 0;

    while (1)
    {
        if (adc_continuous_read(mic_adc_handle_cont, raw, sizeof(raw), &got, portMAX_DELAY) == ESP_OK)
        {
            size_t n = process_recorded_audio(raw, got, pcm);
            if (n > 0)
            {
                xStreamBufferSend(audio_stream, pcm, n * sizeof(int16_t), portMAX_DELAY);
            }
        }
    }
}




#endif
