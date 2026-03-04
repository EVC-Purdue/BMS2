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
// #include "util/serial.hpp"
#include "hardware/pins.hpp"
#include "math.h"
#include "t_battery.hpp"
#include <stdio.h>

// TODO: RunCommand function

namespace t_battery
{
	int gain_set = 3;
	uint64_t lastSaveTime = 0;
	uint64_t lastPollTime = 0;
	uint64_t lastStateTime = 0;

	TBattery::TBattery(uint32_t period)
		: task_base::TaskBase(period),
		  mode(modes::Mode::IDLE),
		  battery_data({}),
		  parameters({}),
		  fault_manager({}),
		  new_faults(false),
		  any_bypassed(false),
		  iters_without_log(0)
	{
	}

	void TBattery::check_and_set_faults()
	{
		this->fault_manager.clear_current_faults();

		this->fault_manager.set_fault(battery::TO_VOLTAGE(this->battery_data.min_voltage) < this->parameters.v_min, faults::PersistentFault::CELL_UNDERVOLTAGE);
		this->fault_manager.set_fault(battery::TO_VOLTAGE(this->battery_data.max_voltage) > this->parameters.v_max, faults::PersistentFault::CELL_OVERVOLTAGE);
		this->fault_manager.set_fault(battery::TO_VOLTAGE(this->battery_data.avg_voltage) < this->parameters.v_min_avg, faults::PersistentFault::BATTERY_UNDERVOLTAGE);
		this->fault_manager.set_fault(battery::TO_VOLTAGE(this->battery_data.avg_voltage) > this->parameters.v_max_avg, faults::PersistentFault::BATTERY_OVERVOLTAGE);
		this->fault_manager.set_fault(
			!util::check_difference(
				battery::TO_VOLTAGE(this->battery_data.max_voltage),
				battery::TO_VOLTAGE(this->battery_data.min_voltage),
				this->parameters.v_diff),
			faults::PersistentFault::BATTERY_VOLTAGE_IMBALANCE);
		this->fault_manager.set_fault(this->battery_data.current > this->parameters.i_max, faults::PersistentFault::OVERCURRENT);
		this->fault_manager.set_fault(this->battery_data.current < this->parameters.i_min, faults::PersistentFault::UNDERCURRENT);
		this->fault_manager.set_fault(!util::check_within(this->battery_data.temps.therms[0], this->parameters.t_min, this->parameters.t_max), faults::PersistentFault::TEMP_0);
		this->fault_manager.set_fault(!util::check_within(this->battery_data.temps.therms[1], this->parameters.t_min, this->parameters.t_max), faults::PersistentFault::TEMP_1);
		this->fault_manager.set_fault(!util::check_within(this->battery_data.temps.therms[2], this->parameters.t_min, this->parameters.t_max), faults::PersistentFault::TEMP_2);
		this->fault_manager.set_fault(!util::check_within(this->battery_data.temps.therms[3], this->parameters.t_min, this->parameters.t_max), faults::PersistentFault::TEMP_3);

		this->fault_manager.set_fault(
			(battery::TO_VOLTAGE(this->battery_data.avg_voltage) * this->battery_data.current) > this->parameters.p_max,
			faults::WarningFault::OVERPOWER);
		this->fault_manager.set_fault(this->any_bypassed, faults::WarningFault::ANY_BYPASSED);
		for (size_t i = 0; i < battery::THERM_COUNT; i++)
		{
			for (size_t j = i + 1; j < battery::THERM_COUNT; j++)
			{
				this->fault_manager.set_fault(
					!util::check_difference(
						this->battery_data.temps.therms[i],
						this->battery_data.temps.therms[j],
						this->parameters.t_diff),
					faults::WarningFault::TEMPS_IMBALANCE);
			}
		}

		this->new_faults = this->fault_manager.new_faults_present();
		this->fault_manager.update_previous_faults();
	}

