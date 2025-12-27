#ifndef HARDWARE_PINS_HPP
#define HARDWARE_PINS_HPP

#include "soc/gpio_num.h"


namespace pins {
    namespace LTC {
        constexpr size_t THERM1 = 2;
        constexpr size_t THERM2 = 3;
        constexpr size_t THERM3 = 4;
        constexpr size_t THERM4 = 5;
        constexpr size_t THERM_FET = 1;
        constexpr size_t THERM_BAL_BOT = 2;
        constexpr size_t THERM_BAL_TOP = 3;
        constexpr size_t CURRENT = 1;
    } // namespace LTC

    namespace ESP {
        constexpr gpio_num_t CONTACTOR = GPIO_NUM_16; // GPIO output
        constexpr gpio_num_t SS_SWITCH = GPIO_NUM_36; // GPIO output
        constexpr gpio_num_t PWR_EN = GPIO_NUM_2;     // GPIO output
        constexpr gpio_num_t BUZZER = GPIO_NUM_17;    // GPIO output
        constexpr gpio_num_t GS0 = GPIO_NUM_21;       // GPIO output
        constexpr gpio_num_t GS1 = GPIO_NUM_9;        // GPIO output
        constexpr gpio_num_t LED = GPIO_NUM_15;       // GPIO output
        constexpr gpio_num_t CAN_ON = GPIO_NUM_35;    // GPIO output
        constexpr gpio_num_t CAN_S = GPIO_NUM_37;     // GPIO output
        constexpr gpio_num_t SPI_CS = GPIO_NUM_39;    // GPIO output, CS is manually set
        constexpr gpio_num_t CAN_TX = GPIO_NUM_18;    // CAN
        constexpr gpio_num_t CAN_RX = GPIO_NUM_8;     // CAN
        constexpr gpio_num_t SPI_SCK = GPIO_NUM_38;   // SPI
        constexpr gpio_num_t SPI_MISO = GPIO_NUM_40;  // SPI
        constexpr gpio_num_t SPI_MOSI = GPIO_NUM_41;  // SPI
    } // namespace ESP
} // namespace pins


#endif // HARDWARE_PINS_HPP