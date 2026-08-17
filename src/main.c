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
#include "streams.h"
#include "event_group.h"

// #define TRANSMITTER
#define RECEIVER


#if defined(TRANSMITTER) && defined(RECEIVER)
    #error "Configuration error: TRANSMITTER and RECEIVER cannot be defined at the same time."
#endif


#define MIC_DATA_TYPE int16_t
#define MIC_UNALIGNED_BUFFER_SIZE ( 50'000 )
#define MIC_BUFFER_SIZE ( (((MIC_UNALIGNED_BUFFER_SIZE / nRF_PAYLOAD_BYTE_ALIGNMENT) + (nRF_PAYLOAD_BYTE_ALIGNMENT - 1))) * (nRF_PAYLOAD_BYTE_ALIGNMENT) )
#define MIC_BUFFER_BYTE_SIZE ( MIC_BUFFER_SIZE * sizeof(MIC_DATA_TYPE) )
static MIC_DATA_TYPE MIC_BUFFER[MIC_BUFFER_SIZE]; // 10'000 * sizeof(int16_t) == ~20 kB, w .bss



// void TASK_cont_mic_stream()



void app_main(void)
{
    printf("Walkie-talkie %d Hz - PTT na GPIO%d\n", SAMPLE_RATE, PTT_GPIO);

    // INITS //
    {
        blinker = init_gpio_output(BLINK_GPIO, true);
        gpio_output_blink(&blinker, 3, 500, 500);

            ESP_ERROR_CHECK(init_ptt());

            ESP_ERROR_CHECK(init_event_group());
            ESP_ERROR_CHECK(init_mic_to_en_crypto_stream());
            ESP_ERROR_CHECK(init_en_crypto_to_nRF_transmit_stream());
            ESP_ERROR_CHECK(init_nRF_receive_to_de_crypto_stream());
            ESP_ERROR_CHECK(init_de_crypto_to_speaker_stream());

            ESP_ERROR_CHECK(init_mic_cont());
            ESP_ERROR_CHECK(init_speaker());

            ESP_ERROR_CHECK(init_nRF());
            ESP_ERROR_CHECK(init_crypto());

        gpio_output_blink(&blinker, 3, 500, 500);
    }

    // TASKS //
    {
        xTaskCreate(TASK_cont_mic_stream,   "mic",          4096, NULL, 5, NULL);
        xTaskCreate(TASK_crypto_en_crypto,  "en_crypto",    4096, NULL, 5, NULL);
        xTaskCreate(TASK_nRF_send,          "nRF_send",     4096, NULL, 5, NULL);

        xTaskCreate(TASK_nRF_receive,       "nRF_receive",  4096, NULL, 5, NULL);
        xTaskCreate(TASK_crypto_de_crypto,  "de_crypto",    4096, NULL, 5, NULL);
        xTaskCreate(TASK_speaker,           "speaker",      4096, NULL, 5, NULL);
    }

    /*
    while (1)
    {
        #ifdef TRANSMITTER
        {
            if (ptt_is_transmitting())
            {
                // ptt_force_stop(); // turning to non-transmit mode

                en_crypto_audio_packet(tmp_single_packet_buffer, &tmp_single_encypted_packet);
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
            // for(int i=0; i<CRYPTO_PAYLOAD_BYTE_SIZE; i += 8)
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
    */
}
