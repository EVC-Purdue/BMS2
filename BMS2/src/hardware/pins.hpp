#ifndef HARDWARE_PINS_HPP
#define HARDWARE_PINS_HPP

#include "soc/gpio_num.h"
#include "driver/uart.h"

namespace pins
{
    namespace LTC1
    { // TOP
        constexpr size_t THERM1 = 3;
        constexpr size_t THERM2 = 2;
        constexpr size_t THERM3 = 4;
        constexpr size_t THERM4 = 5;

        constexpr size_t THERM_BAL_BOT = 2;
        constexpr size_t THERM_BAL_TOP = 3;
        constexpr size_t CURRENT = 1;
    } // namespace LTC1

    namespace LTC2
    { // BOTTOM
        constexpr size_t THERM_FET = 1;
        constexpr size_t THERM_BAL_BOT = 2;
        constexpr size_t THERM_BAL_TOP = 3;
    } // namespace LTC2

    namespace ESP
    {
        #if true // rev-1.3 == true
        constexpr gpio_num_t CONTACTOR = GPIO_NUM_13; // GPIO output
        constexpr gpio_num_t SS_SWITCH = GPIO_NUM_35; // GPIO output
        constexpr gpio_num_t PWR_EN = GPIO_NUM_2;     // GPIO output
        constexpr gpio_num_t GS0 = GPIO_NUM_47;       // GPIO output
        constexpr gpio_num_t GS1 = GPIO_NUM_48;        // GPIO output
        constexpr gpio_num_t LED = GPIO_NUM_15;       // GPIO output
        constexpr gpio_num_t CAN_S = GPIO_NUM_12;     // GPIO output
        constexpr gpio_num_t CAN_TX = GPIO_NUM_11;    // CAN
        constexpr gpio_num_t CAN_RX = GPIO_NUM_10;     // CAN
        constexpr gpio_num_t SPI_SCK = GPIO_NUM_38;   // SPI
        constexpr gpio_num_t SPI_MISO = GPIO_NUM_40;  // SPI
        constexpr gpio_num_t SPI_MOSI = GPIO_NUM_41;  // SPI
        constexpr gpio_num_t SPI_CS = GPIO_NUM_39;    // GPIO output, CS is manually set
        constexpr gpio_num_t BUZZER = GPIO_NUM_17;    // LEDC
        // constexpr uart_port_t UART_NUM = UART_NUM_0;  // Serial port (usb
        #else // rev-1.2
        constexpr gpio_num_t CONTACTOR = GPIO_NUM_33;                // GPIO output
        constexpr gpio_num_t SS_SWITCH = GPIO_NUM_16;                // GPIO output
        constexpr gpio_num_t PWR_EN = static_cast<gpio_num_t>(22);   // GPIO output
        constexpr gpio_num_t GS0 = GPIO_NUM_14;                      // GPIO output
        constexpr gpio_num_t GS1 = GPIO_NUM_13;                      // GPIO output
        constexpr gpio_num_t LED = GPIO_NUM_32;                      // GPIO output
        constexpr gpio_num_t CAN_ON = GPIO_NUM_4;                    // GPIO output
        constexpr gpio_num_t CAN_S = GPIO_NUM_17;                    // GPIO output
        constexpr gpio_num_t CAN_TX = GPIO_NUM_26;                   // CAN
        constexpr gpio_num_t CAN_RX = GPIO_NUM_27;                   // CAN
        constexpr gpio_num_t SPI_SCK = GPIO_NUM_5;                   // SPI
        constexpr gpio_num_t SPI_MISO = GPIO_NUM_19;                 // SPI
        constexpr gpio_num_t SPI_MOSI = static_cast<gpio_num_t>(23); // SPI
        constexpr gpio_num_t SPI_CS = GPIO_NUM_18;                   // GPIO output, CS is manually set
        constexpr gpio_num_t BUZZER = static_cast<gpio_num_t>(25);   // LEDC
        constexpr uart_port_t UART_NUM = UART_NUM_0;                 // Serial port (usb)
        #endif
    } // namespace ESP
} // namespace pins

#endif // HARDWARE_PINS_HPP