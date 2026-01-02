#include <cstdint>
#include <cstring>
#include <optional>

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#include "battery/q_battery.hpp"
#include "battery/faults.hpp"
#include "battery/parameters.hpp"
#include "battery/battery.hpp"
#include "logger/q_logger.hpp"
#include "hardware/pins.hpp"
#include "util/overloaded.hpp"
#include "util/cmp.hpp"


#include "battery/t_battery.hpp"



namespace t_battery {

TBattery::TBattery(uint32_t period)
	: task_base::TaskBase(period),
    mode(modes::Mode::IDLE),
    battery_data({}),
    parameters({}),
    fault_manager({}),
    new_faults(false),
    any_bypassed(false),
    iters_without_log(0)
    {}


void TBattery::check_and_set_faults() {
	this->fault_manager.clear_current_faults();
	
	this->fault_manager.set_fault(battery::TO_VOLTAGE(this->battery_data.min_voltage) < this->parameters.v_min, faults::PersistentFault::CELL_UNDERVOLTAGE);
	this->fault_manager.set_fault(battery::TO_VOLTAGE(this->battery_data.max_voltage) > this->parameters.v_max, faults::PersistentFault::CELL_OVERVOLTAGE);
	this->fault_manager.set_fault(battery::TO_VOLTAGE(this->battery_data.avg_voltage) < this->parameters.v_min_avg, faults::PersistentFault::BATTERY_UNDERVOLTAGE);
	this->fault_manager.set_fault(battery::TO_VOLTAGE(this->battery_data.avg_voltage) > this->parameters.v_max_avg, faults::PersistentFault::BATTERY_OVERVOLTAGE);
	this->fault_manager.set_fault(
		!util::check_difference(
			battery::TO_VOLTAGE(this->battery_data.max_voltage),
			battery::TO_VOLTAGE(this->battery_data.min_voltage),
			this->parameters.v_diff
		),
		faults::PersistentFault::BATTERY_VOLTAGE_IMBALANCE
	);
	this->fault_manager.set_fault(this->battery_data.current > this->parameters.i_max, faults::PersistentFault::OVERCURRENT);
	this->fault_manager.set_fault(this->battery_data.current < this->parameters.i_min, faults::PersistentFault::UNDERCURRENT);
	this->fault_manager.set_fault(!util::check_within(this->battery_data.temps.therms[0], this->parameters.t_min, this->parameters.t_max), faults::PersistentFault::TEMP_0);
	this->fault_manager.set_fault(!util::check_within(this->battery_data.temps.therms[1], this->parameters.t_min, this->parameters.t_max), faults::PersistentFault::TEMP_1);
	this->fault_manager.set_fault(!util::check_within(this->battery_data.temps.therms[2], this->parameters.t_min, this->parameters.t_max), faults::PersistentFault::TEMP_2);
	this->fault_manager.set_fault(!util::check_within(this->battery_data.temps.therms[3], this->parameters.t_min, this->parameters.t_max), faults::PersistentFault::TEMP_3);

	this->fault_manager.set_fault(
		(battery::TO_VOLTAGE(this->battery_data.avg_voltage) * this->battery_data.current) > this->parameters.p_max,
		faults::WarningFault::OVERPOWER
	);
	this->fault_manager.set_fault(this->any_bypassed, faults::WarningFault::ANY_BYPASSED);
	for (size_t i = 0; i < battery::THERM_COUNT; i++) {
		for (size_t j = i + 1; j < battery::THERM_COUNT; j++) {
			this->fault_manager.set_fault(
				!util::check_difference(
					this->battery_data.temps.therms[i],
					this->battery_data.temps.therms[j],
					this->parameters.t_diff
				),
				faults::WarningFault::TEMPS_IMBALANCE
			);
		}
	}

    this->new_faults = this->fault_manager.new_faults_present();
	this->fault_manager.update_previous_faults();
}

void TBattery::task() {
    // Read and process all messages from the battery queue
	q_battery::Message rx_msg = {};
    while (xQueueReceive(q_battery::g_battery_queue, &rx_msg, 0) == pdTRUE) {
        std::visit(util::OverloadedVisit {
            [this](const q_battery::msg::SetMode& sm) {
                this->mode = sm.mode;
                // Handle mode change as necessary
            },
            [this](const params::msg::Message& p_msg) {
                this->parameters.set_parameter(p_msg);
                std::optional<bool> forward_delete_log = this->parameters.try_consume_forward_delete_log();
                if (forward_delete_log.has_value()) {
                    q_logger::msg::SetDeleteLog log_msg = {};
                    log_msg.delete_log = forward_delete_log.value();
                    xQueueSend(q_logger::g_logger_queue, &log_msg, portMAX_DELAY);
                }
            },
            [this](const faults::msg::ClearFault& cf) {
                this->fault_manager.clear_fault(cf.fault_index);
            }
        }, rx_msg);
    }

    // Reset new_faults before checking faults
    this->new_faults = false;

    this->check_and_set_faults();

    // Handle operations based on the current mode
    // switch (this->mode) {
    //     case modes::Mode::IDLE:
    //         // Handle IDLE mode operations
    //         break;
    //     case modes::Mode::MONITORING:
    //         // Handle MONITORING mode operations
    //         break;
    //     case modes::Mode::BALANCING:
    //         // Handle BALANCING mode operations
    //         break;
    // }

    // integer division will floor the result, which is desired here
    uint32_t log_interval_iters = this->parameters.log_inter / t_battery::TASK_PERIOD_MS;
    if ((this->iters_without_log >= log_interval_iters) || this->new_faults) {
        q_logger::msg::LogLine msg = {};
        msg.timestamp = esp_timer_get_time();
        for (size_t i = 0; i < battery::IC_COUNT; i++) {
            memcpy(
                &msg.voltages[i * battery::CELL_COUNT_PER_IC],
                this->battery_data.ics[i].cell_voltages,
                sizeof(this->battery_data.ics[i].cell_voltages)
            );
        }
        msg.temps = this->battery_data.temps;
        msg.current = this->battery_data.current;
        msg.faults = this->fault_manager.get_current_set_faults();
        xQueueSend(q_logger::g_logger_queue, &msg, portMAX_DELAY);

        this->iters_without_log = 0;
    } else {
        this->iters_without_log++;
    }
}


} // namespace t_battery
