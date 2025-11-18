#include "freertos/FreeRTOS.h"

#include "logger/q_logger.hpp"
#include "util/overloaded.hpp"


#include "logger/t_logger.hpp"



namespace t_logger {

TLogger::TLogger(uint32_t period)
	: task_base::TaskBase(period) {}

void TLogger::task() {
	// Implement logging functionality here
	q_logger::Message msg = {};

	// Read messages from the logger queue
	if (xQueueReceive(q_logger::g_logger_queue, &msg, 0) == pdTRUE) {
		std::visit(util::OverloadedVisit{
			[](const q_logger::msg::Write& w) {
				// handle write
			},
			[](const q_logger::msg::Read& r) {
				// handle read
			},
			[](const q_logger::msg::Flush& f) {
				// handle flush
			}
		}, msg);
	}
	
}


} // namespace t_logger
