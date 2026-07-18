#include <stdio.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "blinker.h"
#include "speaker.h"
#include "mic.h"
#include "ptt.h"

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

#define nRF_CHECK_ERR(...) esp_err_t ret = __VA_ARGS__; \
if (ret != ESP_OK) { \
    printf("Fatal error %s\n", #__VA_ARGS__); assert(false); \
} \

void app_main(void)
{
    printf("Walkie-talkie %d Hz - PTT na GPIO%d\n", SAMPLE_RATE, PTT_GPIO);

    gpio_output_t blinker = gpio_output_init(BLINK_GPIO, true);

    ESP_ERROR_CHECK(mic_init());
    ESP_ERROR_CHECK(speaker_init());
    ESP_ERROR_CHECK(ptt_init());


    // #define TRANSMITTER
    #define RECEIVER



    NRF24_t dev;
    nRF_CHECK_ERR(Nrf24_init(&dev));
    // nRF_CHECK_ERR(Nrf24_enableNoAckFeature(&dev));
	const uint8_t payload_length = 32;
	const uint8_t rf_channel = 90; // CONFIG_RADIO_CHANNEL
	nRF_CHECK_ERR(Nrf24_config(&dev, rf_channel, payload_length));
    nRF_CHECK_ERR(Nrf24_setTADDR(&dev, (uint8_t*)"WALK1"));
    nRF_CHECK_ERR(Nrf24_setRADDR(&dev, (uint8_t*)"WALK1"));

	// // Set destination address using 5 characters
	// esp_err_t ret = Nrf24_setTADDR(&dev, (uint8_t *)"FGHIJ");
	// if (ret != ESP_OK) {
	// 	ESP_LOGE(pcTaskGetName(NULL), "nrf24l01 not installed");
	// }

    uint8_t buf[32];

    while(1)
    {
        #ifdef TRANSMITTER
        {
            if (ptt_is_transmitting())
            {
                // TX:
                memset(buf, 0xda, sizeof(buf));

                // Nrf24_send(&dev, buf);
                Nrf24_sendNoAck(&dev, buf);
                bool status = Nrf24_isSend(&dev, 1000);
            }
        }

        #ifdef RECEIVER
        {
            // RX:
            if (Nrf24_dataReady(&dev))
            {
                Nrf24_getData(&dev, buf);

                gpio_output_toggle(&blinker);
                vTaskDelay(pdMS_TO_TICKS(10));
                gpio_output_toggle(&blinker);

                printf("Payload data [0]:0x%02x [1]:0x%02x [2]:0x%02x [3]:0x%02x \n", buf[0], buf[1], buf[2], buf[3]);
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
