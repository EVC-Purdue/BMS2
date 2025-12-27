#ifndef HARDWARE_HPP
#define HARDWARE_HPP

#include <cstdint>


namespace hardware {

constexpr int MAX_SPI_TRANSFER_SZ = 4096;
const uint8_t SPI_MODE = 0;
const int SPI_CLOCK_SPEED_HZ = 500'000; // 500 kHz

// Sets up GPIO pins as outputs
void configure_gpio_output();

// Setup SPI peripheral
void configure_spi(spi_device_handle_t* spi_handle);

// Calls all other configure functions
void configure(spi_device_handle_t* spi_handle);

// Set the initial states of output pins
void setup_initial_gpio_states();
} // namespace hardware

#endif // HARDWARE_HPP