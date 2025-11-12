
#include <cstdint>

#include "freertos/FreeRTOS.h"

#include "logger/logger.hpp"


// Task control blocks (TCBs)
static StaticTask_t g_logger_tcb = {};


// Task stacks
static StackType_t g_logger_stack[logger::TASK_STACK_SIZE];





extern "C" void app_main() {
	TaskHandle_t logger_task_handle = xTaskCreateStaticPinnedToCore(
		logger::task_function,
		logger::TASK_NAME,
		logger::TASK_STACK_SIZE,
		nullptr,
		logger::TASK_PRIORITY,
		g_logger_stack,
		&g_logger_tcb,
		logger::TASK_CORE_ID
	);
	
}