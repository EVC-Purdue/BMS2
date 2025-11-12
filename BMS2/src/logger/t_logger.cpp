#include "freertos/FreeRTOS.h"


#include "logger/t_logger.hpp"


namespace t_logger {

TLogger::TLogger(uint32_t period)
	: task_base::TaskBase(period) {}

void TLogger::task() {
	// Implement logging functionality here
}


} // namespace t_logger