	void TBattery::task()
	{
		// Read and process all messages from the battery queue
		q_battery::Message rx_msg = {};
		while (xQueueReceive(q_battery::g_battery_queue, &rx_msg, 0) == pdTRUE)
		{
			std::visit(
				util::OverloadedVisit{[this](const q_battery::msg::SetMode &sm)
									  {
										  this->mode = sm.mode;
										  // Handle mode change as necessary
									  },
									  [this](const params::msg::Message &p_msg)
									  {
										  this->parameters.set_parameter(p_msg);
										  std::optional<bool> forward_delete_log = this->parameters.try_consume_forward_delete_log();
										  if (forward_delete_log.has_value())
										  {
											  q_logger::msg::SetDeleteLog log_msg = {};
											  log_msg.delete_log = forward_delete_log.value();
											  xQueueSend(q_logger::g_logger_queue, &log_msg, portMAX_DELAY);
										  }
									  },
									  [this](const faults::msg::ClearFault &cf)
									  {
										  this->fault_manager.clear_fault(cf.fault_index);
									  }},
				rx_msg);
		}

		// Reset new_faults before checking faults
		this->new_faults = false;

		this->check_and_set_faults();

		uint64_t loopTime = MEASUREMENT_LOOP_TIME;
		if (mode == modes::Mode::MONITORING ||
			mode == modes::Mode::IDLE)
		{
			loopTime = MEASUREMENT_LOOP_TIME;
		}

		else if (mode == modes::Mode::BALANCING)
		{
			loopTime = BALANCE_LOOP_TIME;
		}
		loopTime *= 1000; // Adjust for micro to milliseconds

		int64_t currentTime = esp_timer_get_time();
		if (currentTime - lastStateTime > loopTime)
		{
			lastStateTime = currentTime;
			switch (mode)
			{
			case modes::Mode::MONITORING:
				readBattery();
				readTempatures();
				break;
			case modes::Mode::BALANCING:
				readBattery();
				readTempatures();
				balanceCells();
				generateLogLine();
				break;
			case modes::Mode::IDLE:
				readTempatures();
				readBattery();
				break;
			}

			if ((mode == modes::Mode::MONITORING || mode == modes::Mode::IDLE) && checkBatteryProblems())
			{
				digitalWrite(pins::ESP::CONTACTOR, LOW);
			}
		}

		// uint16_t logTime = 1000 * parameters.log_inter; // parameters.logSpeed in ms, convert to microseconds // unused?
		if (mode == modes::Mode::MONITORING && currentTime - lastSaveTime > parameters.log_inter)
		{
			lastSaveTime = currentTime;
			generateLogLine();
		}

		// integer division will floor the result, which is desired here
		uint32_t log_interval_iters = this->parameters.log_inter / t_battery::TASK_PERIOD_MS;
		if ((this->iters_without_log >= log_interval_iters) || this->new_faults)
		{
			q_logger::msg::LogLine msg = {};
			msg.timestamp = esp_timer_get_time();
			for (size_t i = 0; i < battery::IC_COUNT; i++)
			{
				memcpy(
					&msg.voltages[i * battery::CELL_COUNT_PER_IC],
					this->battery_data.ics[i].cell_voltages,
					sizeof(this->battery_data.ics[i].cell_voltages));
			}
			msg.temps = this->battery_data.temps;
			msg.current = this->battery_data.current;
			msg.faults = this->fault_manager.get_current_set_faults();
			xQueueSend(q_logger::g_logger_queue, &msg, portMAX_DELAY);

			this->iters_without_log = 0;
		}
		else
		{
			this->iters_without_log++;
		}
		// Check for user input
		check_debugging_input();
	}

