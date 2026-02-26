#include "util/serial.hpp"
#include "battery/t_battery.hpp"
#include "hardware/LTC/LTC6811.h"
#include "battery/t_battery.hpp"
#include "util/cmp.hpp"
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
#include "util/serial.hpp"
#include "hardware/pins.hpp"
#include "math.h"
#include "esp_littlefs.h"
#include <unistd.h>

#define ENABLED_VAL 1
#define DISABLED_VAL 0
#define DEC 10
#define TOTAL_IC battery::IC_COUNT

namespace t_battery
{

    // Nothing function??
    inline char *F(char *str)
    {
        return str;
    }

    inline void checkError(int error)
    {
        if (error == -1)
        {
            Serial::println(F("A PEC error was detected in the received data"));
        }
    }

    void TBattery::printOpen()
    {
        for (int current_ic = 0; current_ic < TOTAL_IC; current_ic++)
        {
            if (bms_ic[current_ic].system_open_wire == 0)
            {
                Serial::print("No Opens Detected on IC: ");
                Serial::print(current_ic + 1, DEC);
                Serial::println();
            }
            else
            {
                for (int cell = 0; cell < bms_ic[0].ic_reg.cell_channels + 1; cell++)
                {
                    if ((bms_ic[current_ic].system_open_wire & (1 << cell)) > 0)
                    {
                        Serial::print(F("There is an open wire on IC: "));
                        Serial::print(current_ic + 1, DEC);
                        Serial::print(F(" Channel: "));
                        Serial::print(cell, DEC);
                        Serial::println("");
                    }
                }
            }
        }
    }

    void TBattery::printAux(uint8_t datalog_en)
    {
        for (int current_ic = 0; current_ic < TOTAL_IC; current_ic++)
        {
            if (datalog_en == DISABLED_VAL)
            {
                Serial::print(" IC ");
                Serial::print(current_ic + 1, DEC);
                for (int i = 0; i < 5; i++)
                {
                    Serial::print(F(" GPIO-"));
                    Serial::print(i + 1, DEC);
                    Serial::print(":");
                    Serial::print(bms_ic[current_ic].aux.a_codes[i] * 0.0001, 4);
                    Serial::print(",");
                }
                Serial::print(F(" Vref2"));
                Serial::print(":");
                Serial::print(bms_ic[current_ic].aux.a_codes[5] * 0.0001, 4);
                Serial::println();
            }
            else
            {
                Serial::print("AUX, ");

                for (int i = 0; i < 6; i++)
                {
                    Serial::print(bms_ic[current_ic].aux.a_codes[i] * 0.0001, 4);
                    Serial::print(",");
                }
            }
        }
        Serial::println();
    }

    void TBattery::printStat()
    {
        for (int current_ic = 0; current_ic < TOTAL_IC; current_ic++)
        {
            Serial::print(F(" IC "));
            Serial::print(current_ic + 1, DEC);
            Serial::print(F(" SOC:"));
            Serial::print(bms_ic[current_ic].stat.stat_codes[0] * 0.0001 * 20, 4);
            Serial::print(F(","));
            Serial::print(F(" Itemp:"));
            Serial::print(bms_ic[current_ic].stat.stat_codes[1] * 0.0001, 4);
            Serial::print(F(","));
            Serial::print(F(" VregA:"));
            Serial::print(bms_ic[current_ic].stat.stat_codes[2] * 0.0001, 4);
            Serial::print(F(","));
            Serial::print(F(" VregD:"));
            Serial::print(bms_ic[current_ic].stat.stat_codes[3] * 0.0001, 4);
            Serial::println();
        }

        Serial::println();
    }

