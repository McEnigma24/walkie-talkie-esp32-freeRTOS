#include "blinker.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

gpio_output_t blinker;

// #define ON  ( true )
// #define OFF ( false )

gpio_output_t init_gpio_output(uint8_t pin, bool state)
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

void gpio_output_set(gpio_output_t *wrapper, bool on)
{
    wrapper->state = on;
    gpio_set_level(wrapper->pin, on);
}

void gpio_output_toggle(gpio_output_t *wrapper)
{
    wrapper->state = !wrapper->state;
    gpio_set_level(wrapper->pin, wrapper->state);
}

void gpio_output_blink(gpio_output_t *wrapper, uint32_t n, uint32_t time_on_ms, uint32_t time_off_ms)
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
