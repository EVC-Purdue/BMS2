#include "freertos/FreeRTOS.h"

#include "battery/q_battery.hpp"
#include "battery/faults.hpp"
#include "util/overloaded.hpp"
#include "util/cmp.hpp"


#include "battery/t_battery.hpp"



namespace t_battery {

TBattery::TBattery(uint32_t period)
	: task_base::TaskBase(period) {}


void TBattery::set_fault(bool condition, size_t fault_bit) {
	if (condition) {
		this->current_set_faults |= (static_cast<uint32_t>(1) << fault_bit);
	}
}

void TBattery::check_and_set_faults() {
	this->current_set_faults = 0;

	// TODO: parameters system
	float MIN_CELL_VOLTAGE = 3.0f;
	float MAX_CELL_VOLTAGE = 4.2f;
	float MIN_BATTERY_VOLTAGE = 3.2f;
	float MAX_BATTERY_VOLTAGE = 4.2f;
	float MAX_VOLTAGE_IMBALANCE = 0.1f;
	float MAX_DISCHARGE_CURRENT = 100.0f;
	float MAX_CHARGE_CURRENT = -50.0f;
	float MIN_TEMP = 20.0f;
	float MAX_TEMP = 60.0f;
	float POWER_MAX = 1400.0f;
	float MAX_TEMP_DIFF = 10.0f;
	
	this->set_fault(TO_VOLTAGE(this->battery_data.min_voltage) < MIN_CELL_VOLTAGE, PersistentFault::CELL_UNDERVOLTAGE);
	this->set_fault(TO_VOLTAGE(this->battery_data.max_voltage) > MAX_CELL_VOLTAGE, PersistentFault::CELL_OVERVOLTAGE);
	this->set_fault(TO_VOLTAGE(this->battery_data.avg_voltage) < MIN_BATTERY_VOLTAGE, PersistentFault::BATTERY_UNDERVOLTAGE);
	this->set_fault(TO_VOLTAGE(this->battery_data.avg_voltage) > MAX_BATTERY_VOLTAGE, PersistentFault::BATTERY_OVERVOLTAGE);
	this->set_fault(
		!util::check_difference(
			TO_VOLTAGE(this->battery_data.max_voltage),
			TO_VOLTAGE(this->battery_data.min_voltage),
			MAX_VOLTAGE_IMBALANCE
		),
		PersistentFault::BATTERY_VOLTAGE_IMBALANCE
	);
	this->set_fault(this->battery_data.current > MAX_DISCHARGE_CURRENT, PersistentFault::OVERCURRENT);
	this->set_fault(this->battery_data.current < MAX_CHARGE_CURRENT, PersistentFault::UNDERCURRENT);
	this->set_fault(!util::check_within(this->battery_data.temps.therms[0], MIN_TEMP, MAX_TEMP), PersistentFault::TEMP_0);
	this->set_fault(!util::check_within(this->battery_data.temps.therms[1], MIN_TEMP, MAX_TEMP), PersistentFault::TEMP_1);
	this->set_fault(!util::check_within(this->battery_data.temps.therms[2], MIN_TEMP, MAX_TEMP), PersistentFault::TEMP_2);
	this->set_fault(!util::check_within(this->battery_data.temps.therms[3], MIN_TEMP, MAX_TEMP), PersistentFault::TEMP_3);

	this->set_fault(
		(TO_VOLTAGE(this->battery_data.avg_voltage) * this->battery_data.current) > POWER_MAX,
		WarningFault::OVERPOWER
	);
	// TODO: handle any_bypassed setting
	for (size_t i = 0; i < THERM_COUNT; i++) {
		for (size_t j = i + 1; j < THERM_COUNT; j++) {
			this->set_fault(
				!util::check_difference(
					this->battery_data.temps.therms[i],
					this->battery_data.temps.therms[j],
					MAX_TEMP_DIFF
				),
				WarningFault::TEMPS_IMBALANCE
			);
		}
	}

	this->previous_set_faults |= this->current_set_faults;
}

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
