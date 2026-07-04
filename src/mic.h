#ifndef MIC_H
#define MIC_H

#include <stdint.h>
#include <stdlib.h>
#include "esp_err.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "esp_adc/adc_oneshot.h"
#include "speaker.h"

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
    const int64_t period_us = 1000000 / SAMPLE_RATE;
    int64_t next_us = esp_timer_get_time();

    for (size_t i = 0; i < num_samples; i++)
    {
        int raw = 0;
        ESP_RETURN_ON_ERROR(
            adc_oneshot_read(mic_adc_handle, MIC_ADC_CHANNEL, &raw),
            "MIC",
            "stream read"
        );

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

#endif
