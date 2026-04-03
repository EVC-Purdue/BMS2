#ifndef BATTERY_STATE_CACHE_HPP
#define BATTERY_STATE_CACHE_HPP

#include <cstdint>

#include "battery/battery.hpp"
#include "battery/modes.hpp"
#include "battery/parameters.hpp"

namespace battery_state_cache
{

    struct ParameterSnapshot
    {
        bool bypass = params::PARAMETER_BYPASS;
        float v_bypass = params::PARAMETER_V_BYPASS;

        float v_min = params::PARAMETER_V_MIN;
        float v_max = params::PARAMETER_V_MAX;
        float v_min_avg = params::PARAMETER_V_MIN_AVG;
        float v_max_avg = params::PARAMETER_V_MAX_AVG;
        float v_diff = params::PARAMETER_V_DIFF;

        float t_min = params::PARAMETER_T_MIN;
        float t_max = params::PARAMETER_T_MAX;
        float t_diff = params::PARAMETER_T_DIFF;

        float t_max_bal = params::PARAMETER_T_MAX_BAL;
        float t_reset_bal = params::PARAMETER_T_RESET_BAL;

        uint32_t log_inter = params::PARAMETER_LOG_INTER;
        bool delete_log = params::PARAMETER_DELETE_LOG;

        float v_can_charge = params::PARAMETER_V_CAN_CHARGE;
        float i_can_charge = params::PARAMETER_I_CAN_CHARGE;
    };

    struct Snapshot
    {
        int64_t timestamp_us = 0;

        modes::Mode mode = modes::Mode::IDLE;
        battery::BatteryData battery_data = {};

        ParameterSnapshot parameters = {};

        uint32_t faults = 0;
        uint32_t persistent_faults = 0;

        bool any_bypassed = false;
        bool t_diff_triggered = false;
        bool bal_temp_bot_triggered = false;
        bool bal_temp_top_triggered = false;
    };

    void publish(const Snapshot &snapshot);
    void read(Snapshot &out_snapshot);

} // namespace battery_state_cache

#endif // BATTERY_STATE_CACHE_HPP
