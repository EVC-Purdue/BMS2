#include "battery/t_battery.hpp"
#include "hardware/LTC/LTC6811.h"
#include "battery/t_battery.hpp"
#include "util/cmp.hpp"
#include <cstdint>
#include <cstring>
#include <optional>
#include <algorithm>
#include <inttypes.h>
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
#include "hardware/pins.hpp"
#include "math.h"
#include "esp_littlefs.h"
#include <unistd.h>
#include "t_battery.hpp"
#include <stdio.h>
#include <sys/select.h>
#include <fcntl.h>

#define ENABLED 1
#define DISABLED 0

namespace t_battery
{
    // Helper function to check if input is available
    static bool console_available() {
        struct timeval tv = {0, 0};
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
    }

    // Helper function to read an integer (blocking until newline)
    static int console_read_int() {
        char buf[32];
        int idx = 0;
        int c;
        // Simple line reader
        while(idx < sizeof(buf)-1) {
            c = fgetc(stdin);
            if(c == EOF) {
                 vTaskDelay(pdMS_TO_TICKS(10));
                 continue; 
            }
            if(c == '\n' || c == '\r') {
                if (idx == 0 && c == '\r') continue; // consume CR if empty?
                if (idx > 0) break;
                // if idx is 0, we might have just hit enter without typing number? 
                // Original code was tricky. Let's assume user types number then enter.
                break; 
            }
            buf[idx++] = (char)c;
        }
        buf[idx] = 0;
        return atoi(buf);
    }
    
    // Helper function to read a char
    static char console_getChar() {
        int c = fgetc(stdin);
        if (c == EOF) return 0;
        return (char)c;
    }

    inline void checkError(int error)
    {
        if (error == -1)
        {
            printf("A PEC error was detected in the received data\n");
        }
    }

    void TBattery::printOpen()
    {
        for (int current_ic = 0; current_ic < battery::IC_COUNT; current_ic++)
        {
            if (bms_ic[current_ic].system_open_wire == 0)
            {
                printf("No Opens Detected on IC: ");
                printf("%d", current_ic + 1);
                printf("\n");
            }
            else
            {
                for (int cell = 0; cell < bms_ic[0].ic_reg.cell_channels + 1; cell++)
                {
                    if ((bms_ic[current_ic].system_open_wire & (1 << cell)) > 0)
                    {
                        printf("There is an open wire on IC: ");
                        printf("%d", current_ic + 1);
                        printf(" Channel: ");
                        printf("%d", cell);
                        printf("\n");
                    }
                }
            }
        }
    }

    void TBattery::printAux(uint8_t datalog_en)
    {
        for (int current_ic = 0; current_ic < battery::IC_COUNT; current_ic++)
        {
            if (datalog_en == DISABLED)
            {
                printf(" IC ");
                printf("%d", current_ic + 1);
                for (int i = 0; i < 5; i++)
                {
                    printf(" GPIO-");
                    printf("%d", i + 1);
                    printf(":");
                    printf("%.4f", bms_ic[current_ic].aux.a_codes[i] * 0.0001);
                    printf(",");
                }
                printf(" Vref2");
                printf(":");
                printf("%.4f", bms_ic[current_ic].aux.a_codes[5] * 0.0001);
                printf("\n");
            }
            else
            {
                printf("AUX, ");

                for (int i = 0; i < 6; i++)
                {
                    printf("%.4f", bms_ic[current_ic].aux.a_codes[i] * 0.0001);
                    printf(",");
                }
            }
        }
        printf("\n");
    }

    void TBattery::printStat()
    {
        for (int current_ic = 0; current_ic < battery::IC_COUNT; current_ic++)
        {
            printf(" IC ");
            printf("%d", current_ic + 1);
            printf(" SOC:");
            printf("%.4f", bms_ic[current_ic].stat.stat_codes[0] * 0.0001 * 20);
            printf(",");
            printf(" Itemp:");
            printf("%.4f", bms_ic[current_ic].stat.stat_codes[1] * 0.0001);
            printf(",");
            printf(" VregA:");
            printf("%.4f", bms_ic[current_ic].stat.stat_codes[2] * 0.0001);
            printf(",");
            printf(" VregD:");
            printf("%.4f", bms_ic[current_ic].stat.stat_codes[3] * 0.0001);
            printf("\n");
        }

        printf("\n");
    }

    void TBattery::printRxConfig()
    {
        printf("Received Configuration \n");
        for (int current_ic = 0; current_ic < battery::IC_COUNT; current_ic++)
        {
            printf(" IC ");
            printf("%d", current_ic + 1);
            printf(": 0x");
            printf("%X", bms_ic[current_ic].config.rx_data[0]);
            printf(", 0x");
            printf("%X", bms_ic[current_ic].config.rx_data[1]);
            printf(", 0x");
            printf("%X", bms_ic[current_ic].config.rx_data[2]);
            printf(", 0x");
            printf("%X", bms_ic[current_ic].config.rx_data[3]);
            printf(", 0x");
            printf("%X", bms_ic[current_ic].config.rx_data[4]);
            printf(", 0x");
            printf("%X", bms_ic[current_ic].config.rx_data[5]);
            printf(", Received PEC: 0x");
            printf("%X", bms_ic[current_ic].config.rx_data[6]);
            printf(", 0x");
            printf("%X", bms_ic[current_ic].config.rx_data[7]);
            printf("\n");
        }
        printf("\n");
    }