    void TBattery::printRxConfig()
    {
        Serial::println(F("Received Configuration "));
        for (int current_ic = 0; current_ic < TOTAL_IC; current_ic++)
        {
            Serial::print(F(" IC "));
            Serial::print(current_ic + 1, DEC);
            Serial::print(F(": 0x"));
            Serial::print(bms_ic[current_ic].config.rx_data[0], 16);
            Serial::print(F(", 0x"));
            Serial::print(bms_ic[current_ic].config.rx_data[1], 16);
            Serial::print(F(", 0x"));
            Serial::print(bms_ic[current_ic].config.rx_data[2], 16);
            Serial::print(F(", 0x"));
            Serial::print(bms_ic[current_ic].config.rx_data[3], 16);
            Serial::print(F(", 0x"));
            Serial::print(bms_ic[current_ic].config.rx_data[4], 16);
            Serial::print(F(", 0x"));
            Serial::print(bms_ic[current_ic].config.rx_data[5], 16);
            Serial::print(F(", Received PEC: 0x"));
            Serial::print(bms_ic[current_ic].config.rx_data[6], 16);
            Serial::print(F(", 0x"));
            Serial::print(bms_ic[current_ic].config.rx_data[7], 16);
            Serial::println();
        }
        Serial::println();
    }

    void TBattery::printPec()
    {
        for (int current_ic = 0; current_ic < TOTAL_IC; current_ic++)
        {
            Serial::println("");
            Serial::print(bms_ic[current_ic].crc_count.pec_count, DEC);
            Serial::print(F(" : PEC Errors Detected on IC"));
            Serial::print(current_ic + 1, DEC);
            Serial::println("");
        }
    }

    void TBattery::printCells(uint8_t datalog_en)
    {
        for (int current_ic = 0; current_ic < battery::IC_COUNT; current_ic++)
        {
            if (datalog_en == 0)
            {
                Serial::print(" IC ");
                Serial::print(current_ic + 1, DEC);
                Serial::print(", ");
                for (int i = 0; i < battery::CELL_COUNT_PER_IC; i++)
                {
                    Serial::print(" C");
                    Serial::print(i + 1, DEC);
                    Serial::print(":");
                    Serial::print(this->battery_data.ics[current_ic].cell_voltages[i] * 0.0001, 4);
                    Serial::print(",");
                }
                Serial::println();
            }
            else
            {
                Serial::print("Cells, ");
                for (int i = 0; i < battery::CELL_COUNT_PER_IC; i++)
                {
                    Serial::print(this->battery_data.ics[current_ic].cell_voltages[i] * 0.0001, 4);
                    Serial::print(",");
                }
            }
        }
        Serial::println();
    }

    void TBattery::printMenu()
    {
        Serial::println(F("Please enter LTC6811 Command"));
        Serial::println(F("Write Configuration: 1            | Reset PEC Counter: 11 "));
        Serial::println(F("Read Configuration: 2             | Run ADC Self Test: 12"));
        Serial::println(F("Start Cell Voltage Conversion: 3  | Set Discharge: 13"));
        Serial::println(F("Read Cell Voltages: 4             | Clear Discharge: 14"));
        Serial::println(F("Start Aux Voltage Conversion: 5   | Clear Registers: 15"));
        Serial::println(F("Read Aux Voltages: 6              | Run Mux Self Test: 16"));
        Serial::println(F("Start Stat Voltage Conversion: 7  | Run ADC overlap Test: 17"));
        Serial::println(F("Read Stat Voltages: 8             | Run Digital Redundancy Test: 18"));
        Serial::println(F("loop Measurements: 9              | Run Open Wire Test: 19"));
        Serial::println(F("Read PEC Errors: 10               |  Loop measurements with datalog output: 20"));
        Serial::println(F("States, MONITOR: 21, Charging: 22, Delete Datastore: 30"));
        Serial::println(F("Please enter command: "));
        Serial::println("");
    }

    void TBattery::check_debugging_input()
    {
        if (unlikely(Serial::available()))
        {
            uint32_t user_command;
            user_command = Serial::read_int(); // Read the user command
            Serial::print(user_command, 10);
            Serial::println("");

            runCommand(user_command);
        }
    }

