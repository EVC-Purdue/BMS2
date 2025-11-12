#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <cstdint>

#include "freertos/FreeRTOS.h"


namespace logger {

constexpr uint32_t TASK_STACK_SIZE = 4096;
constexpr UBaseType_t TASK_PRIORITY = 5;
constexpr BaseType_t TASK_CORE_ID = 0;
constexpr const char* TASK_NAME = "LoggingTask";

	

void task_function(void* pvParameters);



} // namespace logger





#endif // LOGGER_HPP