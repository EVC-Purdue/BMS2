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


// Faults stay live until cleared
class PersistentFaults {
	public:
		const static size_t FAULT_COUNT = 7 + THERM_COUNT;

		bool cell_undervoltage;
		bool cell_overvoltage;
		bool battery_undervoltage;
		bool battery_overvoltage;
		bool battery_voltage_imbalance;
		bool overcurrent;
		bool undercurrent;
		bool temps[THERM_COUNT];

		void formatFaults(uint32_t& out);
};

// Faults that clear themselves
class LiveFaults {
	public:
		const static size_t FAULT_COUNT = 0;
		void formatFaults(uint32_t& out) {}
};

// Faults that warn but do not take action
class WarningFaults {
	public:
		const static size_t FAULT_COUNT = 3;

		bool overpower;
		bool any_bypassed;
		bool temps_imbalance;

		void formatFaults(uint32_t& out);
};

constexpr size_t TOTAL_FAULT_COUNT = PersistentFaults::FAULT_COUNT + LiveFaults::FAULT_COUNT + WarningFaults::FAULT_COUNT;

static_assert(TOTAL_FAULT_COUNT < 32, "Total fault count exceeds 32 bits");


// PARAMETERS



class TBattery : public task_base::TaskBase {
	private:
		State state;
		BatteryData battery_data;

	public:
		TBattery(uint32_t period);

		void task() override;
};



} // namespace t_battery





#endif // T_BATTERY_HPP