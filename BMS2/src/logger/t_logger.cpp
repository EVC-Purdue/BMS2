#include "freertos/FreeRTOS.h"

#include "logger/q_logger.hpp"


#include "logger/t_logger.hpp"



namespace t_logger {

TLogger::TLogger(uint32_t period)
	: task_base::TaskBase(period) {}

void TLogger::task() {
	// Implement logging functionality here
	q_logger::Message msg = {};

	// Read messages from the logger queue
	if (xQueueReceive(q_logger::g_logger_queue, &msg, 0) == pdTRUE) {
		std::visit([](auto&& arg) {
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, q_logger::msg::Write>) {
				// Handle Write message
			} else if constexpr (std::is_same_v<T, q_logger::msg::Read>) {
				// Handle Read message
			} else if constexpr (std::is_same_v<T, q_logger::msg::Flush>) {
				// Handle Flush message
			}
		}, msg);
	}
	
}


} // namespace t_logger
