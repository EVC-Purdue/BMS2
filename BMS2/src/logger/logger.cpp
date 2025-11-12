#include "logger/logger.hpp"


namespace logger {

TLogger::TLogger(uint32_t period)
	: task_base::TaskBase(period) {}

void TLogger::task() {
	// Implement logging functionality here
}


} // namespace logger
