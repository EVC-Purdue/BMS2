#include "freertos/FreeRTOS.h"

#include "battery/q_battery.hpp"
#include "util/overloaded.hpp"


#include "battery/t_battery.hpp"



namespace t_battery {

void PersistentFaults::formatFaults(uint32_t& out) {
	out |= static_cast<uint32_t>(cell_undervoltage) << 0;
	out |= static_cast<uint32_t>(cell_overvoltage) << 1;
	out |= static_cast<uint32_t>(battery_undervoltage) << 2;
	out |= static_cast<uint32_t>(battery_overvoltage) << 3;
	out |= static_cast<uint32_t>(battery_voltage_imbalance) << 4;
	out |= static_cast<uint32_t>(overcurrent) << 5;
	out |= static_cast<uint32_t>(undercurrent) << 6;
	for (size_t i = 0; i < THERM_COUNT; ++i) {
		out |= static_cast<uint32_t>(temps[i]) << (7 + i);
	}
}

void LiveFaults::formatFaults(uint32_t& out) {
	size_t offset = PersistentFaults::FAULT_COUNT;
	// No live faults to format
}

void WarningFaults::formatFaults(uint32_t& out) {
	size_t offset = PersistentFaults::FAULT_COUNT + LiveFaults::FAULT_COUNT;
	out |= static_cast<uint32_t>(overpower) << offset;
	out |= static_cast<uint32_t>(any_bypassed) << (offset + 1);
	out |= static_cast<uint32_t>(temps_imbalance) << (offset + 2);
}



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