    void TBattery::runCommand(uint32_t cmd)
    {
        int8_t error = 0;
        uint32_t conv_time = 0;
        int8_t readIC = 0;
        char input = 0;

        // legacy conversion variables

        switch (cmd)
        {
        case 1: // Write Configuration Register
            wakeup_sleep(TOTAL_IC);
            LTC6811_wrcfg(TOTAL_IC, bms_ic);
            printConfig();
            break;

        case 2: // Read Configuration Register
            wakeup_sleep(TOTAL_IC);
            error = LTC6811_rdcfg(TOTAL_IC, bms_ic);
            checkError(error);
            printRxConfig();
            break;

        case 3: // Start Cell ADC Measurement
            wakeup_sleep(TOTAL_IC);
            LTC6811_adcv(ADC_CONVERSION_MODE, ADC_DCP, CELL_CH_TO_CONVERT);
            conv_time = LTC6811_pollAdc();
            Serial::print(F("cell conversion completed in:"));
            Serial::print(((float)conv_time / 1000), 1);
            Serial::println(F("mS"));
            Serial::println("");
            break;

        case 4: // Read Cell Voltage Registers
            wakeup_sleep(TOTAL_IC);
            error = LTC6811_rdcv(0, TOTAL_IC, bms_ic); // Set to read back all cell voltage registers
            checkError(error);
            printCells(DISABLED_VAL);
            break;

        case 5: // Start GPIO ADC Measurement
            wakeup_sleep(TOTAL_IC);
            LTC6811_adax(ADC_CONVERSION_MODE, AUX_CH_TO_CONVERT);
            LTC6811_pollAdc();
            Serial::println(F("aux conversion completed"));
            Serial::println("");
            break;

        case 6: // Read AUX Voltage Registers
            wakeup_sleep(TOTAL_IC);
            error = LTC6811_rdaux(0, TOTAL_IC, bms_ic); // Set to read back all aux registers
            checkError(error);

            printAux(DISABLED_VAL);
            break;

        case 7: // Start Status ADC Measurement
            wakeup_sleep(TOTAL_IC);
            LTC6811_adstat(ADC_CONVERSION_MODE, STAT_CH_TO_CONVERT);
            LTC6811_pollAdc();
            Serial::println(F("stat conversion completed"));
            Serial::println("");
            break;

        case 8: // Read Status registers
            wakeup_sleep(TOTAL_IC);
            error = LTC6811_rdstat(0, TOTAL_IC, bms_ic); // Set to read back all aux registers
            checkError(error);
            printStat();
            break;

        case 9: // Loop Measurements
            Serial::println("transmit 'm' to quit");
            wakeup_sleep(TOTAL_IC);
            LTC6811_wrcfg(TOTAL_IC, bms_ic);
            while (input != 'm')
            {
                if (Serial::available() > 0)
                {
                    input = Serial::getChar();
                }

                measure();

                vTaskDelay(pdMS_TO_TICKS(MEASUREMENT_LOOP_TIME));
            }
            // printMenu();
            break;

        case 10: // Run open wire self test
            printPec();

            break;

        case 11: // Read in raw configuration data
            LTC6811_reset_crc_count(TOTAL_IC, bms_ic);
            break;

        case 12: // Run the ADC/Memory Self Test
            wakeup_sleep(TOTAL_IC);
            error = LTC6811_run_cell_adc_st(CELL, ADC_CONVERSION_MODE, bms_ic);
            Serial::print(error, DEC);
            Serial::println(F(" : errors detected in Digital Filter and CELL Memory \n"));

            wakeup_sleep(TOTAL_IC);
            error = LTC6811_run_cell_adc_st(AUX, ADC_CONVERSION_MODE, bms_ic);
            Serial::print(error, DEC);
            Serial::println(F(" : errors detected in Digital Filter and AUX Memory \n"));

            wakeup_sleep(TOTAL_IC);
            error = LTC6811_run_cell_adc_st(STAT, ADC_CONVERSION_MODE, bms_ic);
            Serial::print(error, DEC);
            Serial::println(F(" : errors detected in Digital Filter and STAT Memory \n"));
            printMenu();
            break;

        case 13: // Enable a discharge transistor
            Serial::println(F("Please enter the Spin number"));
            readIC = (int8_t)Serial::read_int();
            LTC6811_set_discharge(readIC, TOTAL_IC, bms_ic);
            wakeup_sleep(TOTAL_IC);
            LTC6811_wrcfg(TOTAL_IC, bms_ic);
            printConfig();
            break;

        case 14: // Clear all discharge transistors
            clear_discharge(TOTAL_IC, bms_ic);
            wakeup_sleep(TOTAL_IC);
            LTC6811_wrcfg(TOTAL_IC, bms_ic);
            printConfig();
            break;

        case 15: // Clear all ADC measurement registers
            wakeup_sleep(TOTAL_IC);
            LTC6811_clrcell();
            LTC6811_clraux();
            LTC6811_clrstat();
            Serial::println(F("All Registers Cleared"));
            break;

        case 16: // Run the Mux Decoder Self Test
            wakeup_sleep(TOTAL_IC);
            LTC6811_diagn();
            vTaskDelay(pdMS_TO_TICKS(5));
            error = LTC6811_rdstat(0, TOTAL_IC, bms_ic); // Set to read back all aux registers
            checkError(error);
            error = 0;
            for (int ic = 0; ic < TOTAL_IC; ic++)
            {
                if (bms_ic[ic].stat.mux_fail[0] != 0)
                    error++;
            }
            if (error == 0)
                Serial::println(F("Mux Test: PASS "));
            else
                Serial::println(F("Mux Test: FAIL "));

            break;

        case 17: // Run ADC Overlap self test
            wakeup_sleep(TOTAL_IC);
            error = (int8_t)LTC6811_run_adc_overlap(TOTAL_IC, bms_ic);
            if (error == 0)
                Serial::println(F("Overlap Test: PASS "));
            else
                Serial::println(F("Overlap Test: FAIL"));
            break;

        case 18: // Run ADC Redundancy self test
            wakeup_sleep(TOTAL_IC);
            error = LTC6811_run_adc_redundancy_st(ADC_CONVERSION_MODE, AUX, TOTAL_IC, bms_ic);
            Serial::print(error, DEC);
            Serial::println(F(" : errors detected in AUX Measurement \n"));

            wakeup_sleep(TOTAL_IC);
            error = LTC6811_run_adc_redundancy_st(ADC_CONVERSION_MODE, STAT, TOTAL_IC, bms_ic);
            Serial::print(error, DEC);
            Serial::println(F(" : errors detected in STAT Measurement \n"));
            break;

        case 19:
            LTC6811_run_openwire(TOTAL_IC, bms_ic);
            printOpen();
            break;

        case 20: // Datalog print option Loop Measurements
            Serial::println(F("transmit 'm' to quit"));
            wakeup_sleep(TOTAL_IC);
            LTC6811_wrcfg(TOTAL_IC, bms_ic);
            while (input != 'm')
            {
                if (Serial::available() > 0)
                {
                    input = Serial::getChar();
                }

                measure();

                vTaskDelay(pdMS_TO_TICKS(MEASUREMENT_LOOP_TIME));
            }
            printMenu();
            break;

        case 'm': // prints menu
            printMenu();
            break;

        case 21:
            mode = modes::Mode::MONITORING;
            break;
        case 22:
            mode = modes::Mode::BALANCING;
            break;

        case 30:
            Serial::printf("Deleting file: %s\r\n", "/log.csv");
            if (unlink("/littlefs/log.csv")) // Use unlink to delete the file from LittleFS
            {
                Serial::println("- file deleted");
            }
            else
            {
                Serial::println("- delete failed");
            }

            break;

        default:
            Serial::println(F("Incorrect Option"));
            break;
        }
    }

