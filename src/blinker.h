#ifndef BLINKER_H
#define BLINKER_H

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define BLINK_GPIO 22

typedef struct
{
    uint8_t pin;
    bool state;
} gpio_output_t;

static gpio_output_t gpio_output_init(uint8_t pin, bool state)
{
    gpio_output_t wrapper = {
        .pin = pin,
        .state = state,
    };

    gpio_reset_pin(wrapper.pin);
    gpio_set_direction(wrapper.pin, GPIO_MODE_OUTPUT);
    gpio_set_level(wrapper.pin, wrapper.state);
    return wrapper;
}

static void gpio_output_set(gpio_output_t *wrapper, bool on)
{
    wrapper->state = on;
    gpio_set_level(wrapper->pin, on);
}

static void gpio_output_toggle(gpio_output_t *wrapper)
{
    wrapper->state = !wrapper->state;
    gpio_set_level(wrapper->pin, wrapper->state);
}

static void gpio_output_blink(gpio_output_t *wrapper, uint32_t n, uint32_t time_on_ms, uint32_t time_off_ms)
{
    if (time_off_ms == ((uint32_t)-1))
        time_off_ms = time_on_ms;

    for (uint32_t i = 0; i < n; i++)
    {
        gpio_output_toggle(wrapper);
        vTaskDelay(pdMS_TO_TICKS(time_on_ms));
        gpio_output_toggle(wrapper);
        vTaskDelay(pdMS_TO_TICKS(time_off_ms));
    }
}

#endif