#ifndef PTT_H
#define PTT_H

#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// Jeden pin wystarczy: przycisk miedzy GPIO a GND (pull-up wewnetrzny).
// GPIO17 zostaw wolny pod NRF24 CE.
#define PTT_GPIO            16
#define PTT_DEBOUNCE_MS     50

static SemaphoreHandle_t ptt_sem;
static volatile bool ptt_transmitting = false;

static void IRAM_ATTR ptt_gpio_isr(void *arg)
{
    (void)arg;
    BaseType_t wake = pdFALSE;
    if (ptt_sem != NULL)
    {
        xSemaphoreGiveFromISR(ptt_sem, &wake);
    }
    if (wake == pdTRUE)
    {
        portYIELD_FROM_ISR();
    }
}

static void ptt_task(void *arg)
{
    (void)arg;
    bool prev_pressed = false;
    bool click_armed = false;
    TickType_t stable_since = 0;

    while (1)
    {
        xSemaphoreTake(ptt_sem, portMAX_DELAY);

        
        while (1)
        {
            bool pressed = gpio_get_level(PTT_GPIO) == 0;
            TickType_t now = xTaskGetTickCount();

            if (pressed != prev_pressed)
            {
                stable_since = now;
                prev_pressed = pressed;
            }

            if ((now - stable_since) < pdMS_TO_TICKS(PTT_DEBOUNCE_MS))
            {
                vTaskDelay(pdMS_TO_TICKS(5));
                continue;
            }

            if (pressed)
            {
                click_armed = true;
            }
            else if (click_armed)
            {
                ptt_transmitting = !ptt_transmitting;
                click_armed = false;
            }

            break;
        }
    }
}

static esp_err_t ptt_init(void)
{
    ptt_sem = xSemaphoreCreateBinary();
    if (ptt_sem == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << PTT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), "PTT", "gpio config");

    esp_err_t isr_err = gpio_install_isr_service(0);
    if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE)
    {
        return isr_err;
    }
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(PTT_GPIO, ptt_gpio_isr, NULL), "PTT", "isr add");

    BaseType_t ok = xTaskCreate(ptt_task, "ptt", 2048, NULL, 10, NULL);
    if (ok != pdPASS)
    {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static bool ptt_is_transmitting(void)
{
    return ptt_transmitting;
}

#endif
