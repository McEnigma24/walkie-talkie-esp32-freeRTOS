#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "blinker.h"
#include "speaker.h"
#include "mic.h"
#include "ptt.h"
#include "nRF.h"


// static int16_t tx_chunk[BUFFER_SAMPLES];

// static void run_transmit_mode(gpio_output_t *blinker)
// {
//     printf("Tryb TX (emisja) - mow do mikrofonu\n");
//     gpio_output_toggle(blinker);
//     ESP_ERROR_CHECK(speaker_stream_begin());

//     while (ptt_is_transmitting())
//     {
//         ESP_ERROR_CHECK(mic_read_samples(tx_chunk, BUFFER_SAMPLES));
//         ESP_ERROR_CHECK(speaker_stream_write(tx_chunk, BUFFER_SAMPLES));
//     }

//     ESP_ERROR_CHECK(speaker_stream_end());
//     gpio_output_toggle(blinker);
//     printf("Tryb RX (odbior)\n");
// }






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

    #ifdef TRANSMITTER
        ESP_ERROR_CHECK(mic_init());
        ESP_ERROR_CHECK(ptt_init());
    #endif

    #ifdef RECEIVER
        ESP_ERROR_CHECK(speaker_stream_begin());
        int mic_buffer_idx = 0;
    #endif

    // uint8_t tmp_single_packet_buffer[32];
    // int64_t TPUT_last_tick_before_full_1s = esp_timer_get_time();

    while (1)
    {
        #ifdef TRANSMITTER
        {
            if (ptt_is_transmitting())
            {
                // ptt_force_stop(); // turning to non-transmit mode
                nRF_send_data((uint8_t*)tmp_single_packet_buffer, 32);

                // gpio_output_blink(&blinker, 1, 50, 20);
            }
        }
        #endif

        #ifdef RECEIVER
        {
            if (Nrf24_dataReady(&dev))
            {
                Nrf24_getData(&dev, tmp_single_packet_buffer);
            }



            // static uint32_t TPUT_volume = 0;
            // static uint32_t TPUT_count = 0;

            // TPUT_volume += 32;
            // TPUT_count++;

            // int64_t now_us = esp_timer_get_time();
            // int64_t diff_us = ( now_us - TPUT_last_tick_before_full_1s );
            // if(1'000'000 < diff_us)
            // {
            //     // printf("TPUT_volume  %ld\n", TPUT_volume);
            //     // printf("TPUT_count  %ld\n", TPUT_count);
            //     printf("TPUT_volume per second %fB\n", TPUT_volume / ((float)diff_us / 1'000'000));
            //     printf("TPUT_count  per second %f\n", TPUT_count / ((float)diff_us / 1'000'000));

            //     TPUT_last_tick_before_full_1s = now_us;

            //     TPUT_volume = 0;
            //     TPUT_count = 0;
            // }

            // transmittig all stored data //
            // nRF_send_data((uint8_t*)MIC_BUFFER, MIC_BUFFER_BYTE_SIZE);

            // uint8_t ending_preamble[32];
            // for(int i=0; i<32; i+=2)
            // {
            //     ending_preamble[i] = 0;
            //     ending_preamble[i + 1] = 0xff;
            // }
            // nRF_send_data((uint8_t*)ending_preamble, 32);

            // gpio_output_toggle(&blinker);
            // mic_read_samples(MIC_BUFFER, MIC_BUFFER_SIZE);
            // gpio_output_toggle(&blinker);

            /*
                    // TX:
                    for(int i=0; i<32; i++)
                    {
                        buf[i] = i;
                    }

                    // Nrf24_send(&dev, buf);
                    Nrf24_sendNoAck(&dev, buf);
                    bool status = Nrf24_isSend(&dev, 1000);
                    printf("Sending data - %d \n", status);

                    vTaskDelay(pdMS_TO_TICKS(20));
                */

            // play_tone();

            // // RX:
            // if (Nrf24_dataReady(&dev))
            // {
            //     uint8_t pkt[32];

            //     Nrf24_getData(&dev, pkt); // odbierz do bufora tymczasowego, NIE prosto do MIC_BUFFER

            //     bool pattern_holds = true;
            //     for(int i=0; i<32; i+=2)
            //     {
            //         if(! ((pkt[i] == 0) && (pkt[(i + 1)] == 0xff)))
            //         {
            //             pattern_holds = false;
            //             break;
            //         }
            //     }

            //     if(pattern_holds)
            //     {
            //         printf("END MARKER detected\n");
            //         speaker_stream_write(MIC_BUFFER, mic_buffer_idx); // graj tyle, ile faktycznie odebrano (bez wartownika)
            //         mic_buffer_idx = 0;                               // gotowi na kolejna wiadomosc
            //     }
            //     else if(mic_buffer_idx + TRANSMISSION_PAYLOAD_LENGTH / sizeof(MIC_DATA_TYPE) <= MIC_BUFFER_SIZE)
            //     {
            //         memcpy(&MIC_BUFFER[mic_buffer_idx], pkt, TRANSMISSION_PAYLOAD_LENGTH);   // dopisz audio
            //         mic_buffer_idx += TRANSMISSION_PAYLOAD_LENGTH / sizeof(MIC_DATA_TYPE);   // 16
            //     }
            //     // else: bufor pelny -> pomijaj audio az do wartownika
            // }
        }
        #endif
    }

    ESP_ERROR_CHECK(speaker_stream_end());


    // printf("\n");
    // for(int i=0; i<32; i++)
    // {
    //     if((i % 4) == 0) printf("Payload data ");

    //     char sting_bufor[12];
    //     snprintf(sting_bufor, sizeof(sting_bufor), "%d", i);
    //     printf(" [%s]:0x%02x,", sting_bufor, buf[i]);

    //     if((i % 4) == 3) printf("\n");
    // }

    // while (1)
    // {
    //     if (ptt_is_transmitting())
    //     {
    //         run_transmit_mode(&blinker);
    //     }

    //     vTaskDelay(pdMS_TO_TICKS(50));
    // }
}
