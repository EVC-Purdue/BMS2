#ifndef T_LOGGER_HPP
#define T_LOGGER_HPP

#include <cstdint>

#include "freertos/FreeRTOS.h"

#include "task/task_base.hpp"


namespace t_logger {

constexpr uint32_t TASK_PERIOD_MS = 1000;
constexpr uint32_t TASK_STACK_SIZE = 4096;
constexpr UBaseType_t TASK_PRIORITY = 5;
constexpr BaseType_t TASK_CORE_ID = 0;
constexpr const char* TASK_NAME = "LoggingTask";



class TLogger : public task_base::TaskBase {
	private:
		// Private members for logging functionality

	public:
		TLogger(uint32_t period);

		void task() override;
};



} // namespace t_logger





#endif // T_LOGGER_HPP