#include <cstdint>
#include <cstring>

#include "util/overloaded.hpp"


#include "battery/parameters.hpp"



namespace params {

void Parameters::set_parameter_f32(const char key[KEY_CHAR_COUNT], float value) {
    if (std::strncmp(key, "v_bypass", KEY_CHAR_COUNT) == 0) {
        this->v_bypass = value;
    } else if (std::strncmp(key, "v_min", KEY_CHAR_COUNT) == 0) {
        this->v_min = value;
    } else if (std::strncmp(key, "v_max", KEY_CHAR_COUNT) == 0) {
        this->v_max = value;
    } else if (std::strncmp(key, "v_min_avg", KEY_CHAR_COUNT) == 0) {
        this->v_min_avg = value;
    } else if (std::strncmp(key, "v_max_avg", KEY_CHAR_COUNT) == 0) {
        this->v_max_avg = value;
    } else if (std::strncmp(key, "v_diff", KEY_CHAR_COUNT) == 0) {
        this->v_diff = value;
    } else if (std::strncmp(key, "t_min", KEY_CHAR_COUNT) == 0) {
        this->t_min = value;
    } else if (std::strncmp(key, "t_max", KEY_CHAR_COUNT) == 0) {
        this->t_max = value;
    } else if (std::strncmp(key, "t_diff", KEY_CHAR_COUNT) == 0) {
        this->t_diff = value;
    } else if (std::strncmp(key, "t_max_bal", KEY_CHAR_COUNT) == 0) {
        this->t_max_bal = value;
    } else if (std::strncmp(key, "t_reset_bal", KEY_CHAR_COUNT) == 0) {
        this->t_reset_bal = value;
    } else if (std::strncmp(key, "over_power", KEY_CHAR_COUNT) == 0) {
        this->over_power = value;
    }

}

void Parameters::set_parameter_u32(const char key[KEY_CHAR_COUNT], uint32_t value) {
    if (std::strncmp(key, "log_speed", KEY_CHAR_COUNT) == 0) {
        this->log_speed = value;
    }
}

void Parameters::set_parameter_bool(const char key[KEY_CHAR_COUNT], bool value) {
    if (std::strncmp(key, "bypass", KEY_CHAR_COUNT) == 0) {
        this->bypass = value;
    } else if (std::strncmp(key, "delete_log", KEY_CHAR_COUNT) == 0) {
        this->delete_log = value;
    }
}

void Parameters::set_parameter(const msg::Message& msg) {
    std::visit(
        util::OverloadedVisit {
            [this](const msg::SetParameterF32& p) {
                this->set_parameter_f32(p.key, p.value);
            },
            [this](const msg::SetParameterU32& p) {
                this->set_parameter_u32(p.key, p.value);
            },
            [this](const msg::SetParameterBool& p) {
                this->set_parameter_bool(p.key, p.value);
            }
        },
        msg
    );   

}

} // namespace params