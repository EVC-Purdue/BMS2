#ifndef BATTERY_HPP
#define BATTERY_HPP

#include <cstdint>


namespace battery {

constexpr size_t IC_COUNT = 2;
constexpr size_t CELL_COUNT_PER_IC = 12;
constexpr size_t THERM_COUNT = 4;

constexpr float RAW_TO_VOLTAGE_FACTOR = 0.0001f;

inline float TO_VOLTAGE(uint32_t raw_v) {
	return static_cast<float>(raw_v) * RAW_TO_VOLTAGE_FACTOR;
}

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
	float bal_bot;
	float bal_top;
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

} // namespace battery



#endif // BATTERY_HPP