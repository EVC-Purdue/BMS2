
#include <cstdint>

#include "freertos/FreeRTOS.h"

#include "battery/t_battery.hpp"
#include "battery/q_battery.hpp"
#include "logger/t_logger.hpp"
#include "logger/q_logger.hpp"


// Task control blocks (TCBs)
static StaticTask_t g_battery_tcb = {};
static StaticTask_t g_logger_tcb = {};


// Task stacks
static StackType_t g_battery_stack[t_battery::TASK_STACK_SIZE];
static StackType_t g_logger_stack[t_logger::TASK_STACK_SIZE];




extern "C" void app_main() {
	// Queue initialization
	q_battery::g_battery_queue = xQueueCreate(q_battery::QUEUE_SIZE, sizeof(q_battery::Message));
	q_logger::g_logger_queue = xQueueCreate(q_logger::QUEUE_SIZE, sizeof(q_logger::Message));


	// Task definitions
	t_battery::TBattery t_battery(t_battery::TASK_PERIOD_MS);
	t_logger::TLogger t_logger(t_logger::TASK_PERIOD_MS);


	// Start tasks
	TaskHandle_t battery_task_handle = xTaskCreateStaticPinnedToCore(
		&t_battery::TBattery::taskWrapper,
		t_battery::TASK_NAME,
		t_battery::TASK_STACK_SIZE,
		&t_battery,
		t_battery::TASK_PRIORITY,
		g_battery_stack,
		&g_battery_tcb,
		t_battery::TASK_CORE_ID
	);
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
	
	while (true) {
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}