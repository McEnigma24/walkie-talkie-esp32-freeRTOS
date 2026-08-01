#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "blinker.h"
#include "speaker.h"
#include "mic.h"
#include "ptt.h"
#include "nRF.h"
#include "crypto.h"

// #define TRANSMITTER
#define RECEIVER


#if defined(TRANSMITTER) && defined(RECEIVER)
    #error "Configuration error: TRANSMITTER and RECEIVER cannot be defined at the same time."
#endif


#define MIC_DATA_TYPE int16_t
#define MIC_UNALIGNED_BUFFER_SIZE ( 50'000 )
#define MIC_BUFFER_SIZE ( (((MIC_UNALIGNED_BUFFER_SIZE / TRANSMISSION_PAYLOAD_BYTE_ALIGNMENT) + (TRANSMISSION_PAYLOAD_BYTE_ALIGNMENT - 1))) * (TRANSMISSION_PAYLOAD_BYTE_ALIGNMENT) )
#define MIC_BUFFER_BYTE_SIZE ( MIC_BUFFER_SIZE * sizeof(MIC_DATA_TYPE) )
static MIC_DATA_TYPE MIC_BUFFER[MIC_BUFFER_SIZE]; // 10'000 * sizeof(int16_t) == ~20 kB, w .bss

void app_main(void)
{
    printf("Walkie-talkie %d Hz - PTT na GPIO%d\n", SAMPLE_RATE, PTT_GPIO);

    gpio_output_t blinker = gpio_output_init(BLINK_GPIO, true);
    gpio_output_blink(&blinker, 3, 500, 500);

    ESP_ERROR_CHECK(speaker_init());
    ESP_ERROR_CHECK(nRF_init());
    ESP_ERROR_CHECK(crypto_init());

    #ifdef TRANSMITTER
        // ESP_ERROR_CHECK(mic_init());
        // ESP_ERROR_CHECK(mic_init_cont());
        // ESP_ERROR_CHECK(mic_stream_init());

        ESP_ERROR_CHECK(ptt_init());

        uint8_t tmp_single_packet_buffer[32] = {
            1, 2, 3, 4, 5, 6, 7, 8,
            1, 2, 3, 4, 5, 6, 7, 8,
            1, 2, 3, 4, 5, 6, 7, 8,
            1, 2, 3, 4, 5, 6, 7, 8
        };
    #endif

    #ifdef RECEIVER
        // ESP_ERROR_CHECK(speaker_stream_begin());
        // int mic_buffer_idx = 0;
        uint8_t tmp_single_packet_buffer[32] = { 0 };
    #endif

    int64_t TPUT_last_tick_before_full_1s = esp_timer_get_time();
    radio_packet_t tmp_single_encypted_packet;

    gpio_output_blink(&blinker, 3, 500, 500);

    /*
        #define BENCHMARK(x, numbers) \
        { \
        \
        \
        }

        int64_t next_us = esp_timer_get_time();
        int64_t next_us = esp_timer_get_time();
        int64_t next_us = esp_timer_get_time();
    */

    // xTaskCreate(cont_mic_stream_task, "mic", 4096, NULL, 5, NULL);
    // xTaskCreate(nRF_stream_task,      "nrf", 4096, NULL, 5, NULL);

    while (1)
    {
        #ifdef TRANSMITTER
        {
            if (ptt_is_transmitting())
            {
                // ptt_force_stop(); // turning to non-transmit mode

                encode_audio_packet(tmp_single_packet_buffer, &tmp_single_encypted_packet);
                nRF_send_data((uint8_t*)&tmp_single_encypted_packet, sizeof(radio_packet_t));



                // just to check
                // gpio_output_blink(&blinker, 1, 50, 50);
            }
        }
        #endif

        #ifdef RECEIVER
        {
            if (! Nrf24_dataReady(&dev))
            {
                continue;
            }

            Nrf24_getData(&dev, (uint8_t*)&tmp_single_encypted_packet);
            decode_radio_packet(&tmp_single_encypted_packet, tmp_single_packet_buffer);

            // printf("Received payload\n");
            // for(int i=0; i<PAYLOAD_DATA_BYTE_SIZE; i += 8)
            // {
            //     printf("[%d]:%d, [%d]:%d, [%d]:%d, [%d]:%d, [%d]:%d, [%d]:%d, [%d]:%d, [%d]:%d\n",
            //         i + 0, tmp_single_packet_buffer[i + 0],
            //         i + 1, tmp_single_packet_buffer[i + 1],
            //         i + 2, tmp_single_packet_buffer[i + 2],
            //         i + 3, tmp_single_packet_buffer[i + 3],
            //         i + 4, tmp_single_packet_buffer[i + 4],
            //         i + 5, tmp_single_packet_buffer[i + 5],
            //         i + 6, tmp_single_packet_buffer[i + 6],
            //         i + 7, tmp_single_packet_buffer[i + 7]
            //     );
            // }

            static uint32_t TPUT_volume = 0;
            static uint32_t TPUT_count = 0;

            TPUT_volume += 32;
            TPUT_count++;

            int64_t now_us = esp_timer_get_time();
            int64_t diff_us = ( now_us - TPUT_last_tick_before_full_1s );
            if(1'000'000 < diff_us)
            {
                // printf("TPUT_volume  %ld\n", TPUT_volume);
                // printf("TPUT_count  %ld\n", TPUT_count);
                printf("TPUT_volume per second %fB\n", TPUT_volume / ((float)diff_us / 1'000'000));
                printf("TPUT_count  per second %f\n", TPUT_count / ((float)diff_us / 1'000'000));

                TPUT_last_tick_before_full_1s = now_us;

                TPUT_volume = 0;
                TPUT_count = 0;
            }
        }
        #endif
    }

    // ESP_ERROR_CHECK(speaker_stream_end());
}
