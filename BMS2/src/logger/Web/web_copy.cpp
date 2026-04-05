#if true // disable entire file for now, as it's not being used and is incomplete

#include "logger/Web/web_copy.hpp"
#include "stdint.h"
#include "battery/battery.hpp"
#include "battery/parameters.hpp"
#include "esp_http_server.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include "stdio.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_littlefs.h"
#include "esp_partition.h"

#include "battery/faults.hpp"
#include "battery/q_battery.hpp"
#include "battery/state_cache.hpp"
#include "hardware/CAN.hpp"
#include "hardware/gpio.hpp"
#include "hardware/pins.hpp"
#include "logger/q_logger.hpp"
#include "hardware/hardware.hpp"

namespace web
{
    static httpd_handle_t s_server = nullptr;

    static const char *TAG = "web_idf";
    constexpr const char *LOG_FILE = "/littlefs/log.csv";
    constexpr const char *HTML_FILE = "/littlefs/index.html";
    constexpr const char *BMS_NAME = "bms";
    constexpr size_t MAX_UPLOAD_SIZE = 512U * 1024U;

    bool parse_bool(const std::string &value, bool &out)
    {
        if (value == "true" || value == "1" || value == "on")
        {
            out = true;
            return true;
        }
        if (value == "false" || value == "0" || value == "off")
        {
            out = false;
            return true;
        }
        return false;
    }

    bool parse_float(const std::string &value, float &out)
    {
        if (value.empty())
        {
            return false;
        }

        char *end = nullptr;
        errno = 0;
        const float parsed = std::strtof(value.c_str(), &end);
        if (errno != 0 || end == value.c_str() || (end != nullptr && *end != '\0'))
        {
            return false;
        }

        out = parsed;
        return true;
    }

    bool parse_u32(const std::string &value, uint32_t &out)
    {
        if (value.empty())
        {
            return false;
        }

        char *end = nullptr;
        errno = 0;
        const unsigned long parsed = std::strtoul(value.c_str(), &end, 10);
        if (errno != 0 || end == value.c_str() || (end != nullptr && *end != '\0'))
        {
            return false;
        }

        out = static_cast<uint32_t>(parsed);
        return true;
    }

