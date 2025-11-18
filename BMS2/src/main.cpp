
#include <cstdint>

#include "freertos/FreeRTOS.h"

#include "logger/t_logger.hpp"
#include "logger/q_logger.hpp"


// Task control blocks (TCBs)
static StaticTask_t g_logger_tcb = {};


// Task stacks
static StackType_t g_logger_stack[t_logger::TASK_STACK_SIZE];




extern "C" void app_main() {
	// Queue initialization
	q_logger::g_logger_queue = xQueueCreate(q_logger::QUEUE_SIZE, sizeof(q_logger::Message));


	// Task definitions
	t_logger::TLogger t_logger(t_logger::TASK_PERIOD_MS);


	// Start tasks
	TaskHandle_t logger_task_handle = xTaskCreateStaticPinnedToCore(
		&t_logger::TLogger::taskWrapper,
		t_logger::TASK_NAME,
		t_logger::TASK_STACK_SIZE,
		&t_logger,
		t_logger::TASK_PRIORITY,
		g_logger_stack,
		&g_logger_tcb,
		t_logger::TASK_CORE_ID
	);
	
}