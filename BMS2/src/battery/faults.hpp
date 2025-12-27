#ifndef BATTERY_FAULTS_HPP
#define BATTERY_FAULTS_HPP

// Faults stay active until cleared
// If the bit is set in the current or previous fault, it is considered active
enum PersistentFault
{
	CELL_UNDERVOLTAGE = 0,	   // Lowest cell is too low
	CELL_OVERVOLTAGE,		   // Highest cell is too high
	BATTERY_UNDERVOLTAGE,	   // Average cell voltage too low
	BATTERY_OVERVOLTAGE,	   // Average cell voltage too high
	BATTERY_VOLTAGE_IMBALANCE, // Difference between highest and lowest cell too high
	OVERCURRENT,			   // Discharge current too high
	UNDERCURRENT,			   // Charge current too high
	TEMP_0,					   // Temperature sensor 0 too high
	TEMP_1,					   // Temperature sensor 1 too high
	TEMP_2,					   // Temperature sensor 2 too high
	TEMP_3,					   // Temperature sensor 3 too high
	PERSISTENT_FAULTS_END
};

// Faults that clear themselves
// If the bit is set in the current fault, it is considered active
enum LiveFault
{
	// No live faults defined yet
	LIVE_FAULTS_END = PERSISTENT_FAULTS_END
};

// Faults that warn but do not take action
enum WarningFault
{
	OVERPOWER = LIVE_FAULTS_END, // Power (V*I) too high
	ANY_BYPASSED,				 // Bypass (noise) mode is active and it was used
	TEMPS_IMBALANCE,			 // Temperature sensors differ too much
	WARNING_FAULTS_END
};

static_assert(WarningFault::WARNING_FAULTS_END <= 32, "Total fault count exceeds 32 bits");

#endif // BATTERY_FAULTS_HPP