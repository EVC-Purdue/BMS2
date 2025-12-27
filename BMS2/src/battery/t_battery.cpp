#include <cstdint>

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

void TBattery::clear_fault(size_t fault_bit) {
	this->previous_set_faults &= ~(static_cast<uint32_t>(1) << fault_bit);
}

void TBattery::check_and_set_faults() {
	this->current_set_faults = 0;
	
	this->set_fault(TO_VOLTAGE(this->battery_data.min_voltage) < this->parameters.v_min, faults::PersistentFault::CELL_UNDERVOLTAGE);
	this->set_fault(TO_VOLTAGE(this->battery_data.max_voltage) > this->parameters.v_max, faults::PersistentFault::CELL_OVERVOLTAGE);
	this->set_fault(TO_VOLTAGE(this->battery_data.avg_voltage) < this->parameters.v_min_avg, faults::PersistentFault::BATTERY_UNDERVOLTAGE);
	this->set_fault(TO_VOLTAGE(this->battery_data.avg_voltage) > this->parameters.v_max_avg, faults::PersistentFault::BATTERY_OVERVOLTAGE);
	this->set_fault(
		!util::check_difference(
			TO_VOLTAGE(this->battery_data.max_voltage),
			TO_VOLTAGE(this->battery_data.min_voltage),
			this->parameters.v_diff
		),
		faults::PersistentFault::BATTERY_VOLTAGE_IMBALANCE
	);
	this->set_fault(this->battery_data.current > this->parameters.i_max, faults::PersistentFault::OVERCURRENT);
	this->set_fault(this->battery_data.current < this->parameters.i_min, faults::PersistentFault::UNDERCURRENT);
	this->set_fault(!util::check_within(this->battery_data.temps.therms[0], this->parameters.t_min, this->parameters.t_max), faults::PersistentFault::TEMP_0);
	this->set_fault(!util::check_within(this->battery_data.temps.therms[1], this->parameters.t_min, this->parameters.t_max), faults::PersistentFault::TEMP_1);
	this->set_fault(!util::check_within(this->battery_data.temps.therms[2], this->parameters.t_min, this->parameters.t_max), faults::PersistentFault::TEMP_2);
	this->set_fault(!util::check_within(this->battery_data.temps.therms[3], this->parameters.t_min, this->parameters.t_max), faults::PersistentFault::TEMP_3);

	this->set_fault(
		(TO_VOLTAGE(this->battery_data.avg_voltage) * this->battery_data.current) > this->parameters.p_max,
		faults::WarningFault::OVERPOWER
	);
	this->set_fault(this->any_bypassed, faults::WarningFault::ANY_BYPASSED);
	for (size_t i = 0; i < THERM_COUNT; i++) {
		for (size_t j = i + 1; j < THERM_COUNT; j++) {
			this->set_fault(
				!util::check_difference(
					this->battery_data.temps.therms[i],
					this->battery_data.temps.therms[j],
					this->parameters.t_diff
				),
				faults::WarningFault::TEMPS_IMBALANCE
			);
		}
	}

	this->previous_set_faults |= this->current_set_faults;
}

bool TBattery::problems_present() {
	uint32_t persistent_faults_mask = (static_cast<uint32_t>(1) << faults::PersistentFault::PERSISTENT_FAULTS_END) - 1;
	uint32_t live_faults_mask = ((static_cast<uint32_t>(1) << faults::LiveFault::LIVE_FAULTS_END) - 1) & ~persistent_faults_mask;
	return ((this->current_set_faults & persistent_faults_mask) != 0) ||  // Check current persistent faults
		   ((this->previous_set_faults & persistent_faults_mask) != 0) || // Check previous persistent faults
		   ((this->current_set_faults & live_faults_mask) != 0); 		  // Check current live faults
}

void TBattery::task() {
	// Implement logging functionality here
	q_battery::Message msg = {};

	// Read messages from the battery queue
	if (xQueueReceive(q_battery::g_battery_queue, &msg, 0) == pdTRUE) {
		// std::visit(util::OverloadedVisit{
		// 	[](const q_battery::msg::Write& w) {
		// 		// handle write
		// 	},
		// 	[](const q_battery::msg::Read& r) {
		// 		// handle read
		// 	},
		// 	[](const q_battery::msg::Flush& f) {
		// 		// handle flush
		// 	}
		// }, msg);
	}
	
}


} // namespace t_battery
