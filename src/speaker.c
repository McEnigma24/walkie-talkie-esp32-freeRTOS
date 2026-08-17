#include "speaker.h"

#include <math.h>
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"

#include "crypto.h"
#include "streams.h"
#include "event_group.h"

static const char *TAG = "SPEAKER";

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

esp_err_t init_speaker(void)
{
    // I2S0 jest na ESP32 zajete przez ADC continuous (mikrofon), wiec bierzemy I2S1 wprost
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 8;      // wiecej buforow = wiekszy zapas, ale wieksza latencja
    chan_cfg.dma_frame_num = 256;   // wiekszy bufor = mniej wybudzen, ale wieksza latencja
    chan_cfg.auto_clear = true;     // przy underrunie graj cisze zamiast powtarzac ostatni bufor

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

static esp_err_t speaker_write_silence(int buffers)
{
    int16_t silence[BUFFER_SAMPLES * 2] = {0};
    size_t bytes_written = 0;

    for (int i = 0; i < buffers; i++)
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

    return ESP_OK;
}

esp_err_t speaker_stream_begin(void)
{
    ESP_RETURN_ON_ERROR(i2s_channel_enable(tx_handle), "I2S", "enable");

    // Nadajnik i odbiornik taktuja sie wlasnymi kwarcami i pakiety przychodza
    // nierowno, wiec nabijamy zapas ciszy w DMA. Bez niego kazde spoznienie
    // dostawy natychmiast slychac jako dziure w dzwieku.
    return speaker_write_silence(SPEAKER_PREBUFFER_BUFFERS);
}

esp_err_t speaker_stream_end(void)
{
    ESP_RETURN_ON_ERROR(speaker_write_silence(SILENCE_FLUSH_BUFFERS), "I2S", "disable");
    return i2s_channel_disable(tx_handle);
}

esp_err_t speaker_stream_write(const int16_t *mono, size_t num_samples)
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

esp_err_t speaker_play(const int16_t *mono, size_t buffer_bytes)
{
    ESP_RETURN_ON_ERROR(speaker_stream_begin(), "I2S", "enable");

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

    return speaker_stream_end();
}

void play_tone(void)
{
    ESP_ERROR_CHECK(speaker_stream_begin());

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

    ESP_ERROR_CHECK(speaker_stream_end());
}

void TASK_speaker(void *arg)
{
    (void)arg;
    int16_t RAW_AUDIO_OUTPUT[CRYPTO_PAYLOAD_BYTE_SIZE / sizeof(int16_t)];

    while(1)
    {
        blockWaitForReceiveMode(); // blocking

        speaker_stream_begin(); // speaker ON

        while(isReceiveModeStillActive())
        {
            size_t got = xStreamBufferReceive(
                de_crypto_to_speaker_stream,
                RAW_AUDIO_OUTPUT,
                CRYPTO_PAYLOAD_BYTE_SIZE,
                common_timeout
            );

            if(got > 0)
            {
                // we got something //

                if(got != CRYPTO_PAYLOAD_BYTE_SIZE)
                {
                    ESP_LOGE(TAG, "Received from StreamBuffer with incorrect size -> %u != 30", (unsigned)got);
                    continue;
                }

                speaker_stream_write(RAW_AUDIO_OUTPUT, CRYPTO_PAYLOAD_BYTE_SIZE / sizeof(int16_t));
            }
        }

        speaker_stream_end(); // speaker OFF
    }
}


