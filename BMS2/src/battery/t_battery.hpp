#ifndef T_BATTERY_HPP
#define T_BATTERY_HPP

#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "task/task_base.hpp"
#include "battery/parameters.hpp"
#include "battery/faults.hpp"
#include "battery/modes.hpp"
#include "battery/battery.hpp"
#include "hardware/LTC/LTC681x.h"
#include "hardware/gpio.hpp"


namespace t_battery {

constexpr uint32_t TASK_PERIOD_MS = 50;
constexpr uint32_t TASK_STACK_SIZE = 4096;
constexpr UBaseType_t TASK_PRIORITY = 10;
constexpr BaseType_t TASK_CORE_ID = 1;
constexpr const char* TASK_NAME = "BatteryTask";

// Params
const uint64_t MEASUREMENT_LOOP_TIME = 100;		// milliseconds(mS)
const uint64_t BALANCE_LOOP_TIME = 10000;		// milliseconds(mS)
const uint64_t POLL_TIME = 1500;  				// milliseconds(mS)

// ADC Command Configurations
const uint8_t ADC_OPT = ADC_OPT_DISABLED;          // See LTC6811_daisy.h for Options
const uint8_t ADC_CONVERSION_MODE = MD_7KHZ_3KHZ;  // MD_7KHZ_3KHZ; //MD_26HZ_2KHZ;//MD_7KHZ_3KHZ; // See LTC6811_daisy.h for Options
const uint8_t ADC_DCP = DCP_DISABLED;              // See LTC6811_daisy.h for Options
const uint8_t CELL_CH_TO_CONVERT = CELL_CH_ALL;    // See LTC6811_daisy.h for Options
const uint8_t AUX_CH_TO_CONVERT = AUX_CH_ALL;      // See LTC6811_daisy.h for Options
const uint8_t STAT_CH_TO_CONVERT = STAT_CH_ALL;    // See LTC6811_daisy.h for Options

#define SHUNT_RESISTANCE 0.0001
#define CURRENT_REF_OFFSET .496
#define MAX_CHARGE_CURRENT -100
#define MAX_DISCHARGE_CURRENT 400
#define MAX_BALANCE_COUNT 5

uint64_t lastSaveTime;
uint64_t lastPollTime;
uint64_t lastStateTime;

#define CONTACTOR_GPIO 25

static_assert(TASK_PERIOD_MS != 0, "TASK_PERIOD_MS must be non-zero, as it is used as a divisor");


class TBattery : public task_base::TaskBase {
    private:
        modes::Mode mode;
        battery::BatteryData battery_data;
        cell_asic bms_ic[battery::IC_COUNT];

        params::Parameters parameters;
        
        faults::FaultManager fault_manager;
        bool new_faults;

        bool any_bypassed; // Whether any cell was bypassed when bypass (noise) mode was active

        size_t iters_without_log; // Number of iterations since last log write

        // Check all battery parameters and set faults accordingly. Also updates previous_set_faults
        // and sets new_faults flag.
        void check_and_set_faults();

        void readBattery();
        void balanceCells();
        void readTempatures();
        bool checkBatteryProblems();

        int sortDescCompFn(const void *cmp1, const void *cmp2);
        void printConfig();


    public:
        TBattery(uint32_t period);

        void task() override;
};



} // namespace t_battery





#endif // T_BATTERY_HPP