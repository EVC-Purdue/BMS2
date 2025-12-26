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


// TODO: move faults into own header

// Faults stay active until cleared
// If the bit is set in the current or previous fault, it is considered active
enum PersistentFault {
	CELL_UNDERVOLTAGE = 0,
	CELL_OVERVOLTAGE,
	BATTERY_UNDERVOLTAGE,
	BATTERY_OVERVOLTAGE,
	BATTERY_VOLTAGE_IMBALANCE,
	OVERCURRENT,
	UNDERCURRENT,
	TEMP_0,
	TEMP_1,
	TEMP_2,
	TEMP_3,
	PERSISTENT_FAULTS_END
};

// Faults that clear themselves
// If the bit is set in the current fault, it is considered active
enum LiveFault {
	// No live faults defined yet
	LIVE_FAULTS_END = PERSISTENT_FAULTS_END
};

// Faults that warn but do not take action
enum WarningFault {
	OVERPOWER = LIVE_FAULTS_END,
	ANY_BYPASSED,
	TEMPS_IMBALANCE,
	WARNING_FAULTS_END
};

static_assert(WarningFault::WARNING_FAULTS_END <= 32, "Total fault count exceeds 32 bits");


// PARAMETERS


class TBattery : public task_base::TaskBase {
	private:
		State state;
		BatteryData battery_data;
		
		uint32_t current_set_faults;
		uint32_t previous_set_faults; // For persistent faults and to display past faults in WebApp

		void check_and_set_faults();

	public:
		TBattery(uint32_t period);

		void task() override;
};



} // namespace t_battery





#endif // T_BATTERY_HPP