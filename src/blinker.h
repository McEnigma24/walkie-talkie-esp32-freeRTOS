#ifndef BLINKER_H
#define BLINKER_H

#include <stdbool.h>
#include <stdint.h>

#define BLINK_GPIO 22

#define ON  ( false )
#define OFF ( true )

typedef struct
{
    uint8_t pin;
    bool state;
}
gpio_output_t;

gpio_output_t init_gpio_output(uint8_t pin, bool state);
void gpio_output_set(gpio_output_t *wrapper, bool state);
void gpio_output_toggle(gpio_output_t *wrapper);
void gpio_output_blink(gpio_output_t *wrapper, uint32_t n, uint32_t time_on_ms, uint32_t time_off_ms);

extern gpio_output_t blinker;

#endif