    esp_err_t setupServer()
    {
        if (s_server != nullptr)
        {
            return ESP_OK;
        }

        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.uri_match_fn = httpd_uri_match_wildcard;

        ESP_RETURN_ON_ERROR(httpd_start(&s_server, &config), TAG, "httpd start failed");

        const httpd_uri_t routes[] = {
            {.uri = "/", .method = HTTP_GET, .handler = handleRoot, .user_ctx = nullptr},
            {.uri = "/default", .method = HTTP_GET, .handler = handleDefault, .user_ctx = nullptr},
            {.uri = "/frontend", .method = HTTP_POST, .handler = handleFrontend, .user_ctx = nullptr},

            {.uri = "/name", .method = HTTP_GET, .handler = handleName, .user_ctx = nullptr},

            {.uri = "/readCells", .method = HTTP_GET, .handler = handleReadCells, .user_ctx = nullptr},
            {.uri = "/data", .method = HTTP_GET, .handler = handleData, .user_ctx = nullptr},

            {.uri = "/state", .method = HTTP_GET, .handler = handleState, .user_ctx = nullptr},
            {.uri = "/state/balancing", .method = HTTP_GET, .handler = handleBalancing, .user_ctx = nullptr},
            {.uri = "/state/monitor", .method = HTTP_GET, .handler = handleMonitor, .user_ctx = nullptr},
            {.uri = "/state/idle", .method = HTTP_GET, .handler = handleIdle, .user_ctx = nullptr},

            {.uri = "/canMode/*", .method = HTTP_GET, .handler = handleCanMode, .user_ctx = nullptr},
            {.uri = "/parameters/*", .method = HTTP_GET, .handler = handleParameters, .user_ctx = nullptr},
            {.uri = "/acknowledge/*", .method = HTTP_GET, .handler = handleAcknowledge, .user_ctx = nullptr},

            {.uri = "/fullShutdown", .method = HTTP_GET, .handler = handleFullShutdown, .user_ctx = nullptr},

            {.uri = "/upload", .method = HTTP_POST, .handler = handleFileUpload, .user_ctx = nullptr},

            {.uri = "/log/download", .method = HTTP_GET, .handler = handleLogDownload, .user_ctx = nullptr},
            {.uri = "/log/delete", .method = HTTP_GET, .handler = handleLogDelete, .user_ctx = nullptr},

            {.uri = "/forceDischarge/enable", .method = HTTP_GET, .handler = hangleForceDischargeEnable, .user_ctx = nullptr},
            {.uri = "/forceDischarge/disable", .method = HTTP_GET, .handler = handleForceDischargeDisable, .user_ctx = nullptr},
        };

        for (const auto &route : routes)
        {
            ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &route), TAG, "uri register failed");
        }

        ESP_RETURN_ON_ERROR(httpd_register_err_handler(s_server, HTTPD_404_NOT_FOUND, handleNotFound), TAG, "404 handler register failed");

        return ESP_OK;
    }

    // -------------------------------------------------------------------------- //
    // https://github.com/espressif/arduino-esp32/blob/master/libraries/WebServer/src/middleware/CorsMiddleware.cpp#L36
    bool checkCorsPreflight(httpd_req_t *req)
    {
        // TODO: No has header "Origin", is important?
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");

        if (req->method == HTTP_OPTIONS)
        {
            httpd_resp_set_status(req, "204 No Content");
            httpd_resp_send(req, NULL, 0);
            return true;
        }
        return false;
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    esp_err_t handleParameters(httpd_req_t *req)
    {
        if (checkCorsPreflight(req))
            return ESP_OK;

        std::string key = get_path_arg(req, "/parameters/", 0);
        std::string value = get_path_arg(req, "/parameters/", 1);

        if (key.empty() || value.empty())
        {
            return send_text(req, "400 Bad Request", "text/plain", "parameter not found");
        }

        std::string parsed_value;
        if (!update_parameter(key, value, parsed_value))
        {
            return send_text(req, "400 Bad Request", "text/plain", "parameter not found");
        }

        std::string response = "ok -> ";
        response += parsed_value;
        return send_text(req, "200 OK", "text/plain", response);
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    esp_err_t handleAcknowledge(httpd_req_t *req)
    {
        if (checkCorsPreflight(req))
            return ESP_OK;

        std::string fault = get_path_arg(req, "/acknowledge/", 0);

        std::string r = "ok";

        if (fault == "batteryMinVoltage")
        {
            clear_fault_by_index(faults::PersistentFault::CELL_UNDERVOLTAGE);
        }
        if (fault == "batteryMaxVoltage")
        {
            clear_fault_by_index(faults::PersistentFault::CELL_OVERVOLTAGE);
        }
        if (fault == "batteryAverageVoltage")
        {
            clear_fault_by_index(faults::PersistentFault::BATTERY_UNDERVOLTAGE);
            clear_fault_by_index(faults::PersistentFault::BATTERY_OVERVOLTAGE);
        }
        if (fault == "batteryVoltageDiff")
        {
            clear_fault_by_index(faults::PersistentFault::BATTERY_VOLTAGE_IMBALANCE);
        }
        if (fault == "batteryTherm1Temp")
        {
            clear_fault_by_index(faults::PersistentFault::TEMP_0);
        }
        if (fault == "batteryTherm2Temp")
        {
            clear_fault_by_index(faults::PersistentFault::TEMP_1);
        }
        if (fault == "batteryTherm3Temp")
        {
            clear_fault_by_index(faults::PersistentFault::TEMP_2);
        }
        if (fault == "batteryTherm4Temp")
        {
            clear_fault_by_index(faults::PersistentFault::TEMP_3);
        }
        if (fault == "batteryCurrent")
        {
            clear_fault_by_index(faults::PersistentFault::OVERCURRENT);
            clear_fault_by_index(faults::PersistentFault::UNDERCURRENT);
        }
        if (fault == "overPower")
        {
            clear_fault_by_index(faults::WarningFault::OVERPOWER);
        }
        if (fault == "coreZeroWatch")
        {
            clear_fault_by_index(faults::WarningFault::CORE_ZERO_WATCH);
        }
        else
        {
            r = "fault not found";
        }

        return send_text(req, "200 OK", "text/plain", r);
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    esp_err_t handleCanMode(httpd_req_t *req)
    {
        if (checkCorsPreflight(req))
            return ESP_OK;

        battery_state_cache::Snapshot snapshot = {};
        battery_state_cache::read(snapshot);
        std::string mode = get_path_arg(req, "/canMode/", 0);

        if (mode == "off")
        {
            digitalWrite(pins::ESP::CAN_S, LOW);
            can::stopCharging();
            return send_text(req, "200 OK", "text/plain", "off");
        }
        else if (mode == "charge")
        {
            can::requestCharging(snapshot.parameters.v_can_charge, snapshot.parameters.i_can_charge, true);
            return send_text(req, "200 OK", "text/plain", "charge");
        }
        // else if (mode == "vesc")
        // {
        //     canMode = CanMode::VESC;
        //     server.send(200, "text/plain", "vesc");
        // }
        // else if (mode == "sevcon")
        // {
        //     canMode = CanMode::SEVCON;
        //     server.send(200, "text/plain", "sevcon");
        // }
        else
        {
            return send_text(req, "400 Bad Request", "text/plain", "invalid mode");
        }
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    esp_err_t handleFullShutdown(httpd_req_t *req)
    {
        if (checkCorsPreflight(req))
            return ESP_OK;

        digitalWrite(pins::ESP::CONTACTOR, LOW);
        digitalWrite(pins::ESP::PWR_EN, LOW);
        pinMode(pins::ESP::PWR_EN, INPUT);

        return send_text(req, "200 OK", "text/plain", "ok");
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    esp_err_t hangleForceDischargeEnable(httpd_req_t *req)
    {
        if (checkCorsPreflight(req))
            return ESP_OK;

        ledcWrite(0, 0); // TODO: check on LEDC

        digitalWrite(pins::ESP::CONTACTOR, HIGH);
        return send_text(req, "200 OK", "text/plain", "enable");
    }

    esp_err_t handleForceDischargeDisable(httpd_req_t *req)
    {
        if (checkCorsPreflight(req))
            return ESP_OK;

        digitalWrite(pins::ESP::CONTACTOR, LOW);
        digitalWrite(pins::ESP::SS_SWITCH, LOW);
        return send_text(req, "200 OK", "text/plain", "disable");
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    esp_err_t handleLogDownload(httpd_req_t *req)
    {
        if (checkCorsPreflight(req))
            return ESP_OK;

        // TODO: check if heartbeat exists
        uint8_t heartbeat = HEARTBEAT_VALUE_LONG;
        xQueueSend(heartbeatQueue, &heartbeat, HEARTBEAT_SEND_DELAY);

        queue_logger_message(q_logger::Message{q_logger::msg::Flush{}});

        if (!stream_file(req, LOG_FILE, "text/csv"))
        {
            return send_text(req, "404 Not Found", "text/plain", "error");
        }

        return ESP_OK;
    }

    esp_err_t handleLogDelete(httpd_req_t *req)
    {
        if (checkCorsPreflight(req))
            return ESP_OK;

        // TODO: check if heartbeat exists
        uint8_t heartbeat = HEARTBEAT_VALUE_LONG;
        xQueueSend(heartbeatQueue, &heartbeat, HEARTBEAT_SEND_DELAY);

        queue_logger_message(q_logger::Message{q_logger::msg::Flush{}});
        bool success = std::remove(LOG_FILE) == 0;
        return send_text(req, "200 OK", "text/plain", success ? "ok" : "error");
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    esp_err_t handleName(httpd_req_t *req)
    {
        if (checkCorsPreflight(req))
            return ESP_OK;
        return send_text(req, "200 OK", "text/plain", BMS_NAME);
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    esp_err_t handleReadCells(httpd_req_t *req)
    {
        battery_state_cache::Snapshot snapshot = {};
        battery_state_cache::read(snapshot);

        std::string output;
        output += "apSSID: ";
        output += BMS_NAME;
        output += "\n";
        for (size_t current_ic = 0; current_ic < battery::IC_COUNT; current_ic++)
        {
            for (size_t i = 0; i < battery::CELL_COUNT_PER_IC; i++)
            {
                output += format_float(
                    battery::TO_VOLTAGE(snapshot.battery_data.ics[current_ic].cell_voltages[i]),
                    4);
                output += ",";
            }
            output += "\n";
        }
        output += "\n";
        output += "Min_Volt: ";
        output += format_float(battery::TO_VOLTAGE(snapshot.battery_data.min_voltage), 4);
        output += "\n";
        output += "Max_Volt: ";
        output += format_float(battery::TO_VOLTAGE(snapshot.battery_data.max_voltage), 4);
        output += "\n";
        output += "Avg_Volt: ";
        output += format_float(battery::TO_VOLTAGE(snapshot.battery_data.avg_voltage), 4);
        output += "\n";
        output += "Sum:";
        output += format_float(battery::TO_VOLTAGE(snapshot.battery_data.sum_voltage), 4);
        output += "\n";

        output += "Therm1:";
        output += format_float(snapshot.battery_data.temps.therms[0], 2);
        output += "\n";

        output += "Therm2:";
        output += format_float(snapshot.battery_data.temps.therms[1], 2);
        output += "\n";

        output += "Therm3:";
        output += format_float(snapshot.battery_data.temps.therms[2], 2);
        output += "\n";

        output += "Therm4:";
        output += format_float(snapshot.battery_data.temps.therms[3], 2);
        output += "\n";

        output += "ThermFET:";
        output += format_float(snapshot.battery_data.temps.fet, 2);
        output += "\n";

        output += "ThermBalBot:";
        output += format_float(snapshot.battery_data.temps.bal_bot, 2);
        output += "\n";

        output += "ThermBalTop:";
        output += format_float(snapshot.battery_data.temps.bal_top, 2);
        output += "\n";

        output += "Current:";
        output += format_float(snapshot.battery_data.current, 2);
        output += "\n";

        output += "Bypass: ";
        output += snapshot.parameters.bypass ? "ON\n" : "OFF\n";

        output += "Any bypassed: ";
        output += snapshot.any_bypassed ? "*Yes*\n" : "No\n";

        output += "State: ";
        output += printState(snapshot.mode);
        output += "\n";

        output += "SSS State: ";
        output += gpio_get_level(pins::ESP::SS_SWITCH) ? "ON" : "OFF";
        output += "  HCS State: ";
        output += gpio_get_level(pins::ESP::CONTACTOR) ? "ON" : "OFF";
        output += "\n";

        return send_text(req, "200 OK", "text/plain", output);
    }

    esp_err_t handleData(httpd_req_t *req)
    {
        if (checkCorsPreflight(req))
            return ESP_OK;

        battery_state_cache::Snapshot snapshot = {};
        battery_state_cache::read(snapshot);

        std::string json;
        json.reserve(3072);
        json += "{";

        json += "\"cells\":[";
        for (size_t ic = 0; ic < battery::IC_COUNT; ic++)
        {
            if (ic > 0)
            {
                json += ",";
            }
            json += "[";
            for (size_t cell = 0; cell < battery::CELL_COUNT_PER_IC; cell++)
            {
                if (cell > 0)
                {
                    json += ",";
                }
                json += format_float(
                    battery::TO_VOLTAGE(snapshot.battery_data.ics[ic].cell_voltages[cell]),
                    4);
            }
            json += "]";
        }
        json += "],";

        json += "\"discharge\":[";
        for (size_t ic = 0; ic < battery::IC_COUNT; ic++)
        {
            if (ic > 0)
            {
                json += ",";
            }
            json += "[";
            for (size_t cell = 0; cell < battery::CELL_COUNT_PER_IC; cell++)
            {
                if (cell > 0)
                {
                    json += ",";
                }
                json += snapshot.battery_data.ics[ic].discharge[cell] ? "true" : "false";
            }
            json += "]";
        }
        json += "],";

        json += "\"avg\":" + format_float(battery::TO_VOLTAGE(snapshot.battery_data.avg_voltage), 4) + ",";
        json += "\"min\":" + format_float(battery::TO_VOLTAGE(snapshot.battery_data.min_voltage), 4) + ",";
        json += "\"max\":" + format_float(battery::TO_VOLTAGE(snapshot.battery_data.max_voltage), 4) + ",";
        json += "\"sum\":" + format_float(battery::TO_VOLTAGE(snapshot.battery_data.sum_voltage), 4) + ",";

        const float sum_voltage = battery::TO_VOLTAGE(snapshot.battery_data.sum_voltage);
        json += "\"power\":" + format_float(sum_voltage * snapshot.battery_data.current, 4) + ",";

        json += "\"pack\":{";
        json += "\"1\":" + format_float(pack_voltage_sum(snapshot.battery_data.ics[0]), 4) + ",";
        json += "\"2\":" + format_float(pack_voltage_sum(snapshot.battery_data.ics[1]), 4);
        json += "},";

        json += "\"current\":" + format_float(snapshot.battery_data.current, 4) + ",";

        json += "\"therm\":{";
        json += "\"1\":" + format_float(snapshot.battery_data.temps.therms[0], 2) + ",";
        json += "\"2\":" + format_float(snapshot.battery_data.temps.therms[1], 2) + ",";
        json += "\"3\":" + format_float(snapshot.battery_data.temps.therms[2], 2) + ",";
        json += "\"4\":" + format_float(snapshot.battery_data.temps.therms[3], 2) + ",";
        json += "\"FET\":" + format_float(snapshot.battery_data.temps.fet, 2) + ",";
        json += "\"balBot\":" + format_float(snapshot.battery_data.temps.bal_bot, 2) + ",";
        json += "\"balTop\":" + format_float(snapshot.battery_data.temps.bal_top, 2);
        json += "},";

        json += "\"anyBypassed\":";
        json += snapshot.any_bypassed ? "true," : "false,";
        json += "\"tDiffTriggered\":";
        json += snapshot.t_diff_triggered ? "true," : "false,";
        json += "\"balTempBotTriggered\":";
        json += snapshot.bal_temp_bot_triggered ? "true," : "false,";
        json += "\"balTempTopTriggered\":";
        json += snapshot.bal_temp_top_triggered ? "true," : "false,";

        json += "\"state\":\"";
        json += printState(snapshot.mode);
        json += "\",";

        json += "\"SSS\":";
        json += gpio_get_level(pins::ESP::SS_SWITCH) ? "true," : "false,";
        json += "\"HCS\":";
        json += gpio_get_level(pins::ESP::CONTACTOR) ? "true," : "false,";

        json += "\"parameters\":{";
        json += "\"bypass\":";
        json += snapshot.parameters.bypass ? "true," : "false,";
        json += "\"vBypass\":" + format_float(snapshot.parameters.v_bypass, 4) + ",";
        json += "\"vMin\":" + format_float(snapshot.parameters.v_min, 4) + ",";
        json += "\"vMax\":" + format_float(snapshot.parameters.v_max, 4) + ",";
        json += "\"vMinAvg\":" + format_float(snapshot.parameters.v_min_avg, 4) + ",";
        json += "\"vMaxAvg\":" + format_float(snapshot.parameters.v_max_avg, 4) + ",";
        json += "\"vDiff\":" + format_float(snapshot.parameters.v_diff, 4) + ",";
        json += "\"tMin\":" + format_float(snapshot.parameters.t_min, 4) + ",";
        json += "\"tMax\":" + format_float(snapshot.parameters.t_max, 4) + ",";
        json += "\"tDiff\":" + format_float(snapshot.parameters.t_diff, 4) + ",";
        json += "\"tMaxBal\":" + format_float(snapshot.parameters.t_max_bal, 4) + ",";
        json += "\"tResetBal\":" + format_float(snapshot.parameters.t_reset_bal, 4) + ",";
        json += "\"logSpeed\":" + std::to_string(snapshot.parameters.log_inter) + ",";
        json += "\"deleteLog\":";
        json += snapshot.parameters.delete_log ? "true," : "false,";
        json += "\"vCanCharge\":" + format_float(snapshot.parameters.v_can_charge, 4) + ",";
        json += "\"iCanCharge\":" + format_float(snapshot.parameters.i_can_charge, 4);
        json += "},";

        json += "\"faults\":{";
        json += "\"batteryMinVoltage\":";
        json += is_fault_set(snapshot.faults, faults::PersistentFault::CELL_UNDERVOLTAGE) ? "true," : "false,";
        json += "\"batteryMaxVoltage\":";
        json += is_fault_set(snapshot.faults, faults::PersistentFault::CELL_OVERVOLTAGE) ? "true," : "false,";
        json += "\"batteryAverageVoltage\":";
        json += (is_fault_set(snapshot.faults, faults::PersistentFault::BATTERY_UNDERVOLTAGE) ||
                 is_fault_set(snapshot.faults, faults::PersistentFault::BATTERY_OVERVOLTAGE))
                    ? "true,"
                    : "false,";
        json += "\"batteryVoltageDiff\":";
        json += is_fault_set(snapshot.faults, faults::PersistentFault::BATTERY_VOLTAGE_IMBALANCE) ? "true," : "false,";
        json += "\"batteryTherm1Temp\":";
        json += is_fault_set(snapshot.faults, faults::PersistentFault::TEMP_0) ? "true," : "false,";
        json += "\"batteryTherm2Temp\":";
        json += is_fault_set(snapshot.faults, faults::PersistentFault::TEMP_1) ? "true," : "false,";
        json += "\"batteryTherm3Temp\":";
        json += is_fault_set(snapshot.faults, faults::PersistentFault::TEMP_2) ? "true," : "false,";
        json += "\"batteryTherm4Temp\":";
        json += is_fault_set(snapshot.faults, faults::PersistentFault::TEMP_3) ? "true," : "false,";
        json += "\"batteryCurrent\":";
        json += (is_fault_set(snapshot.faults, faults::PersistentFault::OVERCURRENT) ||
                 is_fault_set(snapshot.faults, faults::PersistentFault::UNDERCURRENT))
                    ? "true,"
                    : "false,";
        json += "\"overPower\":";
        json += is_fault_set(snapshot.faults, faults::WarningFault::OVERPOWER) ? "true," : "false,";
        json += "\"coreZeroWatch\":";
        json += is_fault_set(snapshot.faults, faults::WarningFault::CORE_ZERO_WATCH) ? "true" : "false";
        json += "},";

        json += "\"pFaults\":{";
        json += "\"batteryMinVoltage\":";
        json += is_fault_set(snapshot.persistent_faults, faults::PersistentFault::CELL_UNDERVOLTAGE) ? "true," : "false,";
        json += "\"batteryMaxVoltage\":";
        json += is_fault_set(snapshot.persistent_faults, faults::PersistentFault::CELL_OVERVOLTAGE) ? "true," : "false,";
        json += "\"batteryAverageVoltage\":";
        json += (is_fault_set(snapshot.persistent_faults, faults::PersistentFault::BATTERY_UNDERVOLTAGE) ||
                 is_fault_set(snapshot.persistent_faults, faults::PersistentFault::BATTERY_OVERVOLTAGE))
                    ? "true,"
                    : "false,";
        json += "\"batteryVoltageDiff\":";
        json += is_fault_set(snapshot.persistent_faults, faults::PersistentFault::BATTERY_VOLTAGE_IMBALANCE) ? "true," : "false,";
        json += "\"batteryTherm1Temp\":";
        json += is_fault_set(snapshot.persistent_faults, faults::PersistentFault::TEMP_0) ? "true," : "false,";
        json += "\"batteryTherm2Temp\":";
        json += is_fault_set(snapshot.persistent_faults, faults::PersistentFault::TEMP_1) ? "true," : "false,";
        json += "\"batteryTherm3Temp\":";
        json += is_fault_set(snapshot.persistent_faults, faults::PersistentFault::TEMP_2) ? "true," : "false,";
        json += "\"batteryTherm4Temp\":";
        json += is_fault_set(snapshot.persistent_faults, faults::PersistentFault::TEMP_3) ? "true," : "false,";
        json += "\"batteryCurrent\":";
        json += (is_fault_set(snapshot.persistent_faults, faults::PersistentFault::OVERCURRENT) ||
                 is_fault_set(snapshot.persistent_faults, faults::PersistentFault::UNDERCURRENT))
                    ? "true,"
                    : "false,";
        json += "\"overPower\":";
        json += is_fault_set(snapshot.persistent_faults, faults::WarningFault::OVERPOWER) ? "true," : "false,";
        json += "\"coreZeroWatch\":";
        json += is_fault_set(snapshot.persistent_faults, faults::WarningFault::CORE_ZERO_WATCH) ? "true" : "false";
        json += "},";

        json += "\"name\":\"";
        json += BMS_NAME;
        json += "\"";
        json += "}";

        return send_text(req, "200 OK", "application/json", json);
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    esp_err_t handleIdle(httpd_req_t *req)
    {
        if (checkCorsPreflight(req))
            return ESP_OK;

        // TODO: convert
        clear_discharge(TOTAL_IC, bms_ic);
        wakeup_sleep(TOTAL_IC);
        LTC6811_wrcfg(TOTAL_IC, bms_ic);

        for (int current_ic = 0; current_ic < TOTAL_IC; current_ic++)
        {
            battery.pack[current_ic].discharge = 0;
            bool gpio[5];
            // gpio[GPIO_BACKBAL] = 0;
            LTC681x_set_cfgr_gpio(current_ic, bms_ic, gpio);
        }

        set_mode(modes::Mode::IDLE);

        digitalWrite(pins::ESP::CONTACTOR, LOW);
        digitalWrite(pins::ESP::SS_SWITCH, LOW);

        return send_text(req, "200 OK", "text/plain", printState(modes::Mode::IDLE));
    }

    esp_err_t handleMonitor(httpd_req_t *req)
    {
        if (checkCorsPreflight(req))
            return ESP_OK;

        // TODO: convert
        clear_discharge(TOTAL_IC, bms_ic);
        wakeup_sleep(TOTAL_IC);
        LTC6811_wrcfg(TOTAL_IC, bms_ic);
        for (int current_ic = 0; current_ic < TOTAL_IC; current_ic++)
        {
            battery.pack[current_ic].discharge = 0;
            bool gpio[5];
            // gpio[GPIO_BACKBAL] = 0;
            LTC681x_set_cfgr_gpio(current_ic, bms_ic, gpio);
        }

        set_mode(modes::Mode::MONITORING);

        digitalWrite(pins::ESP::CONTACTOR, HIGH);

        return send_text(req, "200 OK", "text/plain", printState(modes::Mode::MONITORING));
    }

    esp_err_t handleBalancing(httpd_req_t *req)
    {
        if (checkCorsPreflight(req))
            return ESP_OK;

        if (!set_mode(modes::Mode::BALANCING))
        {
            return send_text(req, "503 Service Unavailable", "text/plain", "queue unavailable");
        }

        digitalWrite(pins::ESP::SS_SWITCH, HIGH);

        return send_text(req, "200 OK", "text/plain", printState(modes::Mode::BALANCING));
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    esp_err_t handleState(httpd_req_t *req)
    {
        if (checkCorsPreflight(req))
            return ESP_OK;

        battery_state_cache::Snapshot snapshot = {};
        battery_state_cache::read(snapshot);

        size_t total = 0;
        size_t used = 0;
        esp_littlefs_info(nullptr, &total, &used);

        char response[96] = {};
        std::snprintf(
            response,
            sizeof(response),
            "%s<br>%.2f/%.2f KB",
            printState(snapshot.mode),
            static_cast<float>(used) / 1024.0f,
            static_cast<float>(total) / 1024.0f);

        return send_text(req, "200 OK", "text/plain", response);
    }

    const char *printState(modes::Mode state)
    {
        switch (state)
        {
        case modes::Mode::IDLE:
            return "idle";
        case modes::Mode::MONITORING:
            return "monitor";
        case modes::Mode::BALANCING:
            return "balancing";
        default:
            return "STATE ERROR";
        }
    }
    // -------------------------------------------------------------------------- //

    const char *getContentType(const std::string &filename)
    { // determine the filetype of a given filename, based on the extension
        if (filename.size() >= 5 && filename.compare(filename.size() - 5, 5, ".html") == 0)
        {
            return "text/html";
        }

        if (filename.size() >= 4 && filename.compare(filename.size() - 4, 4, ".css") == 0)
        {
            return "text/css";
        }

        if (filename.size() >= 3 && filename.compare(filename.size() - 3, 3, ".js") == 0)
        {
            return "application/javascript";
        }

        if (filename.size() >= 4 && filename.compare(filename.size() - 4, 4, ".ico") == 0)
        {
            return "image/x-icon";
        }

        if (filename.size() >= 3 && filename.compare(filename.size() - 3, 3, ".gz") == 0)
        {
            return "application/x-gzip";
        }

        return "text/plain";
    }
    // -------------------------------------------------------------------------- //

    void writeToFile(FILE *file)
    {
        if (file == nullptr)
        {
            return;
        }

        std::string output = generateLogLine();
        std::fwrite(output.data(), 1, output.size(), file);
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    esp_err_t sendDefaultHTML(httpd_req_t *req)
    {
        ESP_RETURN_ON_ERROR(add_cors_headers(req), TAG, "cors failed");
        ESP_RETURN_ON_ERROR(httpd_resp_set_type(req, "text/html"), TAG, "type failed");

        size_t html_len = std::strlen(INDEX_HTML);
        size_t offset = 0;
        constexpr size_t CHUNK = 512;

        while (offset < html_len)
        {
            size_t send_len = std::min(CHUNK, html_len - offset);
            ESP_RETURN_ON_ERROR(httpd_resp_send_chunk(req, INDEX_HTML + offset, send_len), TAG, "chunk send failed");
            offset += send_len;
        }

        return httpd_resp_send_chunk(req, nullptr, 0);
    }

    esp_err_t handleDefault(httpd_req_t *req)
    {
        if (checkCorsPreflight(req))
            return ESP_OK;

        return sendDefaultHTML(req);
    }

    esp_err_t handleRoot(httpd_req_t *req)
    {
        if (checkCorsPreflight(req))
            return ESP_OK;

        // If there is a file version, use that. Otherwise, use the default embedded HTML.
        if (stream_file(req, HTML_FILE, "text/html"))
        {
            return ESP_OK;
        }
        else
        {
            return sendDefaultHTML(req);
        }
    }

    FILE fFrontend; // Global variable to handle the file upload

    esp_err_t handleFrontend(httpd_req_t *req)
    {
        if (checkCorsPreflight(req))
        {
            return ESP_OK;
        }

        std::vector<uint8_t> body;
        esp_err_t read_status = read_request_body(req, body);
        if (read_status == ESP_ERR_INVALID_SIZE)
        {
            return send_text(req, "413 Payload Too Large", "text/plain", "error");
        }
        if (read_status != ESP_OK)
        {
            return send_text(req, "400 Bad Request", "text/plain", "error");
        }

        std::string filename;
        const uint8_t *file_data = nullptr;
        size_t file_size = 0;

        std::string content_type = content_type_header(req);
        if (!parse_multipart_file(body, content_type, "data", filename, file_data, file_size))
        {
            return send_text(req, "400 Bad Request", "text/plain", "error");
        }

        std::FILE *file = std::fopen(HTML_FILE, "wb");
        if (file == nullptr)
        {
            return send_text(req, "500 Internal Server Error", "text/plain", "error");
        }

        size_t written = std::fwrite(file_data, 1, file_size, file);
        std::fclose(file);

        return send_text(req, "200 OK", "text/plain", written == file_size ? "ok" : "error");
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    esp_err_t handleFileUpload(httpd_req_t *req)
    {
        if (checkCorsPreflight(req))
        {
            return ESP_OK;
        }

        std::vector<uint8_t> body;
        esp_err_t read_status = read_request_body(req, body);
        if (read_status == ESP_ERR_INVALID_SIZE)
        {
            return send_text(req, "413 Payload Too Large", "text/plain", "error");
        }
        if (read_status != ESP_OK)
        {
            return send_text(req, "400 Bad Request", "text/plain", "error");
        }

        std::string filename;
        const uint8_t *file_data = nullptr;
        size_t file_size = 0;

        std::string content_type = content_type_header(req);
        if (!parse_multipart_file(body, content_type, "data", filename, file_data, file_size))
        {
            return send_text(req, "400 Bad Request", "text/plain", "error");
        }

        if (filename.empty())
        {
            filename = "upload.bin";
        }

        if (filename.front() != '/')
        {
            filename = "/" + filename;
        }

        std::string out_path = to_fs_path(filename);

        if (!ends_with(filename, ".gz"))
        {
            std::string gz_path = out_path + ".gz";
            std::remove(gz_path.c_str());
        }

        std::FILE *file = std::fopen(out_path.c_str(), "wb");
        if (file == nullptr)
        {
            return send_text(req, "500 Internal Server Error", "text/plain", "error");
        }

        size_t written = std::fwrite(file_data, 1, file_size, file);
        std::fclose(file);

        return send_text(req, "200 OK", "text/plain", written == file_size ? "ok" : "error");
    }

    bool handleFileRead(httpd_req_t *req, const std::string &path)
    {
        std::string file_path = path;
        if (!file_path.empty() && file_path.back() == '/')
        {
            file_path += "index.html";
        }

        std::string fs_file_path = to_fs_path(file_path);
        std::string path_with_gz = fs_file_path + ".gz";

        if (stream_file(req, path_with_gz.c_str(), getContentType(path_with_gz)) ||
            stream_file(req, fs_file_path.c_str(), getContentType(fs_file_path)))
        {
            return true;
        }

        return false;
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    esp_err_t handleNotFound(httpd_req_t *req, httpd_err_code_t err)
    {
        (void)err;

        if (checkCorsPreflight(req))
            return ESP_OK;

        if (!handleFileRead(req, req->uri))
        { // check if the file exists in the flash memory (SPIFFS), if so, send it
            return send_text(req, "404 Not Found", "text/plain", "404: File Not Found");
        }

        return ESP_OK;
    }
    // -------------------------------------------------------------------------- //

    bool queue_battery_message(const q_battery::Message &msg)
    {
        if (q_battery::g_battery_queue == nullptr)
        {
            return false;
        }
        return xQueueSend(q_battery::g_battery_queue, &msg, 0) == pdTRUE;
    }

    bool queue_logger_message(const q_logger::Message &msg)
    {
        if (q_logger::g_logger_queue == nullptr)
        {
            return false;
        }
        return xQueueSend(q_logger::g_logger_queue, &msg, 0) == pdTRUE;
    }

    bool set_mode(modes::Mode mode)
    {
        q_battery::Message msg = q_battery::msg::SetMode{.mode = mode};
        if (!queue_battery_message(msg))
        {
            return false;
        }
        return true;
    }

    bool clear_fault_by_index(size_t fault_index)
    {
        q_battery::Message msg = faults::msg::ClearFault{.fault_index = fault_index};
        return queue_battery_message(msg);
    }

    bool update_parameter(const std::string &legacy_key, const std::string &raw_value, std::string &out_value)
    {
        if (legacy_key == "bypass")
        {
            bool parsed = false;
            if (!parse_bool(raw_value, parsed))
            {
                return false;
            }
            q_battery::Message msg = params::msg::SetParameterBool{.key = "bypass", .value = parsed};
            if (!queue_battery_message(msg))
            {
                return false;
            }
            out_value = parsed ? "1" : "0";
            return true;
        }

        if (legacy_key == "vBypass")
        {
            float parsed = 0.0f;
            if (!parse_float(raw_value, parsed))
            {
                return false;
            }
            q_battery::Message msg = params::msg::SetParameterF32{.key = "v_bypass", .value = parsed};
            if (!queue_battery_message(msg))
            {
                return false;
            }
            out_value = raw_value;
            return true;
        }

        if (legacy_key == "vMin")
        {
            float parsed = 0.0f;
            if (!parse_float(raw_value, parsed))
            {
                return false;
            }
            q_battery::Message msg = params::msg::SetParameterF32{.key = "v_min", .value = parsed};
            if (!queue_battery_message(msg))
            {
                return false;
            }
            out_value = raw_value;
            return true;
        }

        if (legacy_key == "vMax")
        {
            float parsed = 0.0f;
            if (!parse_float(raw_value, parsed))
            {
                return false;
            }
            q_battery::Message msg = params::msg::SetParameterF32{.key = "v_max", .value = parsed};
            if (!queue_battery_message(msg))
            {
                return false;
            }
            out_value = raw_value;
            return true;
        }

        if (legacy_key == "vMinAvg")
        {
            float parsed = 0.0f;
            if (!parse_float(raw_value, parsed))
            {
                return false;
            }
            q_battery::Message msg = params::msg::SetParameterF32{.key = "v_min_avg", .value = parsed};
            if (!queue_battery_message(msg))
            {
                return false;
            }
            out_value = raw_value;
            return true;
        }

        if (legacy_key == "vMaxAvg")
        {
            float parsed = 0.0f;
            if (!parse_float(raw_value, parsed))
            {
                return false;
            }
            q_battery::Message msg = params::msg::SetParameterF32{.key = "v_max_avg", .value = parsed};
            if (!queue_battery_message(msg))
            {
                return false;
            }
            out_value = raw_value;
            return true;
        }

        if (legacy_key == "vDiff")
        {
            float parsed = 0.0f;
            if (!parse_float(raw_value, parsed))
            {
                return false;
            }
            q_battery::Message msg = params::msg::SetParameterF32{.key = "v_diff", .value = parsed};
            if (!queue_battery_message(msg))
            {
                return false;
            }
            out_value = raw_value;
            return true;
        }

        if (legacy_key == "tMin")
        {
            float parsed = 0.0f;
            if (!parse_float(raw_value, parsed))
            {
                return false;
            }
            q_battery::Message msg = params::msg::SetParameterF32{.key = "t_min", .value = parsed};
            if (!queue_battery_message(msg))
            {
                return false;
            }
            out_value = raw_value;
            return true;
        }

        if (legacy_key == "tMax")
        {
            float parsed = 0.0f;
            if (!parse_float(raw_value, parsed))
            {
                return false;
            }
            q_battery::Message msg = params::msg::SetParameterF32{.key = "t_max", .value = parsed};
            if (!queue_battery_message(msg))
            {
                return false;
            }
            out_value = raw_value;
            return true;
        }

        if (legacy_key == "tDiff")
        {
            float parsed = 0.0f;
            if (!parse_float(raw_value, parsed))
            {
                return false;
            }
            q_battery::Message msg = params::msg::SetParameterF32{.key = "t_diff", .value = parsed};
            if (!queue_battery_message(msg))
            {
                return false;
            }
            out_value = raw_value;
            return true;
        }

        if (legacy_key == "tMaxBal")
        {
            float parsed = 0.0f;
            if (!parse_float(raw_value, parsed))
            {
                return false;
            }
            q_battery::Message msg = params::msg::SetParameterF32{.key = "t_max_bal", .value = parsed};
            if (!queue_battery_message(msg))
            {
                return false;
            }
            out_value = raw_value;
            return true;
        }

        if (legacy_key == "tResetBal")
        {
            float parsed = 0.0f;
            if (!parse_float(raw_value, parsed))
            {
                return false;
            }
            q_battery::Message msg = params::msg::SetParameterF32{.key = "t_reset_bal", .value = parsed};
            if (!queue_battery_message(msg))
            {
                return false;
            }
            out_value = raw_value;
            return true;
        }

        if (legacy_key == "logSpeed")
        {
            uint32_t parsed = 0;
            if (!parse_u32(raw_value, parsed))
            {
                return false;
            }
            q_battery::Message msg = params::msg::SetParameterU32{.key = "log_inter", .value = parsed};
            if (!queue_battery_message(msg))
            {
                return false;
            }
            out_value = std::to_string((parsed == 0U) ? 1U : parsed);
            return true;
        }

        if (legacy_key == "deleteLog")
        {
            bool parsed = false;
            if (!parse_bool(raw_value, parsed))
            {
                return false;
            }
            q_battery::Message msg = params::msg::SetParameterBool{.key = "delete_log", .value = parsed};
            if (!queue_battery_message(msg))
            {
                return false;
            }
            out_value = parsed ? "1" : "0";
            return true;
        }

        if (legacy_key == "vCanCharge")
        {
            float parsed = 0.0f;
            if (!parse_float(raw_value, parsed))
            {
                return false;
            }
            q_battery::Message msg = params::msg::SetParameterF32{.key = "v_can_charge", .value = parsed};
            if (!queue_battery_message(msg))
            {
                return false;
            }
            out_value = raw_value;
            return true;
        }

        if (legacy_key == "iCanCharge")
        {
            float parsed = 0.0f;
            if (!parse_float(raw_value, parsed))
            {
                return false;
            }
            q_battery::Message msg = params::msg::SetParameterF32{.key = "i_can_charge", .value = parsed};
            if (!queue_battery_message(msg))
            {
                return false;
            }
            out_value = raw_value;
            return true;
        }

        return false;
    }

    esp_err_t add_cors_headers(httpd_req_t *req)
    {
        ESP_RETURN_ON_ERROR(httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*"), TAG, "cors header failed");
        ESP_RETURN_ON_ERROR(httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"), TAG, "cors header failed");
        ESP_RETURN_ON_ERROR(httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, Authorization"), TAG, "cors header failed");
        return ESP_OK;
    }

    esp_err_t send_text(httpd_req_t *req, const char *status, const char *type, const std::string &body)
    {
        ESP_RETURN_ON_ERROR(add_cors_headers(req), TAG, "cors failed");
        ESP_RETURN_ON_ERROR(httpd_resp_set_status(req, status), TAG, "status failed");
        ESP_RETURN_ON_ERROR(httpd_resp_set_type(req, type), TAG, "type failed");
        return httpd_resp_send(req, body.c_str(), body.size());
    }

    esp_err_t send_text(httpd_req_t *req, const char *status, const char *type, const char *body)
    {
        ESP_RETURN_ON_ERROR(add_cors_headers(req), TAG, "cors failed");
        ESP_RETURN_ON_ERROR(httpd_resp_set_status(req, status), TAG, "status failed");
        ESP_RETURN_ON_ERROR(httpd_resp_set_type(req, type), TAG, "type failed");
        return httpd_resp_sendstr(req, body);
    }

    esp_err_t require_method(httpd_req_t *req, httpd_method_t method)
    {
        if (req->method != method)
        {
            return send_text(req, "405 Method Not Allowed", "text/plain", "");
        }
        return ESP_OK;
    }

    bool starts_with(const std::string &value, const std::string &prefix)
    {
        return value.rfind(prefix, 0) == 0;
    }

    bool ends_with(const std::string &value, const std::string &suffix)
    {
        if (suffix.size() > value.size())
        {
            return false;
        }

        return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    std::vector<std::string> split_path(const std::string &uri)
    {
        std::vector<std::string> parts;
        size_t start = 0;

        while (start < uri.size())
        {
            size_t next = uri.find('/', start);
            if (next == std::string::npos)
            {
                next = uri.size();
            }

            if (next > start)
            {
                parts.push_back(uri.substr(start, next - start));
            }

            start = next + 1;
        }

        return parts;
    }

    std::string to_fs_path(const std::string &path)
    {
        if (path.empty())
        {
            return std::string(hardware::FILESYSTEM_BASE_PATH);
        }

        if (starts_with(path, hardware::FILESYSTEM_BASE_PATH))
        {
            return path;
        }

        if (path.front() == '/')
        {
            return std::string(hardware::FILESYSTEM_BASE_PATH) + path;
        }

        return std::string(hardware::FILESYSTEM_BASE_PATH) + "/" + path;
    }

    std::string get_path_arg(httpd_req_t *req, const std::string &prefix, size_t index)
    {
        std::string uri = req->uri;
        if (!starts_with(uri, prefix))
        {
            return "";
        }

        std::string rest = uri.substr(prefix.size());
        auto parts = split_path(rest);

        if (index >= parts.size())
        {
            return "";
        }

        return parts[index];
    }

    std::string content_type_header(httpd_req_t *req)
    {
        size_t value_len = httpd_req_get_hdr_value_len(req, "Content-Type");
        if (value_len == 0)
        {
            return "";
        }

        std::string header;
        header.resize(value_len + 1);

        if (httpd_req_get_hdr_value_str(req, "Content-Type", header.data(), header.size()) != ESP_OK)
        {
            return "";
        }

        header.resize(value_len);
        return header;
    }

    esp_err_t read_request_body(httpd_req_t *req, std::vector<uint8_t> &out)
    {
        out.clear();
        if (req->content_len <= 0)
        {
            return ESP_OK;
        }

        if (static_cast<size_t>(req->content_len) > MAX_UPLOAD_SIZE)
        {
            return ESP_ERR_INVALID_SIZE;
        }

        out.resize(static_cast<size_t>(req->content_len));

        size_t offset = 0;
        while (offset < out.size())
        {
            int received = httpd_req_recv(req, reinterpret_cast<char *>(out.data() + offset), out.size() - offset);
            if (received <= 0)
            {
                return ESP_FAIL;
            }
            offset += static_cast<size_t>(received);
        }

        return ESP_OK;
    }

    std::string format_float(float value, int precision)
    {
        char buffer[32] = {};
        std::snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
        return std::string(buffer);
    }

    float pack_voltage_sum(const battery::IcData &ic)
    {
        float sum = 0.0f;
        for (size_t i = 0; i < battery::CELL_COUNT_PER_IC; i++)
        {
            sum += battery::TO_VOLTAGE(ic.cell_voltages[i]);
        }
        return sum;
    }

    bool is_fault_set(uint32_t bits, size_t index)
    {
        return (bits & (static_cast<uint32_t>(1) << index)) != 0U;
    }

    bool clear_fault_by_index(size_t fault_index)
    {
        q_battery::Message msg = faults::msg::ClearFault{.fault_index = fault_index};
        return queue_battery_message(msg);
    }
}
#endif