	// ============== Battery reading and measurement functions ============== //
	void TBattery::readBattery()
	{
		wakeup_sleep(battery::IC_COUNT);
		clear_discharge(battery::IC_COUNT, bms_ic);

		// turn off bypass
		for (int current_ic = 0; current_ic < battery::IC_COUNT; current_ic++)
		{
			bool gpio[5] = {1, 1, 1, 1, 1};
			LTC681x_set_cfgr_gpio(current_ic, bms_ic, gpio);
		}

		LTC6811_wrcfg(battery::IC_COUNT, bms_ic);
		vTaskDelay(pdMS_TO_TICKS(100)); // allow the filters to settle
		printConfig();

		// Read from the battery management ICs and store in bms_ic
		measure();

		any_bypassed = false;
		// check if any are going to end up being bypassed before we start updating values
		if (parameters.bypass)
		{
			for (int current_ic = 0; current_ic < battery::IC_COUNT; current_ic++)
			{
				for (int i = 0; i < bms_ic[0].ic_reg.cell_channels; i++)
				{
					if (bms_ic[current_ic].cells.c_codes[i] * 0.0001 >= parameters.v_bypass)
					{
						any_bypassed = true;
						break;
					}
				}
			}
		}

		if (unlikely(any_bypassed))
		{
			return;
		}

		battery_data.min_voltage = bms_ic[0].cells.c_codes[0];
		battery_data.max_voltage = bms_ic[0].cells.c_codes[0];
		battery_data.sum_voltage = 0;

		for (int current_ic = 0; current_ic < battery::IC_COUNT; current_ic++)
		{
			for (int cell_num = 0; cell_num < bms_ic[0].ic_reg.cell_channels; cell_num++)
			{
				battery_data.sum_voltage += bms_ic[current_ic].cells.c_codes[cell_num];

				if (bms_ic[current_ic].cells.c_codes[cell_num] < battery_data.min_voltage)
				{
					battery_data.min_voltage = bms_ic[current_ic].cells.c_codes[cell_num];
				}

				if (bms_ic[current_ic].cells.c_codes[cell_num] > battery_data.max_voltage)
				{
					battery_data.max_voltage = bms_ic[current_ic].cells.c_codes[cell_num];
				}
			}
		}
		battery_data.avg_voltage = battery_data.sum_voltage / (battery::IC_COUNT * bms_ic[0].ic_reg.cell_channels);
	}

	void TBattery::measure()
	{
		int8_t error = 0;
		wakeup_idle(battery::IC_COUNT);
		LTC6811_adcv(2, 0, 0);
		LTC6811_pollAdc();
		wakeup_idle(battery::IC_COUNT);
		error = LTC6811_rdcv(0, battery::IC_COUNT, bms_ic);
		if (unlikely(error))
		{
			// Handle error (e.g., log it, set fault flags, etc.)
			printf("A PEC error was detected in the received data");
		}
	}
	void TBattery::balanceCells()
	{
		clear_discharge(battery::IC_COUNT, bms_ic);

		// Check balance temperatures and set balance thermal
		if (battery_data.temps.bal_bot > parameters.t_max_bal)
		{
			balTempBotTriggered = true;
		}
		else if (battery_data.temps.bal_bot < parameters.t_reset_bal)
		{
			balTempBotTriggered = false;
		}
		if (battery_data.temps.bal_top > parameters.t_max_bal)
		{
			balTempTopTriggered = true;
		}
		else if (battery_data.temps.bal_top < parameters.t_reset_bal)
		{
			balTempTopTriggered = false;
		}
		if (balTempBotTriggered || balTempTopTriggered)
		{
			fault_manager.set_fault(true, faults::WarningFault::BALANCE_THERMAL);
		}

		for (int current_ic = 0; current_ic < battery::IC_COUNT; current_ic++)
		{
			for (int cell_index = 0; cell_index < battery::CELL_COUNT_PER_IC; cell_index++)
			{
				battery_data.ics[current_ic].discharge[cell_index] = false;
			}

			if ((current_ic == 0 && balTempBotTriggered) ||
				(current_ic == 1 && balTempTopTriggered))
			{
				// bool gpio[5]; // TODO: should this be set to 0?
				// gpio[GPIO_BACKBAL] = 0;
				// LTC681x_set_cfgr_gpio(current_ic,bms_ic,gpio);
				continue;
			}

			for (int current_ic = 0; current_ic < battery::IC_COUNT; current_ic++)
			{
				for (int i = 0; i < battery::CELL_COUNT_PER_IC; i++)
				{
					battery_data.ics[current_ic].discharge[i] = false;
				}

				int sortedCells[12];
				for (int i = 0; i < bms_ic[0].ic_reg.cell_channels; i++)
				{
					sortedCells[i] = i;
				}

				battery::IcData *packToSort = &battery_data.ics[current_ic];
				std::sort(sortedCells, sortedCells + bms_ic[0].ic_reg.cell_channels,
						  [packToSort](int a, int b)
						  {
							  int val_a = packToSort->cell_voltages[a];
							  int val_b = packToSort->cell_voltages[b];
							  return val_a > val_b; // descending order
						  });

				printf("Cells largest to smalest: \n");

				printf("Cell: %d\n", sortedCells[0] + 1);
				for (int i = 0; i < bms_ic[0].ic_reg.cell_channels - 1; i++)
				{
					printf("Cell: %d\n", sortedCells[i + 1] + 1);

					if (sortedCells[i] == -1)
					{
						continue;
					}

					for (int cell = i; cell < 12 - 1; cell++)
					{
						// if cells are next to each other on the bms remove. no adjectent cells can be balanced at a time
						if (sortedCells[i] + 1 == sortedCells[cell + 1])
						{
							sortedCells[cell + 1] = -1;
						}

						if (sortedCells[i] - 1 == sortedCells[cell + 1])
						{
							sortedCells[cell + 1] = -1;
						}
					}
				}

				for (int i = 0, count = 0; count < MAX_BALANCE_COUNT && i < 12; i++)
				{
					if (sortedCells[i] != -1 &&
						(packToSort->cell_voltages[sortedCells[i]] > battery_data.ics[current_ic].avg_voltage + 0.001 / 0.0001 ||
						 packToSort->cell_voltages[sortedCells[i]] > battery_data.avg_voltage + 0.001 / 0.0001))
					{ // 0.01 V above average.
						printf("Discharging: %d\n", 12 * current_ic + sortedCells[i] + 1);
						LTC6811_set_discharge(12 * (1 - current_ic) + sortedCells[i] + 1, battery::IC_COUNT, bms_ic);
						for (int j = 0; j < battery::CELL_COUNT_PER_IC; j++)
						{
							battery_data.ics[current_ic].discharge[j] =
								(battery_data.ics[current_ic].discharge[j] || (sortedCells[i] == j));
						}
						count++;
					}
				}
			}
			wakeup_sleep(battery::IC_COUNT);
			LTC6811_wrcfg(battery::IC_COUNT, bms_ic);
			printConfig();
		}
	}

