#ifndef on_err_HPP
#define on_err_HPP

#include <cstdint>
#include "hardware/gpio.hpp"
#include "hardware/pins.hpp"
#include "battery/q_battery.hpp"

namespace on_err
{
    // Function to call when a critical error occurs.
    static void handle_critical_error(const char *error_message)
    {
        digitalWrite(pins::ESP::CONTACTOR, LOW);

        // Send a message to the logger to log the critical error
        q_battery::msg::GenerateLogLine log_msg = {};
        xQueueSend(q_battery::g_battery_queue, &log_msg, portMAX_DELAY);

        // Set to IDLE 
        q_battery::msg::SetMode set_mode_msg = {};
        set_mode_msg.mode = modes::Mode::IDLE;
        xQueueSend(q_battery::g_battery_queue, &set_mode_msg, portMAX_DELAY);
    }
}

#endif // on_err_HPP