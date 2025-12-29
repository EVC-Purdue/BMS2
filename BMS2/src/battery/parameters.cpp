#include <cstdint>
#include <cstring>

#include "util/overloaded.hpp"


#include "battery/parameters.hpp"



namespace params {

void Parameters::set_parameter_f32(const char key[KEY_CHAR_COUNT], float value) {
    if (std::strncmp(key, "v_bypass", sizeof("v_bypass")) == 0) {
        this->v_bypass = value;
    } else if (std::strncmp(key, "v_min", sizeof("v_min")) == 0) {
        this->v_min = value;
    } else if (std::strncmp(key, "v_max", sizeof("v_max")) == 0) {
        this->v_max = value;
    } else if (std::strncmp(key, "v_min_avg", sizeof("v_min_avg")) == 0) {
        this->v_min_avg = value;
    } else if (std::strncmp(key, "v_max_avg", sizeof("v_max_avg")) == 0) {
        this->v_max_avg = value;
    } else if (std::strncmp(key, "v_diff", sizeof("v_diff")) == 0) {
        this->v_diff = value;
    } else if (std::strncmp(key, "t_min", sizeof("t_min")) == 0) {
        this->t_min = value;
    } else if (std::strncmp(key, "t_max", sizeof("t_max")) == 0) {
        this->t_max = value;
    } else if (std::strncmp(key, "t_diff", sizeof("t_diff")) == 0) {
        this->t_diff = value;
    } else if (std::strncmp(key, "i_max", sizeof("i_max")) == 0) {
        this->i_max = value;
    } else if (std::strncmp(key, "i_min", sizeof("i_min")) == 0) {
        this->i_min = value;
    } else if (std::strncmp(key, "t_max_bal", sizeof("t_max_bal")) == 0) {
        this->t_max_bal = value;
    } else if (std::strncmp(key, "t_reset_bal", sizeof("t_reset_bal")) == 0) {
        this->t_reset_bal = value;
    } else if (std::strncmp(key, "p_max", sizeof("p_max")) == 0) {
        this->p_max = value;
    }

}

void Parameters::set_parameter_u32(const char key[KEY_CHAR_COUNT], uint32_t value) {
    if (std::strncmp(key, "log_speed", sizeof("log_speed")) == 0) {
        this->log_speed = value;
    }
}

void Parameters::set_parameter_bool(const char key[KEY_CHAR_COUNT], bool value) {
    if (std::strncmp(key, "bypass", sizeof("bypass")) == 0) {
        this->bypass = value;
    } else if (std::strncmp(key, "delete_log", sizeof("delete_log")) == 0) {
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