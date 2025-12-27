#ifndef BATTERY_PARAMETERS_HPP
#define BATTERY_PARAMETERS_HPP

#include <cstdint>
#include <variant>


namespace params {

// Initial parameter values
constexpr bool PARAMETER_BYPASS = false;       // bypass enabled
constexpr float PARAMETER_V_BYPASS = 5.0f;     // bypass voltage
constexpr float PARAMETER_V_MIN = 3.0f;        // per cell min voltage
constexpr float PARAMETER_V_MAX = 4.2f;        // per cell max voltage
constexpr float PARAMETER_V_MIN_AVG = 3.0f;    // min average voltage
constexpr float PARAMETER_V_MAX_AVG = 4.2f;    // min average voltage
constexpr float PARAMETER_V_DIFF = 0.2f;       // max voltage difference
constexpr float PARAMETER_T_MIN = 10.0f;       // min temp
constexpr float PARAMETER_T_MAX = 50.0f;       // max temp
constexpr float PARAMETER_T_DIFF = 30.0f;      // max temp difference
constexpr float PARAMETER_T_MAX_BAL = 50.0f;   // max temp for balancing
constexpr float PARAMETER_T_RESET_BAL = 40.0f; // reset temp for balancing (once it goes below this we can balance again)
constexpr uint32_t PARAMETER_LOG_SPEED = 1000; // How often to save to log in monitor state (ms)
constexpr bool PARAMETER_DELETE_LOG = false;   // delete log to make space if full
// constexpr float PARAMETER_V_CAN_CHARGE = 100.8f; // Total voltage to charge to. Sent over CAN
// constexpr float PARAMETER_I_CAN_CHARGE = 10.0f;  // Current to charge at. Sent over CAN
constexpr float PARAMETER_OVER_POWER = 14000.0f; // 14kW max power

constexpr size_t KEY_CHAR_COUNT = 16; // Number of chars that could be used for the key string


namespace msg {
    struct SetParameterF32 {
        char key[KEY_CHAR_COUNT];
        float value;
    };
    struct SetParameterU32 {
        char key[KEY_CHAR_COUNT];
        uint32_t value;
    };
    struct SetParameterBool {
        char key[KEY_CHAR_COUNT];
        bool value;
    };

    using Message = std::variant<
        SetParameterF32,
        SetParameterU32,
        SetParameterBool
    >;
} // namespace msg


class Parameters {
    private:
        void set_parameter_f32(const char key[KEY_CHAR_COUNT], float value);
        void set_parameter_u32(const char key[KEY_CHAR_COUNT], uint32_t value);
        void set_parameter_bool(const char key[KEY_CHAR_COUNT], bool value);

    public:
        bool bypass = PARAMETER_BYPASS;
        float v_bypass = PARAMETER_V_BYPASS;

        float v_min = PARAMETER_V_MIN;
        float v_max = PARAMETER_V_MAX;
        float v_min_avg = PARAMETER_V_MIN_AVG;
        float v_max_avg = PARAMETER_V_MAX_AVG;
        float v_diff = PARAMETER_V_DIFF;

        float t_min = PARAMETER_T_MIN;
        float t_max = PARAMETER_T_MAX;
        float t_diff = PARAMETER_T_DIFF;

        float t_max_bal = PARAMETER_T_MAX_BAL;
        float t_reset_bal = PARAMETER_T_RESET_BAL;

        uint32_t log_speed = PARAMETER_LOG_SPEED;
        bool delete_log = PARAMETER_DELETE_LOG;

        float over_power = PARAMETER_OVER_POWER;

        // float v_can_charge = PARAMETER_V_CAN_CHARGE;
        // float i_can_charge = PARAMETER_I_CAN_CHARGE;

        // Later TODO: load params from SPIFFS

        void set_parameter(const msg::Message& msg);
};


} // namespace params


#endif // BATTERY_PARAMETERS_HPP