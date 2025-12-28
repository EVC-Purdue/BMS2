#ifndef BATTERY_FAULTS_HPP
#define BATTERY_FAULTS_HPP

#include <cstdint>
#include <cstddef>


namespace faults {

namespace msg {
    struct ClearFault {
        size_t fault_index;
    };
} // namespace msg

// Note: all fault values (indexes) are unique across all enums

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
	LIVE_FAULTS_END = PersistentFault::PERSISTENT_FAULTS_END
};

// Faults that warn but do not take action
enum WarningFault
{
	OVERPOWER = LiveFault::LIVE_FAULTS_END, // Power (V*I) too high
	ANY_BYPASSED,				 // Bypass (noise) mode is active and it was used
	TEMPS_IMBALANCE,			 // Temperature sensors differ too much
	WARNING_FAULTS_END
};


// uint32_t is used to store faults, so total fault count must not exceed 32
static_assert(WarningFault::WARNING_FAULTS_END <= 32, "Total fault count exceeds 32 bits");


class FaultManager {
    private:
        uint32_t current_set_faults;
        uint32_t previous_set_faults;

    public:
        FaultManager();

		// If condition is true, set the fault bit at fault_bit index in current_set_faults
        void set_fault(bool condition, size_t fault_bit);
		// Remove the fault bit at fault_bit index from previous_set_faults
        void clear_fault(size_t fault_bit);
        // Remove all current faults (start of new check cycle)
        void clear_current_faults();
        // Update previous_set_faults to include all currently set faults
        void update_previous_faults();
		// Return true if any presisent or live faults are currently set or persistent faults still exist.
        bool has_fault_active(size_t fault_bit) const;
};

} // namespace faults

#endif // BATTERY_FAULTS_HPP