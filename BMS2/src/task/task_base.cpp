#include "task/task_base.hpp"

#include "freertos/FreeRTOS.h"


namespace task_base {

TaskBase::TaskBase(uint32_t period)
	: period_ms(period) {}


void TaskBase::taskWrapper(void* self_ptr) {
	TaskBase* task_instance = static_cast<TaskBase*>(self_ptr);
	
	TickType_t lastWakeTime = xTaskGetTickCount();

	while (true) {
		task_instance->task();
		vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(task_instance->period_ms));
	}
}


} // namespace task_base
