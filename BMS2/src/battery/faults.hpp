#ifndef BATTERY_FAULTS_HPP
#define BATTERY_FAULTS_HPP

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

#endif // BATTERY_FAULTS_HPP