    bool TBattery::checkBatteryProblems()
    {
        // Check temperature difference
        tDiffTriggered = false;

        const int TEMPS_COUNT = sizeof(battery_data.temps.therms) / sizeof(battery_data.temps.therms[0]);
        for (int i = 0; i < TEMPS_COUNT; i++)
        {
            for (int j = i + 1; j < TEMPS_COUNT; j++)
            { // Avoid redundant comparisons
                if (abs(battery_data.temps.therms[i] - battery_data.temps.therms[j]) > parameters.t_diff)
                {
                    tDiffTriggered = true;
                }
            }
        }

        float minCurrent = (mode == modes::Mode::BALANCING) ? this->parameters.i_min : this->parameters.i_min_regen;
        // Cell voltage
        if (unlikely(this->battery_data.min_voltage < this->parameters.v_min))
        {
            fault_manager.set_fault(true, faults::PersistentFault::CELL_UNDERVOLTAGE);
        }
        if (unlikely(this->battery_data.max_voltage > this->parameters.v_max))
        {
            fault_manager.set_fault(true, faults::PersistentFault::CELL_OVERVOLTAGE);
        }

        // Average voltage
        if (unlikely(this->battery_data.avg_voltage > this->parameters.v_max_avg))
        {
            fault_manager.set_fault(true, faults::PersistentFault::BATTERY_OVERVOLTAGE);
        }
        if (unlikely(this->battery_data.avg_voltage < this->parameters.v_min_avg))
        {
            fault_manager.set_fault(true, faults::PersistentFault::BATTERY_UNDERVOLTAGE);
        }

        // Voltage difference
        if (unlikely(!util::check_difference(this->battery_data.max_voltage, this->battery_data.min_voltage, this->parameters.v_diff)))
        {
            fault_manager.set_fault(true, faults::PersistentFault::BATTERY_VOLTAGE_IMBALANCE);
        }

        // Temprature s
        if (unlikely(!util::check_within(this->battery_data.temps.therms[0], this->parameters.t_min, this->parameters.t_max)))
        {
            fault_manager.set_fault(true, faults::PersistentFault::TEMP_0);
        }
        if (unlikely(!util::check_within(this->battery_data.temps.therms[1], this->parameters.t_min, this->parameters.t_max)))
        {
            fault_manager.set_fault(true, faults::PersistentFault::TEMP_1);
        }
        if (unlikely(!util::check_within(this->battery_data.temps.therms[2], this->parameters.t_min, this->parameters.t_max)))
        {
            fault_manager.set_fault(true, faults::PersistentFault::TEMP_2);
        }
        if (unlikely(!util::check_within(this->battery_data.temps.therms[3], this->parameters.t_min, this->parameters.t_max)))
        {
            fault_manager.set_fault(true, faults::PersistentFault::TEMP_3);
        }

        // Current
        if (unlikely(this->battery_data.current > this->parameters.i_max))
        {
            fault_manager.set_fault(true, faults::PersistentFault::OVERCURRENT);
        }
        if (unlikely(this->battery_data.current < minCurrent))
        {
            fault_manager.set_fault(true, faults::PersistentFault::UNDERCURRENT);
        }

        // Max power
        if (unlikely((this->battery_data.avg_voltage * this->battery_data.current) > this->parameters.p_max))
        {
            fault_manager.set_fault(true, faults::WarningFault::OVERPOWER);
        }

        // battery.faults.coreZeroWatch is set in loop()

        // pfaults removed due to being redundant with the fault manager's internal state
        bool problems = fault_manager.get_persistent_faults() != 0;
        // Not overPower
        // Not coreZeroWatch

        if (problems || fault_manager.get_current_fault(faults::WarningFault::OVERPOWER))
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
            xQueueSend(q_logger::g_logger_queue, &msg, 0);
        }

        return problems;
    }
}