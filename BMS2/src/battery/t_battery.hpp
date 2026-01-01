#ifndef T_BATTERY_HPP
#define T_BATTERY_HPP

#include <cstdint>

#include "freertos/FreeRTOS.h"

#include "task/task_base.hpp"
#include "battery/parameters.hpp"
#include "battery/faults.hpp"
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
		
		faults::FaultManager fault_manager;
        bool new_faults;

        bool any_bypassed; // Whether any cell was bypassed when bypass (noise) mode was active

        size_t iters_without_log; // Number of iterations since last log write

		// Check all battery parameters and set faults accordingly. Also updates previous_set_faults
        // and sets new_faults flag.
		void check_and_set_faults();

	public:
		TBattery(uint32_t period);

		void task() override;
};



} // namespace t_battery





#endif // T_BATTERY_HPP