    void TBattery::printPec()
    {
        for (int current_ic = 0; current_ic < battery::IC_COUNT; current_ic++)
        {
            printf("\n");
            printf("%d", bms_ic[current_ic].crc_count.pec_count);
            printf(" : PEC Errors Detected on IC");
            printf("%d", current_ic + 1);
            printf("\n");
        }
    }

    void TBattery::printCells(uint8_t datalog_en)
    {
        for (int current_ic = 0; current_ic < battery::IC_COUNT; current_ic++)
        {
            if (datalog_en == 0)
            {
                printf(" IC ");
                printf("%d", current_ic + 1);
                printf(", ");
                for (int i = 0; i < battery::CELL_COUNT_PER_IC; i++)
                {
                    printf(" C");
                    printf("%d", i + 1);
                    printf(":");
                    printf("%.4f", this->battery_data.ics[current_ic].cell_voltages[i] * 0.0001);
                    printf(",");
                }
                printf("\n");
            }
            else
            {
                printf("Cells, ");
                for (int i = 0; i < battery::CELL_COUNT_PER_IC; i++)
                {
                    printf("%.4f", this->battery_data.ics[current_ic].cell_voltages[i] * 0.0001);
                    printf(",");
                }
            }
        }
        printf("\n");
    }

    void TBattery::printMenu()
    {
        printf("Please enter LTC6811 Command\n");
        printf("Write Configuration: 1            | Reset PEC Counter: 11 \n");
        printf("Read Configuration: 2             | Run ADC Self Test: 12\n");
        printf("Start Cell Voltage Conversion: 3  | Set Discharge: 13\n");
        printf("Read Cell Voltages: 4             | Clear Discharge: 14\n");
        printf("Start Aux Voltage Conversion: 5   | Clear Registers: 15\n");
        printf("Read Aux Voltages: 6              | Run Mux Self Test: 16\n");
        printf("Start Stat Voltage Conversion: 7  | Run ADC overlap Test: 17\n");
        printf("Read Stat Voltages: 8             | Run Digital Redundancy Test: 18\n");
        printf("loop Measurements: 9              | Run Open Wire Test: 19\n");
        printf("Read PEC Errors: 10               |  Loop measurements with datalog output: 20\n");
        printf("States, MONITOR: 21, Charging: 22, Delete Datastore: 30\n");
        printf("Please enter command: \n");
        printf("\n");
    }

