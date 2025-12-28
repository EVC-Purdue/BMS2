#ifndef T_BATTERY_HPP
#define T_BATTERY_HPP

#include <cstdint>

#include "freertos/FreeRTOS.h"

#include "task/task_base.hpp"
#include "battery/parameters.hpp"
#include "battery/modes.hpp"
#include "battery/battery.hpp"


namespace t_battery {

constexpr uint32_t TASK_PERIOD_MS = 50;
constexpr uint32_t TASK_STACK_SIZE = 4096;
constexpr UBaseType_t TASK_PRIORITY = 10;
constexpr BaseType_t TASK_CORE_ID = 1;
constexpr const char* TASK_NAME = "BatteryTask";


class TBattery : public task_base::TaskBase {
	private:
		modes::Mode mode;
		battery::BatteryData battery_data;

		params::Parameters parameters;
		
		uint32_t current_set_faults;
		uint32_t previous_set_faults; // For persistent faults and to display past faults in WebApp

        bool any_bypassed; // Whether any cell was bypassed when bypass (noise) mode was active

		// If condition is true, set the fault bit at fault_bit index in current_set_faults
		void set_fault(bool condition, size_t fault_bit);
		// Remove the fault bit at fault_bit index from previous_set_faults
		void clear_fault(size_t fault_bit);
		// Check all battery parameters and set faults accordingly. Also updates previous_set_faults.
		void check_and_set_faults();
		// Return true if any presisent or live faults are currently set or persistent faults still exist.
		bool problems_present();

	public:
		TBattery(uint32_t period);

		void task() override;
};



} // namespace t_battery





#endif // T_BATTERY_HPP