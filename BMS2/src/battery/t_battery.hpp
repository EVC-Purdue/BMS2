#ifndef T_BATTERY_HPP
#define T_BATTERY_HPP

#include <cstdint>

#include "freertos/FreeRTOS.h"

#include "task/task_base.hpp"


namespace t_battery {

constexpr uint32_t TASK_PERIOD_MS = 50;
constexpr uint32_t TASK_STACK_SIZE = 4096;
constexpr UBaseType_t TASK_PRIORITY = 10;
constexpr BaseType_t TASK_CORE_ID = 1;
constexpr const char* TASK_NAME = "BatteryTask";

constexpr size_t IC_COUNT = 2;
constexpr size_t CELL_COUNT_PER_IC = 12;
constexpr size_t THERM_COUNT = 4;

constexpr float RAW_TO_VOLTAGE_FACTOR = 0.0001f;

inline float TO_VOLTAGE(uint32_t raw_v) {
	return static_cast<float>(raw_v) * RAW_TO_VOLTAGE_FACTOR;
}

enum State {
	IDLE,
	MONITORING,
	BALANCING
};

struct IcData {
	uint32_t cell_voltages[CELL_COUNT_PER_IC];
	uint32_t min_voltage;
	uint32_t max_voltage;
	uint32_t avg_voltage;
	uint32_t sum_voltage;
	bool discharge[CELL_COUNT_PER_IC];
};

struct TempData {
	float therms[THERM_COUNT];
	float fet;
	float balBot;
	float balTop;
};

struct BatteryData {
	IcData ics[IC_COUNT];
	TempData temps;

	float current;

	uint32_t min_voltage;
	uint32_t max_voltage;
	uint32_t avg_voltage;
	uint32_t sum_voltage;
};


// PARAMETERS


class TBattery : public task_base::TaskBase {
	private:
		State state;
		BatteryData battery_data;
		
		uint32_t current_set_faults;
		uint32_t previous_set_faults; // For persistent faults and to display past faults in WebApp

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