#ifndef BLINKER_H
#define BLINKER_H

#include "driver/gpio.h"
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

#endif