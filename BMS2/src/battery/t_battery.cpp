#include <cstdint>
#include <cstring>
#include <optional>
#include <algorithm>
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
#include "hardware/LTC/LTC6811.h"
#include "battery/modes.hpp"
#include "logger/t_logger.hpp"
#include "battery/battery.hpp"

// TODO: RunCommand function


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

    uint64_t loopTime;
	if (mode == modes::Mode::MONITORING || 
        mode == modes::Mode::IDLE) { loopTime = MEASUREMENT_LOOP_TIME; }

	else if (mode == modes::Mode::BALANCING)              { loopTime = BALANCE_LOOP_TIME; }
	loopTime *= 1000; // Adjust for micro to milliseconds

	uint64_t currentTime = esp_timer_get_time();
	if (currentTime - lastStateTime > loopTime) {
		lastStateTime = currentTime; 
		switch (mode) {
			case modes::Mode::MONITORING:
				readTempatures();
				readBattery();

				if (currentTime - lastSaveTime > parameters.log_inter) {
					lastSaveTime = currentTime;
					t_logger::saveEventCounter++;
				}
				break;
			case modes::Mode::BALANCING:
				readTempatures();
				readBattery();
				balanceCells();
				break;
			case modes::Mode::IDLE:
				readTempatures();
				readBattery();
				break;
		}

		if ((mode == modes::Mode::MONITORING || mode == modes::Mode::IDLE) && checkBatteryProblems()) {
			OUTPUT_LOW(CONTACTOR_GPIO);
			if (mode == modes::Mode::MONITORING){
        		t_logger::saveEventCounter++;
      		}
			//buzzOn = true;
		}
	}

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


// ============== Battery reading and measurement functions ============== //

void TBattery::readBattery() {
    int TOTAL_IC = battery::IC_COUNT;
	wakeup_sleep(TOTAL_IC);
	clear_discharge(TOTAL_IC, bms_ic);

	// turn off bypass
	for (int current_ic = 0; current_ic < TOTAL_IC; current_ic++) {
		bool gpio[5] = {1, 1, 1, 1, 1};
		LTC681x_set_cfgr_gpio(current_ic, bms_ic, gpio);
	}

	LTC6811_wrcfg(TOTAL_IC, bms_ic);
    vTaskDelay(pdMS_TO_TICKS(100));  // allow the filters to settle

    // Read from the battery management ICs and store in bms_ic
	int8_t error = 0;
    wakeup_idle(battery::IC_COUNT);
    LTC6811_adcv(2, 0, 0);
    LTC6811_pollAdc();
    wakeup_idle(battery::IC_COUNT);
    error = LTC6811_rdcv(0, battery::IC_COUNT, bms_ic);
    if (unlikely(error)) {
        // Handle error (e.g., log it, set fault flags, etc.)
        printf("A PEC error was detected in the received data");
    }


	any_bypassed = false;
	// check if any are going to end up being bypassed before we start updating values
	if (parameters.bypass) {
		for (int current_ic = 0; current_ic < TOTAL_IC; current_ic++) {
			for (int i = 0; i < bms_ic[0].ic_reg.cell_channels; i++) {
				if (bms_ic[current_ic].cells.c_codes[i] * 0.0001 >= parameters.v_bypass) {
					any_bypassed = true;
					break;
				}
			}
		}
	}

	if (unlikely(any_bypassed)) {
		return;
	}

	battery_data.min_voltage = bms_ic[0].cells.c_codes[0];
	battery_data.max_voltage = bms_ic[0].cells.c_codes[0];
	battery_data.sum_voltage = 0;

	for (int current_ic = 0; current_ic < TOTAL_IC; current_ic++) {
		for (int cell_num = 0; cell_num < bms_ic[0].ic_reg.cell_channels; cell_num++) {
			battery_data.sum_voltage += bms_ic[current_ic].cells.c_codes[cell_num];
			
			if (bms_ic[current_ic].cells.c_codes[cell_num] < battery_data.min_voltage) {
				battery_data.min_voltage = bms_ic[current_ic].cells.c_codes[cell_num];
			}
			
			if (bms_ic[current_ic].cells.c_codes[cell_num] > battery_data.max_voltage) {
				battery_data.max_voltage = bms_ic[current_ic].cells.c_codes[cell_num];
			}
		}
	}
	battery_data.avg_voltage = battery_data.sum_voltage / (TOTAL_IC * bms_ic[0].ic_reg.cell_channels);
}

