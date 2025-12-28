#ifndef HARDWARE_HPP
#define HARDWARE_HPP

#include <cstdint>


namespace hardware {

constexpr int MAX_SPI_TRANSFER_SZ = 4096;
const uint8_t SPI_MODE = 0;
const int SPI_CLOCK_SPEED_HZ = 500'000; // 500 kHz
const uint32_t LEDC_DUTY = 512; // 50% volume for 10-bit resolution

// Sets up GPIO pins as outputs
void configure_gpio_output();

// Configure SPI peripheral
void configure_spi(spi_device_handle_t* spi_handle);

// Configure LEDC (buzzer) peripheral
void configure_ledc();

// Calls all other configure functions
void configure(spi_device_handle_t* spi_handle);

// Set the initial states of output pins
void setup_initial_gpio_states();

// Play a tone on the buzzer at the specified frequency (Hz) for the specified
// duration (ms) (blocking)
void play_buzzer_tone(uint32_t frequency_hz, uint32_t duration_ms);
} // namespace hardware

#endif // HARDWARE_HPP