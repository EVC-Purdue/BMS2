#ifndef Q_BATTERY_HPP
#define Q_BATTERY_HPP

#include <variant>

#include "freertos/FreeRTOS.h"

#include "battery/parameters.hpp"
#include "battery/faults.hpp"
#include "battery/modes.hpp"



namespace q_battery {

extern QueueHandle_t g_battery_queue;

constexpr UBaseType_t QUEUE_SIZE = 10;

namespace msg {
    struct SetMode {
        modes::Mode mode;
    };
} // namespace msg

using Message = std::variant<
    msg::SetMode,
    params::msg::Message,
    faults::msg::ClearFault
>;


} // namespace q_battery

#endif // Q_BATTERY_HPP