void TBattery::balanceCells() {
	clear_discharge(battery::IC_COUNT, bms_ic);

	for (int current_ic = 0; current_ic < battery::IC_COUNT; current_ic++) {
		battery_data.ics[current_ic].discharge = 0;

		int sortedCells[12];
		for (int i = 0; i < bms_ic[0].ic_reg.cell_channels; i++) {
			sortedCells[i] = i;
		}

		battery::IcData* packToSort = &battery_data.ics[current_ic];
		std::sort(sortedCells, sortedCells + bms_ic[0].ic_reg.cell_channels,
		    [packToSort](int a, int b) {
		        int val_a = packToSort->cell_voltages[a];
		        int val_b = packToSort->cell_voltages[b];
		        return val_a > val_b;  // descending order
		    });

		printf("Cells largest to smalest: ");

		printf("Cell: %d\n", sortedCells[0] + 1);
		for (int i = 0; i < bms_ic[0].ic_reg.cell_channels - 1; i++) {
			printf("Cell: %d\n", sortedCells[i + 1] + 1);
			
			if (sortedCells[i] == -1) {
				continue;
			}

			for (int cell = i; cell < 12 - 1; cell++) {
				// if cells are next to each other on the bms remove. no adjectent cells can be balanced at a time
				if (sortedCells[i] + 1 == sortedCells[cell + 1]) {
					sortedCells[cell + 1] = -1;
				}

				if (sortedCells[i] - 1 == sortedCells[cell + 1]) {
					sortedCells[cell + 1] = -1;
				}
			}
		}

		for (int i = 0, count = 0; count < MAX_BALANCE_COUNT && i < 12; i++) {
			if (sortedCells[i] != -1 &&
					(packToSort->cell_voltages[sortedCells[i]] > battery_data.ics[current_ic].avg_voltage + 0.001 / 0.0001 ||
					 packToSort->cell_voltages[sortedCells[i]] > battery_data.avg_voltage + 0.001 / 0.0001)) {  // 0.01 V above average.
				printf("Discharging: %d\n", 12 * current_ic + sortedCells[i] + 1);
				LTC6811_set_discharge(12 * (1 - current_ic) + sortedCells[i] + 1, battery::IC_COUNT, bms_ic);
				battery_data.ics[current_ic].discharge |= 1 << sortedCells[i];
				for 
				count++;
			}
		}
	}
	wakeup_sleep(battery::IC_COUNT);
	LTC6811_wrcfg(battery::IC_COUNT, bms_ic);
	printConfig();
	t_logger::saveEventCounter++;
}

void TBattery::readTempatures() {
	int error;
	wakeup_sleep(battery::IC_COUNT);
	LTC6811_adax(ADC_CONVERSION_MODE, AUX_CH_TO_CONVERT);
	LTC6811_pollAdc();

	vTaskDelay(pdMS_TO_TICKS(100));
	error = LTC6811_rdaux(0, battery::IC_COUNT, bms_ic);  // Set to read back all aux registers
	checkError(error);

	float fetV;

	battery.pack[TEMP_PACK_IDX].therm1Temp = cellTemp(bms_ic[0].aux.a_codes[THERM1_GPIO - 1] * 0.0001);
	battery.pack[TEMP_PACK_IDX].therm2Temp = cellTemp(bms_ic[0].aux.a_codes[THERM2_GPIO - 1] * 0.0001);
	battery.pack[TEMP_PACK_IDX].therm3Temp = cellTemp(bms_ic[0].aux.a_codes[THERM3_GPIO - 1] * 0.0001);
	fetV = bms_ic[0].aux.a_codes[THERM_FET_GPIO - 1] * 0.0001;
	battery.pack[TEMP_PACK_IDX].thermFETTemp = steinhart((fetV * 10000) / (3 - fetV));
	battery.current = convertCurrent(bms_ic[0].aux.a_codes[CURRENT_GPIO - 1] * 0.0001);
}

