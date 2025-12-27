
#include "driver/gpio.h"

#include "hardware/pins.hpp"

#include "hardware/hardware.hpp"

namespace hardware {

void configure_gpio_output() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pins::ESP::CONTACTOR) |
                        (1ULL << pins::ESP::SS_SWITCH) |
                        (1ULL << pins::ESP::PWR_EN) |
                        (1ULL << pins::ESP::BUZZER) |
                        (1ULL << pins::ESP::GS0) |
                        (1ULL << pins::ESP::GS1) |
                        (1ULL << pins::ESP::LED) |
                        (1ULL << pins::ESP::CAN_ON) |
                        (1ULL << pins::ESP::CAN_S),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

void configure() {
    configure_gpio_output();
}

void setup_initial_gpio_states() {
    gpio_set_level(pins::ESP::PWR_EN, 1);
    gpio_set_level(pins::ESP::LED, 1);
    gpio_set_level(pins::ESP::SS_SWITCH, 0);
    gpio_set_level(pins::ESP::CONTACTOR, 1);
    gpio_set_level(pins::ESP::GS0, 1);
    gpio_set_level(pins::ESP::GS1, 1);
    gpio_set_level(pins::ESP::CAN_S, 0);
    gpio_set_level(pins::ESP::CAN_ON, 1);
}

} // namespace hardware