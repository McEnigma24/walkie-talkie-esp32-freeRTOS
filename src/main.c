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
#include "mirf.h"

static int16_t tx_chunk[BUFFER_SAMPLES];

static void run_transmit_mode(gpio_output_t *blinker)
{
    printf("Tryb TX (emisja) - mow do mikrofonu\n");
    gpio_output_toggle(blinker);
    ESP_ERROR_CHECK(speaker_stream_begin());

    while (ptt_is_transmitting())
    {
        ESP_ERROR_CHECK(mic_read_samples(tx_chunk, BUFFER_SAMPLES));
        ESP_ERROR_CHECK(speaker_stream_write(tx_chunk, BUFFER_SAMPLES));
    }

    ESP_ERROR_CHECK(speaker_stream_end());
    gpio_output_toggle(blinker);
    printf("Tryb RX (odbior)\n");
}

#define nRF_CHECK_ERR(call) { \
    esp_err_t ret = (call); \
    if (ret != ESP_OK) { \
        printf("Fatal error %s\n", #call); \
        assert(false); \
    } \
}





// #define TRANSMITTER
#define RECEIVER



void app_main(void)
{
    printf("Walkie-talkie %d Hz - PTT na GPIO%d\n", SAMPLE_RATE, PTT_GPIO);

    gpio_output_t blinker = gpio_output_init(BLINK_GPIO, true);
    gpio_output_blink(&blinker, 3, 500, 500);

    #ifdef TRANSMITTER
        ESP_ERROR_CHECK(mic_init());
        ESP_ERROR_CHECK(speaker_init());
        ESP_ERROR_CHECK(ptt_init());
    #endif

    // nRF //
        NRF24_t dev;
        Nrf24_init(&dev);                                  // zwraca void
        Nrf24_enableNoAckFeature(&dev);
        const uint8_t payload_length = 32;
        const uint8_t rf_channel = 90;                     // CONFIG_RADIO_CHANNEL
        Nrf24_config(&dev, rf_channel, payload_length);    // zwraca void
        nRF_CHECK_ERR(Nrf24_setTADDR(&dev, (uint8_t*)"WALK1"));
        nRF_CHECK_ERR(Nrf24_setRADDR(&dev, (uint8_t*)"WALK1"));

        uint8_t buf[32];
    // ~nRF //

    while (1)
    {
        #ifdef TRANSMITTER
        {
            if (ptt_is_transmitting())
            {
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
            }
        }
        #endif

        #ifdef RECEIVER
        {
            // RX:
            if (Nrf24_dataReady(&dev))
            {
                Nrf24_getData(&dev, buf);

                gpio_output_blink(&blinker, 1, 50, 50);

                printf("\n");
                for(int i=0; i<32; i++)
                {
                    if((i % 4) == 0) printf("Payload data ");

                    char sting_bufor[12];
                    snprintf(sting_bufor, sizeof(sting_bufor), "%d", i);
                    printf(" [%s]:0x%02x,", sting_bufor, buf[i]);

                    if((i % 4) == 3) printf("\n");
                }
            }
        }
        #endif
    }



    // while (1)
    // {
    //     if (ptt_is_transmitting())
    //     {
    //         run_transmit_mode(&blinker);
    //     }

    //     vTaskDelay(pdMS_TO_TICKS(50));
    // }
}