	void TBattery::readTempatures()
	{
		wakeup_idle(battery::IC_COUNT);
		LTC6811_adax(ADC_CONVERSION_MODE, AUX_CH_TO_CONVERT);
		LTC6811_pollAdc();
		wakeup_idle(battery::IC_COUNT);
		// vtaskDelay(pdMS_TO_TICKS(100));
		int error = LTC6811_rdaux(0, battery::IC_COUNT, bms_ic); // Set to read back all aux registers

		if (unlikely(error))
		{
			printf("A PEC error was detected in the received data in readTemperatures");
		}

		battery_data.temps.therms[0] = cellTemp(bms_ic[0].aux.a_codes[pins::LTC1::THERM1 - 1] * 0.0001);
		battery_data.temps.therms[1] = cellTemp(bms_ic[0].aux.a_codes[pins::LTC1::THERM2 - 1] * 0.0001);
		battery_data.temps.therms[2] = cellTemp(bms_ic[0].aux.a_codes[pins::LTC1::THERM3 - 1] * 0.0001);
		battery_data.temps.therms[3] = cellTemp(bms_ic[0].aux.a_codes[pins::LTC1::THERM4 - 1] * 0.0001);

		float fetV = bms_ic[1].aux.a_codes[pins::LTC2::THERM_FET - 1] * 0.0001;
		float balBotV = bms_ic[1].aux.a_codes[pins::LTC2::THERM_BAL_BOT - 1] * 0.0001;
		float balTopV = bms_ic[1].aux.a_codes[pins::LTC2::THERM_BAL_TOP - 1] * 0.0001;

		battery_data.temps.fet = steinhart((fetV * 10000) / (3 - fetV));
		battery_data.temps.bal_bot = steinhart((balBotV * 10000) / (3 - balBotV));
		battery_data.temps.bal_top = steinhart((balTopV * 10000) / (3 - balTopV));

		battery_data.current = convertCurrent(bms_ic[0].aux.a_codes[pins::LTC1::CURRENT - 1] * 0.0001);
	}

