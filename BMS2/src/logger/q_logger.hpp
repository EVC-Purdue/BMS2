#ifndef Q_LOGGER_HPP
#define Q_LOGGER_HPP

#include <cstddef>
#include <variant>

#include "freertos/FreeRTOS.h"

#include "battery/battery.hpp"



namespace q_logger {

extern QueueHandle_t g_logger_queue;

constexpr UBaseType_t QUEUE_SIZE = 10;

namespace msg {
    // Not exactly sure what this struct should look like, but I think it should be data
    // types and TLogger will be responsible for converting to a string/log format
    struct LogLine {
        int64_t timestamp;
        uint32_t voltages[battery::IC_COUNT * battery::CELL_COUNT_PER_IC];
        battery::TempData temps;
        float current;
        uint32_t faults;
    };

    struct ReadStart {
        // No additional data needed for read start
    };

    struct ReadEnd {
        // No additional data needed for read end
    };

    struct Flush {
        // No additional data needed for flush
    };

    struct SetDeleteLog {
        bool delete_log;
    };
} // namespace msg

using Message = std::variant<
    msg::LogLine,
    msg::ReadStart,
    msg::ReadEnd,
    msg::Flush,
    msg::SetDeleteLog
>;


} // namespace q_logger

#endif // Q_LOGGER_HPP