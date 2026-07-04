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

void app_main(void)
{
    printf("Walkie-talkie %d Hz - PTT na GPIO%d\n", SAMPLE_RATE, PTT_GPIO);

    gpio_output_t blinker = gpio_output_init(BLINK_GPIO, true);

    ESP_ERROR_CHECK(mic_init());
    ESP_ERROR_CHECK(speaker_init());
    ESP_ERROR_CHECK(ptt_init());
    printf("Mikrofon OK, I2S OK, PTT OK\n");
    printf("Kliknij PTT (wcisnij i puszcz) aby wlaczyc/wylaczyc emisje\n");

    while (1)
    {
        if (ptt_is_transmitting())
        {
            run_transmit_mode(&blinker);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
