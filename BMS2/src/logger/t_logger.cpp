#include "freertos/FreeRTOS.h"

#include "logger/q_logger.hpp"
#include "util/overloaded.hpp"


#include "logger/t_logger.hpp"



namespace t_logger {

TLogger::TLogger(uint32_t period)
	: task_base::TaskBase(period) {}

void TLogger::task() {

    // Read and process all messages from the logger queue
	q_logger::Message msg = {};
    while (xQueueReceive(q_logger::g_logger_queue, &msg, 0) == pdTRUE) {
        std::visit(util::OverloadedVisit {
            [this](const q_logger::msg::LogLine& log_line) {
                // Handle log line
            },
            [this](const q_logger::msg::ReadStart& read_start) {
                // Handle read start
            },
            [this](const q_logger::msg::ReadEnd& read_end) {
                // Handle read end
            },
            [this](const q_logger::msg::Flush& flush) {
                // Handle flush
            }
        }, msg);
    }
	
}


} // namespace t_logger
