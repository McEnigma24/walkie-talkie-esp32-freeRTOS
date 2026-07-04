#ifndef SPEAKER_H
#define SPEAKER_H

#include <math.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"

#define I2S_WS    25   // LRC   → pin "25"
#define I2S_BCLK  26   // BCLK  → pin "26"
#define I2S_DOUT  27   // DIN   → pin "27"

#define SAMPLE_RATE     16000
#define TONE_HZ         440
#define AMPLITUDE       6000
#define BUFFER_SAMPLES  256
#define RECORD_SEC      2
#define RECORD_SAMPLES  (SAMPLE_RATE * RECORD_SEC)
#define PLAYBACK_VOLUME_PERCENT  30
#define SILENCE_FLUSH_BUFFERS    4

static i2s_chan_handle_t tx_handle;

static int16_t speaker_scale_sample(int16_t sample)
{
    int32_t scaled = (int32_t)sample * PLAYBACK_VOLUME_PERCENT / 100;
    if (scaled > 32767)
    {
        return 32767;
    }
    if (scaled < -32768)
    {
        return -32768;
    }
    return (int16_t)scaled;
}

static esp_err_t speaker_stop(void)
{
    int16_t silence[BUFFER_SAMPLES * 2] = {0};
    size_t bytes_written = 0;

    for (int i = 0; i < SILENCE_FLUSH_BUFFERS; i++)
    {
        ESP_RETURN_ON_ERROR(
            i2s_channel_write(
                tx_handle,
                silence,
                sizeof(silence),
                &bytes_written,
                portMAX_DELAY
            ),
            "I2S",
            "flush"
        );
    }

    return i2s_channel_disable(tx_handle);
}

static esp_err_t speaker_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &tx_handle, NULL), "I2S", "new channel");

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT,
            I2S_SLOT_MODE_STEREO
        ),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK,
            .ws   = I2S_WS,
            .dout = I2S_DOUT,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(tx_handle, &std_cfg), "I2S", "init std");

    return ESP_OK;
}

static esp_err_t speaker_play(const int16_t *mono, size_t buffer_bytes)
{
    ESP_RETURN_ON_ERROR(i2s_channel_enable(tx_handle), "I2S", "enable");

    const size_t num_samples = buffer_bytes / sizeof(int16_t);
    int16_t stereo_buf[BUFFER_SAMPLES * 2];

    for (size_t offset = 0; offset < num_samples; )
    {
        size_t chunk = num_samples - offset;
        if (chunk > BUFFER_SAMPLES)
        {
            chunk = BUFFER_SAMPLES;
        }

        for (size_t i = 0; i < chunk; i++)
        {
            int16_t sample = speaker_scale_sample(mono[offset + i]);
            stereo_buf[i * 2]     = sample;
            stereo_buf[i * 2 + 1] = sample;
        }

        size_t bytes_written = 0;
        ESP_RETURN_ON_ERROR(
            i2s_channel_write(
                tx_handle,
                stereo_buf,
                chunk * sizeof(int16_t) * 2,
                &bytes_written,
                portMAX_DELAY
            ),
            "I2S",
            "play"
        );

        offset += chunk;
    }

    return speaker_stop();
}

static esp_err_t speaker_stream_begin(void)
{
    return i2s_channel_enable(tx_handle);
}

static esp_err_t speaker_stream_write(const int16_t *mono, size_t num_samples)
{
    int16_t stereo_buf[BUFFER_SAMPLES * 2];

    for (size_t offset = 0; offset < num_samples; )
    {
        size_t chunk = num_samples - offset;
        if (chunk > BUFFER_SAMPLES)
        {
            chunk = BUFFER_SAMPLES;
        }

        for (size_t i = 0; i < chunk; i++)
        {
            int16_t sample = speaker_scale_sample(mono[offset + i]);
            stereo_buf[i * 2]     = sample;
            stereo_buf[i * 2 + 1] = sample;
        }

        size_t bytes_written = 0;
        ESP_RETURN_ON_ERROR(
            i2s_channel_write(
                tx_handle,
                stereo_buf,
                chunk * sizeof(int16_t) * 2,
                &bytes_written,
                portMAX_DELAY
            ),
            "I2S",
            "stream"
        );

        offset += chunk;
    }

    return ESP_OK;
}

static esp_err_t speaker_stream_end(void)
{
    return speaker_stop();
}

static void play_tone(void)
{
    int16_t buf[BUFFER_SAMPLES * 2];
    static float phase = 0.0f;
    const float phase_step = 2.0f * (float)M_PI * TONE_HZ / SAMPLE_RATE;

    for (int i = 0; i < BUFFER_SAMPLES; i++)
    {
        int16_t sample = (int16_t)(sinf(phase) * AMPLITUDE);
        phase += phase_step;
        if (phase >= 2.0f * (float)M_PI)
        {
            phase -= 2.0f * (float)M_PI;
        }

        buf[i * 2]     = sample;
        buf[i * 2 + 1] = sample;
    }

    size_t bytes_written = 0;
    i2s_channel_write(
        tx_handle,
        buf,
        sizeof(buf),
        &bytes_written,
        portMAX_DELAY
    );
}

#endif
