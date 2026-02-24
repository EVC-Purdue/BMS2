#include "util/serial.hpp"
#include "battery/t_battery.hpp"
#include "hardware/LTC/LTC6811.h"
#include "battery/t_battery.hpp"

namespace t_battery
{
    void TBattery::check_debugging_input()
    {
        if (unlikely(Serial::available()))
        {
            uint32_t user_command;
            user_command = Serial::read_int(); // Read the user command
            Serial::println(user_command);

            xSemaphoreTake(xMutex, portMAX_DELAY);
            runCommand(user_command);
            xSemaphoreGive(xMutex);
        }
    }

    void TBattery::runCommand(uint32_t cmd)
    {
        int8_t error = 0;
        uint32_t conv_time = 0;
        uint32_t user_command;
        int8_t readIC = 0;
        char input = 0;

        // legacy conversion variables
        const uint8_t TOTAL_IC = battery::IC_COUNT;
        const int DEC = 10;

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

                measurementLoop(DISABLED_VAL);

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

                measurementLoop(ENABLED_VAL);

                vTaskDelay(pdMS_TO_TICKS(MEASUREMENT_LOOP_TIME));
            }
            printMenu();
            break;

        case 'm': // prints menu
            printMenu();
            break;

        case 21:
            mode = modes::Mode::MONITOR;
            break;
        case 22:
            mode = modes::Mode::BALANCING;
            break;

        case 30:
            Serial::printf("Deleting file: %s\r\n", "/log.csv");
            if (SPIFFS.remove("/log.csv"))
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

    // Nothing function??
    char *F(char *str)
    {
        return str;
    }
}