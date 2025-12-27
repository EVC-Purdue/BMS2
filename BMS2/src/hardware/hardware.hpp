#ifndef HARDWARE_HPP
#define HARDWARE_HPP


namespace hardware {

// Sets up GPIO pins as outputs
void configure_gpio_output();

// Calls all other configure functions
void configure();

// Set the initial states of output pins
void setup_initial_gpio_states();
} // namespace hardware

#endif // HARDWARE_HPP