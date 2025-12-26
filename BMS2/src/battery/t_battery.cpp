#include "freertos/FreeRTOS.h"

#include "battery/q_battery.hpp"
#include "util/overloaded.hpp"


#include "battery/t_battery.hpp"



namespace t_battery {

TBattery::TBattery(uint32_t period)
	: task_base::TaskBase(period) {}

void TBattery::task() {
	// Implement logging functionality here
	q_battery::Message msg = {};

	// Read messages from the battery queue
	if (xQueueReceive(q_battery::g_battery_queue, &msg, 0) == pdTRUE) {
		std::visit(util::OverloadedVisit{
			[](const q_battery::msg::Write& w) {
				// handle write
			},
			[](const q_battery::msg::Read& r) {
				// handle read
			},
			[](const q_battery::msg::Flush& f) {
				// handle flush
			}
		}, msg);
	}
	
}


} // namespace t_battery