    void TBattery::check_debugging_input()
    {
        if (unlikely(console_available()))
        {
            uint32_t user_command;
            user_command = console_read_int(); // Read the user command
            printf("%" PRIu32, user_command);
            printf("\n");

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
            wakeup_sleep(battery::IC_COUNT);
            LTC6811_wrcfg(battery::IC_COUNT, bms_ic);
            printConfig();
            break;

        case 2: // Read Configuration Register
            wakeup_sleep(battery::IC_COUNT);
            error = LTC6811_rdcfg(battery::IC_COUNT, bms_ic);
            checkError(error);
            printRxConfig();
            break;

        case 3: // Start Cell ADC Measurement
            wakeup_sleep(battery::IC_COUNT);
            LTC6811_adcv(ADC_CONVERSION_MODE, ADC_DCP, CELL_CH_TO_CONVERT);
            conv_time = LTC6811_pollAdc();
            printf("cell conversion completed in:");
            printf("%.1f", ((float)conv_time / 1000));
            printf("mS\n");
            printf("\n");
            break;

        case 4: // Read Cell Voltage Registers
            wakeup_sleep(battery::IC_COUNT);
            error = LTC6811_rdcv(0, battery::IC_COUNT, bms_ic); // Set to read back all cell voltage registers
            checkError(error);
            printCells(DISABLED);
            break;

        case 5: // Start GPIO ADC Measurement
            wakeup_sleep(battery::IC_COUNT);
            LTC6811_adax(ADC_CONVERSION_MODE, AUX_CH_TO_CONVERT);
            LTC6811_pollAdc();
            printf("aux conversion completed\n");
            printf("\n");
            break;

        case 6: // Read AUX Voltage Registers
            wakeup_sleep(battery::IC_COUNT);
            error = LTC6811_rdaux(0, battery::IC_COUNT, bms_ic); // Set to read back all aux registers
            checkError(error);

            printAux(DISABLED);
            break;

        case 7: // Start Status ADC Measurement
            wakeup_sleep(battery::IC_COUNT);
            LTC6811_adstat(ADC_CONVERSION_MODE, STAT_CH_TO_CONVERT);
            LTC6811_pollAdc();
            printf("stat conversion completed\n");
            printf("\n");
            break;

        case 8: // Read Status registers
            wakeup_sleep(battery::IC_COUNT);
            error = LTC6811_rdstat(0, battery::IC_COUNT, bms_ic); // Set to read back all aux registers
            checkError(error);
            printStat();
            break;

        case 9: // Loop Measurements
            printf("transmit 'm' to quit\n");
            wakeup_sleep(battery::IC_COUNT);
            LTC6811_wrcfg(battery::IC_COUNT, bms_ic);
            while (input != 'm')
            {
                if (console_available() > 0)
                {
                    input = console_getChar();
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
            LTC6811_reset_crc_count(battery::IC_COUNT, bms_ic);
            break;

        case 12: // Run the ADC/Memory Self Test
            wakeup_sleep(battery::IC_COUNT);
            error = LTC6811_run_cell_adc_st(CELL, ADC_CONVERSION_MODE, bms_ic);
            printf("%d", error);
            printf(" : errors detected in Digital Filter and CELL Memory \n\n");

            wakeup_sleep(battery::IC_COUNT);
            error = LTC6811_run_cell_adc_st(AUX, ADC_CONVERSION_MODE, bms_ic);
            printf("%d", error);
            printf(" : errors detected in Digital Filter and AUX Memory \n\n");

            wakeup_sleep(battery::IC_COUNT);
            error = LTC6811_run_cell_adc_st(STAT, ADC_CONVERSION_MODE, bms_ic);
            printf("%d", error);
            printf(" : errors detected in Digital Filter and STAT Memory \n\n");
            printMenu();
            break;

        case 13: // Enable a discharge transistor
            printf("Please enter the Spin number\n");
            readIC = (int8_t)console_read_int();
            LTC6811_set_discharge(readIC, battery::IC_COUNT, bms_ic);
            wakeup_sleep(battery::IC_COUNT);
            LTC6811_wrcfg(battery::IC_COUNT, bms_ic);
            printConfig();
            break;

        case 14: // Clear all discharge transistors
            clear_discharge(battery::IC_COUNT, bms_ic);
            wakeup_sleep(battery::IC_COUNT);
            LTC6811_wrcfg(battery::IC_COUNT, bms_ic);
            printConfig();
            break;

        case 15: // Clear all ADC measurement registers
            wakeup_sleep(battery::IC_COUNT);
            LTC6811_clrcell();
            LTC6811_clraux();
            LTC6811_clrstat();
            printf("All Registers Cleared\n");
            break;

        case 16: // Run the Mux Decoder Self Test
            wakeup_sleep(battery::IC_COUNT);
            LTC6811_diagn();
            vTaskDelay(pdMS_TO_TICKS(5));
            error = LTC6811_rdstat(0, battery::IC_COUNT, bms_ic); // Set to read back all aux registers
            checkError(error);
            error = 0;
            for (int ic = 0; ic < battery::IC_COUNT; ic++)
            {
                if (bms_ic[ic].stat.mux_fail[0] != 0)
                    error++;
            }
            if (error == 0)
                printf("Mux Test: PASS \n");
            else
                printf("Mux Test: FAIL \n");

            break;

        case 17: // Run ADC Overlap self test
            wakeup_sleep(battery::IC_COUNT);
            error = (int8_t)LTC6811_run_adc_overlap(battery::IC_COUNT, bms_ic);
            if (error == 0)
                printf("Overlap Test: PASS \n");
            else
                printf("Overlap Test: FAIL\n");
            break;

        case 18: // Run ADC Redundancy self test
            wakeup_sleep(battery::IC_COUNT);
            error = LTC6811_run_adc_redundancy_st(ADC_CONVERSION_MODE, AUX, battery::IC_COUNT, bms_ic);
            printf("%d", error);
            printf(" : errors detected in AUX Measurement \n\n");

            wakeup_sleep(battery::IC_COUNT);
            error = LTC6811_run_adc_redundancy_st(ADC_CONVERSION_MODE, STAT, battery::IC_COUNT, bms_ic);
            printf("%d", error);
            printf(" : errors detected in STAT Measurement \n\n");
            break;

        case 19:
            LTC6811_run_openwire(battery::IC_COUNT, bms_ic);
            printOpen();
            break;

        case 20: // Datalog print option Loop Measurements
            printf("transmit 'm' to quit\n");
            wakeup_sleep(battery::IC_COUNT);
            LTC6811_wrcfg(battery::IC_COUNT, bms_ic);
            while (input != 'm')
            {
                if (console_available() > 0)
                {
                    input = console_getChar();
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
            printf("Deleting file: %s\r\n", "/littlefs/log.csv");
            if (unlink("/littlefs/log.csv")) // Use unlink to delete the file from LittleFS
            {
                printf("- file deleted\n");
            }
            else
            {
                printf("- delete failed\n");
            }

            break;

        default:
            printf("Incorrect Option\n");
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
            generateLogLine();
        }

        return problems;
    }
}
