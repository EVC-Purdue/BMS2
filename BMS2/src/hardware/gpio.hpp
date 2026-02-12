#ifndef GPIO_HPP
#define GPIO_HPP

#include "driver/gpio.h"

#define digitalWrite(pin, level)  gpio_set_level((gpio_num_t)(pin), (level))
#define OUTPUT_HIGH(pin)  digitalWrite(pin, HIGH)
#define OUTPUT_LOW(pin)   digitalWrite(pin, LOW)
#define pinMode(pin, mode)  gpio_set_direction((gpio_num_t)(pin), (mode))

#define LOW 0
#define HIGH 1
#define INPUT GPIO_MODE_INPUT
#define OUTPUT GPIO_MODE_OUTPUT


#endif // GPIO_HPP