	float TBattery::convertCurrent(float voltage)
	{
		float adjusted_voltage = voltage - CURRENT_REF_OFFSET; // Adjust for error V offset
		float current = adjusted_voltage / (SHUNT_RESISTANCE * CURRENT_GAIN[gain_set]);

		// Adjust the gain based on the current
		adjustGain(voltage);

		return current;
	}

	void TBattery::setAmplifierGain()
	{
		bool gs0;
		bool gs1;
		switch (gain_set)
		{
		case 0:
			gs0 = LOW;
			gs1 = LOW;
			break;
		case 1:
			gs0 = LOW;
			gs1 = HIGH;
			break;
		case 2:
			gs0 = HIGH;
			gs1 = LOW;
			break;
		case 3:
			gs0 = HIGH;
			gs1 = HIGH;
		default:
			gs0 = HIGH;
			gs1 = HIGH;
			gain_set = 3;
			break;
		}
		digitalWrite(pins::ESP::GS0, gs0);
		digitalWrite(pins::ESP::GS1, gs1);

		return;
	}

	void TBattery::adjustGain(float voltage)
	{
		if (voltage < .2 || voltage > 2.8)
		{
			if (gain_set > 0)
			{
				gain_set -= 1;
				setAmplifierGain();
			}
		}
		else if (fabs(voltage - CURRENT_REF_OFFSET) < 0.1)
		{
			if (gain_set < 3)
			{
				gain_set += 1;
				setAmplifierGain();
			}
		}
	}

	float TBattery::steinhart(float R)
	{ // simplified Steinhart approximation with B = 3950

		float tempC;

		tempC = .003354 + (.000253165 * log(R / 10000));
		tempC = (1 / tempC) - 273.15;

		return tempC;
	}

	float TBattery::cellTemp(float voltage)
	{
		float Vin;
		float R;

		Vin = (0.614144 + voltage) / 1.359314; // other side of opamp
		R = (3.3 - Vin) * 4640 / Vin;		   // thermistor resistance

		return steinhart(R);
	}

	void TBattery::printConfig()
	{
		int cfg_pec;

		printf("Written Configuration: \n");
		for (int current_ic = 0; current_ic < battery::IC_COUNT; current_ic++)
		{
			printf(" IC IC %d: ", current_ic + 1);
			printf("0x%X", bms_ic[current_ic].config.tx_data[0]);
			printf(", 0x%X", bms_ic[current_ic].config.tx_data[1]);
			printf(", 0x%X", bms_ic[current_ic].config.tx_data[2]);
			printf(", 0x%X", bms_ic[current_ic].config.tx_data[3]);
			printf(", 0x%X", bms_ic[current_ic].config.tx_data[4]);
			printf(", 0x%X", bms_ic[current_ic].config.tx_data[5]);
			printf(", Calculated PEC: 0x");
			cfg_pec = pec15_calc(6, &bms_ic[current_ic].config.tx_data[0]);
			printf("%X", (uint8_t)(cfg_pec >> 8));
			printf(", 0x%X", (uint8_t)(cfg_pec));
			printf("\n");
		}
		printf("\n");
	}

	void TBattery::generateLogLine()
	{
		// Create a logline msg and send to logger
		q_logger::msg::LogLine msg = {};
		msg.timestamp = esp_timer_get_time();
		for (size_t i = 0; i < battery::IC_COUNT; i++)
		{
			memcpy(
				&msg.voltages[i * battery::CELL_COUNT_PER_IC],
				this->battery_data.ics[i].cell_voltages,
				sizeof(this->battery_data.ics[i].cell_voltages));
		}
		msg.temps = this->battery_data.temps;
		msg.mode = this->mode;
		msg.current = this->battery_data.current;
		msg.faults = this->fault_manager.get_current_set_faults();
		xQueueSend(q_logger::g_logger_queue, &msg, 0);
	}
} // namespace t_battery