bool TBattery::checkBatteryProblems() {
	// return (battery.min.voltage * 0.0001 < parameters.vMin) ||
	// 	(battery.max.voltage * 0.0001 > parameters.vMax) ||
	// 	   (battery.average     * 0.0001 < parameters.vMinAvg) ||
	// 	   (battery.average     * 0.0001 > parameters.vMaxAvg) ||
	// 	   (battery.max.voltage * 0.0001 - battery.min.voltage * 0.0001 > 0.2) ||
	// 	   (battery.pack[TEMP_PACK_IDX].therm1Temp   > parameters.tMax) ||
	// 	   (battery.pack[TEMP_PACK_IDX].therm2Temp   > parameters.tMax) ||
	// 	   (battery.pack[TEMP_PACK_IDX].therm3Temp   > parameters.tMax) ||
	// 	   (battery.pack[TEMP_PACK_IDX].thermFETTemp > parameters.tMax) ||
	// 	   (battery.pack[TEMP_PACK_IDX].therm1Temp   < parameters.tMin) ||
	// 	   (battery.pack[TEMP_PACK_IDX].therm2Temp   < parameters.tMin) ||
	// 	   (battery.pack[TEMP_PACK_IDX].therm3Temp   < parameters.tMin) ||
	// 	   (battery.pack[TEMP_PACK_IDX].thermFETTemp < parameters.tMin) ||
	// 	   (battery.current > MAX_CHARGE_CURRENT);

	// TODO_COUNT++;

	// Serial.println("battery.min.voltage, parameters.vMin");
	// Serial.println(battery.min.voltage * 0.0001);
	// Serial.println(parameters.vMin);

	// Serial.println("battery.max.voltage, parameters.vMax");
	// Serial.println(battery.max.voltage * 0.0001);
	// Serial.println(parameters.vMax);

	// Serial.println("battery.average, parameters.vMinAvg");
	// Serial.println(battery.average * 0.0001);
	// Serial.println(parameters.vMinAvg);

	// Serial.println("battery.average, parameters.vMaxAvg");
	// Serial.println(battery.average * 0.0001);
	// Serial.println(parameters.vMaxAvg);

	// Serial.println("battery.max.voltage, battery.min.voltage, parameters.vDiff");
	// Serial.println(battery.max.voltage * 0.0001);
	// Serial.println(battery.min.voltage * 0.0001);
	// Serial.println(parameters.vDiff);

	// Serial.println("battery.pack[TEMP_PACK_IDX].therm1Temp, parameters.tMin, parameters.tMax");
	// Serial.println(battery.pack[TEMP_PACK_IDX].therm1Temp);
	// Serial.println(parameters.tMin);
	// Serial.println(parameters.tMax);

	// Serial.println("battery.pack[TEMP_PACK_IDX].therm2Temp, parameters.tMin, parameters.tMax");
	// Serial.println(battery.pack[TEMP_PACK_IDX].therm2Temp);
	// Serial.println(parameters.tMin);
	// Serial.println(parameters.tMax);

	// Serial.println("battery.pack[TEMP_PACK_IDX].therm3Temp, parameters.tMin, parameters.tMax");
	// Serial.println(battery.pack[TEMP_PACK_IDX].therm3Temp);
	// Serial.println(parameters.tMin);
	// Serial.println(parameters.tMax);

	// Serial.println("battery.current, MAX_CHARGE_CURRENT");
	// Serial.println(battery.current);
	// Serial.println(MAX_CHARGE_CURRENT);


	// Serial.println("\nSTARTING TO CHECK BATTERY PROBLEMS");

	checkTemperatureDifferences();

	// Serial.println("CHECKED TEMPERATURE DIFFERENCES! (SUCCESS): diff triggered: ");
	// Serial.println(battery.tDiffTriggered);

	// Serial.println("battery.tDiffTriggered");
	// Serial.println(battery.tDiffTriggered);

	return (battery.min.voltage * 0.0001 < parameters.vMin) ||
		   (battery.max.voltage * 0.0001 > parameters.vMax) ||
		   (!checkVoltageWithin(battery.average, parameters.vMinAvg, parameters.vMaxAvg)) ||
		   (!checkDiff(battery.max.voltage * 0.0001, battery.min.voltage * 0.0001, parameters.vDiff)) ||
		   (!checkWithin(battery.pack[TEMP_PACK_IDX].therm1Temp, parameters.tMin, parameters.tMax)) ||
		   (!checkWithin(battery.pack[TEMP_PACK_IDX].therm2Temp, parameters.tMin, parameters.tMax)) ||
		   (!checkWithin(battery.pack[TEMP_PACK_IDX].therm3Temp, parameters.tMin, parameters.tMax)) ||
		   (battery.current < MAX_CHARGE_CURRENT) || (battery.current > MAX_DISCHARGE_CURRENT);
}

// Util functions


int TBattery::sortDescCompFn(const void *cmp1, const void *cmp2) {
	// Need to cast the void * to int * before dereferencing
	int a = packToSort->cells[*((int *)cmp1)];
	int b = packToSort->cells[*((int *)cmp2)];
	return a > b ? -1 : (a < b ? 1 : 0);
}

} // namespace t_battery
