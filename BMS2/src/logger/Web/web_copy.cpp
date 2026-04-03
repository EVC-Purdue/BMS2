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
        {
            return ESP_OK;
        }

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
    void handleAcknowledge()
    {
        if (checkCorsPreflight())
            return;

        String f = server.pathArg(0);

        xSemaphoreTake(xMutex, portMAX_DELAY);

        String r = "ok";

        if (f == "batteryMinVoltage")
        {
            battery.pFaults.batteryMinVoltage = false;
        }
        else if (f == "batteryMaxVoltage")
        {
            battery.pFaults.batteryMaxVoltage = false;
        }
        else if (f == "batteryAverageVoltage")
        {
            battery.pFaults.batteryAverageVoltage = false;
        }
        else if (f == "batteryVoltageDiff")
        {
            battery.pFaults.batteryVoltageDiff = false;
        }
        else if (f == "batteryTherm1Temp")
        {
            battery.pFaults.batteryTherm1Temp = false;
        }
        else if (f == "batteryTherm2Temp")
        {
            battery.pFaults.batteryTherm2Temp = false;
        }
        else if (f == "batteryTherm3Temp")
        {
            battery.pFaults.batteryTherm3Temp = false;
        }
        else if (f == "batteryTherm4Temp")
        {
            battery.pFaults.batteryTherm4Temp = false;
        }
        else if (f == "batteryCurrent")
        {
            battery.pFaults.batteryCurrent = false;
        }
        else if (f == "overPower")
        {
            battery.pFaults.overPower = false;
        }
        else if (f == "coreZeroWatch")
        {
            battery.pFaults.coreZeroWatch = false;
        }
        else
        {
            r = "fault not found";
        }

        xSemaphoreGive(xMutex);

        server.send(200, "text/plain", r);
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    void handleCanMode()
    {
        if (checkCorsPreflight())
            return;

        String m = server.pathArg(0);

        xSemaphoreTake(xMutex, portMAX_DELAY);

        if (m == "off")
        {
            if (canMode != CanMode::OFF)
            {
                stopCan();
            }

            canMode = CanMode::OFF;
            digitalWrite(CAN_ON_GPIO, LOW);
            server.send(200, "text/plain", "off");
        }
        else
        {
            if (canMode == CanMode::OFF)
            {
                startCan();
            }

            digitalWrite(CAN_ON_GPIO, HIGH);

            if (m == "charge")
            {
                canMode = CanMode::CHARGE;
                server.send(200, "text/plain", "charge");
            }
            else if (m == "vesc")
            {
                canMode = CanMode::VESC;
                server.send(200, "text/plain", "vesc");
            }
            else if (m == "sevcon")
            {
                canMode = CanMode::SEVCON;
                server.send(200, "text/plain", "sevcon");
            }
            else
            {
                server.send(400, "text/plain", "invalid mode");
            }
        }

        xSemaphoreGive(xMutex);
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    void handleFullShutdown()
    {
        if (checkCorsPreflight())
            return;

        digitalWrite(CONTACTOR_GPIO, LOW);

        xSemaphoreTake(xMutex, portMAX_DELAY);
        buzzOn = false;
        xSemaphoreGive(xMutex);

        server.send(200, "text/plain", "ok");

        digitalWrite(PWR_EN_GPIO, LOW);
        pinMode(PWR_EN_GPIO, INPUT);
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    void hangleForceDischargeEnable()
    {
        if (checkCorsPreflight())
            return;

        digitalWrite(CONTACTOR_GPIO, HIGH);

        xSemaphoreTake(xMutex, portMAX_DELAY);
        buzzOn = false;
        xSemaphoreGive(xMutex);

        ledcWrite(0, 0);
        // digitalWrite(BUZZER_GPIO, LOW);
        server.send(200, "text/plain", "enable");
    }

    void handleForceDischargeDisable()
    {
        if (checkCorsPreflight())
            return;

        digitalWrite(CONTACTOR_GPIO, LOW);
        digitalWrite(SS_SWITCH_GPIO, LOW);

        xSemaphoreTake(xMutex, portMAX_DELAY);
        buzzOn = true;
        xSemaphoreGive(xMutex);

        server.send(200, "text/plain", "disable");
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    void handleLogDownload()
    {
        if (checkCorsPreflight())
            return;

        uint8_t heartbeat = HEARTBEAT_VALUE_LONG;
        xQueueSend(heartbeatQueue, &heartbeat, HEARTBEAT_SEND_DELAY);
        File file = SPIFFS.open(LOG_FILE, "r");
        server.streamFile(file, "text/csv");
        file.close();
    }

    void handleLogDelete()
    {
        if (checkCorsPreflight())
            return;

        uint8_t heartbeat = HEARTBEAT_VALUE_LONG;
        xQueueSend(heartbeatQueue, &heartbeat, HEARTBEAT_SEND_DELAY);
        bool success = SPIFFS.remove(LOG_FILE);
        server.send(200, "text/plain", success ? "ok" : "error");
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    void handleName()
    {
        if (checkCorsPreflight())
            return;

        server.send(200, "text/plain", apSSID);
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    void handleReadCells()
    {
        String output = "apSSID: ";
        output += apSSID;
        output += "\n";

        xSemaphoreTake(xMutex, portMAX_DELAY);

        for (int current_ic = 0; current_ic < TOTAL_IC; current_ic++)
        {
            for (int i = 0; i < bms_ic[0].ic_reg.cell_channels; i++)
            {
                output += battery.pack[current_ic].cells[i] * 0.0001;
                output += ",";
            }
            output += "\n";
        }
        output += "\n";
        output += "Min_Volt: ";
        output += battery.min.voltage * 0.0001;
        output += ",";
        output += "\n";
        output += "Max_Volt: ";
        output += battery.max.voltage * 0.0001;
        output += ",";
        output += "\n";
        output += "Avg_Volt: ";
        output += battery.average * 0.0001;
        output += "\n";
        output += "Sum:";
        output += battery.sum * 0.0001;
        output += "\n";

        output += "Therm1:";
        output += battery.temps.therm1;
        output += "\n";

        output += "Therm2:";
        output += battery.temps.therm2;
        output += "\n";

        output += "Therm3:";
        output += battery.temps.therm3;
        output += "\n";

        output += "Therm4:";
        output += battery.temps.therm4;
        output += "\n";

        output += "ThermFET:";
        output += battery.temps.thermFET;
        output += "\n";

        output += "ThermBalBot:";
        output += battery.temps.thermBalBot;
        output += "\n";

        output += "ThermBalTop:";
        output += battery.temps.thermBalTop;
        output += "\n";

        output += "Current:";
        output += battery.current;
        output += "\n";

        output += "Bypass: ";
        output += parameters.bypass ? "ON" : "OFF";
        output += "\n";

        output += "Any bypassed: ";
        output += battery.anyBypassed ? "*Yes*" : "No";
        output += "\n";

        output += "State: ";
        output += printState(runState);
        output += "\n";

        output += "SSS State: ";
        output += digitalRead(SS_SWITCH_GPIO) ? "ON" : "OFF";
        output += "  HCS State: ";
        output += digitalRead(CONTACTOR_GPIO) ? "ON" : "OFF";
        output += "\n";

        esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);
        while (it != NULL)
        {
            const esp_partition_t *partition = esp_partition_get(it);
            // Serial.printf("Partition: %s, Size: %d bytes, Address: 0x%08x\n", partition->label, partition->size, partition->address);
            output += "\n";
            output += "Partition: ";
            output += partition->label;
            output += ", Size: ";
            output += partition->size;
            output += " bytes, Address: 0x";
            output += String(partition->address, HEX);
            it = esp_partition_next(it);
        }
        esp_partition_iterator_release(it);

        xSemaphoreGive(xMutex);

        server.send(200, "text/plain", output);
    }

    void handleData()
    {
        if (checkCorsPreflight())
            return;

        // TODO: not sure if I am using ArduinoJson correctly
        // It compiles be it might throw an error at runtime
        JsonDocument doc;

        xSemaphoreTake(xMutex, portMAX_DELAY);

        doc["cells"] = JsonArray();
        doc["discharge"] = JsonArray();

        for (int current_ic = 0; current_ic < TOTAL_IC; current_ic++)
        {
            doc["discharge"][current_ic] = battery.pack[current_ic].discharge;

            doc["cells"][current_ic] = JsonArray();
            for (int i = 0; i < bms_ic[0].ic_reg.cell_channels; i++)
            {
                doc["cells"][current_ic][i] = battery.pack[current_ic].cells[i] * 0.0001;
            }
        }

        doc["avg"] = battery.average * 0.0001;
        doc["min"] = battery.min.voltage * 0.0001;
        doc["max"] = battery.max.voltage * 0.0001;
        doc["sum"] = battery.sum * 0.0001;
        doc["power"] = battery.sum * battery.current * 0.0001;

        doc["pack"]["1"] = battery.pack[0].sum * 0.0001;
        doc["pack"]["2"] = battery.pack[1].sum * 0.0001;

        doc["current"] = battery.current;

        doc["therm"]["1"] = battery.temps.therm1;
        doc["therm"]["2"] = battery.temps.therm2;
        doc["therm"]["3"] = battery.temps.therm3;
        doc["therm"]["4"] = battery.temps.therm4;
        doc["therm"]["FET"] = battery.temps.thermFET;
        doc["therm"]["balBot"] = battery.temps.thermBalBot;
        doc["therm"]["balTop"] = battery.temps.thermBalTop;

        doc["anyBypassed"] = battery.anyBypassed;
        doc["tDiffTriggered"] = battery.tDiffTriggered;
        doc["balTempBotTriggered"] = battery.balTempBotTriggered;
        doc["balTempTopTriggered"] = battery.balTempTopTriggered;

        doc["state"] = printState(runState);

        doc["SSS"] = digitalRead(SS_SWITCH_GPIO) ? true : false;
        doc["HCS"] = digitalRead(CONTACTOR_GPIO) ? true : false;

        doc["parameters"]["bypass"] = parameters.bypass;
        doc["parameters"]["vBypass"] = parameters.vBypass;

        doc["parameters"]["vMin"] = parameters.vMin;
        doc["parameters"]["vMax"] = parameters.vMax;
        doc["parameters"]["vMinAvg"] = parameters.vMinAvg;
        doc["parameters"]["vMaxAvg"] = parameters.vMaxAvg;
        doc["parameters"]["vDiff"] = parameters.vDiff;

        doc["parameters"]["tMin"] = parameters.tMin;
        doc["parameters"]["tMax"] = parameters.tMax;
        doc["parameters"]["tDiff"] = parameters.tDiff;

        doc["parameters"]["tMaxBal"] = parameters.tMaxBal;
        doc["parameters"]["tResetBal"] = parameters.tResetBal;

        doc["parameters"]["logSpeed"] = parameters.logSpeed;
        doc["parameters"]["deleteLog"] = parameters.deleteLog;

        doc["parameters"]["vCanCharge"] = parameters.vCanCharge;
        doc["parameters"]["iCanCharge"] = parameters.iCanCharge;

        doc["faults"]["batteryMinVoltage"] = battery.faults.batteryMinVoltage;
        doc["faults"]["batteryMaxVoltage"] = battery.faults.batteryMaxVoltage;
        doc["faults"]["batteryAverageVoltage"] = battery.faults.batteryAverageVoltage;
        doc["faults"]["batteryVoltageDiff"] = battery.faults.batteryVoltageDiff;
        doc["faults"]["batteryTherm1Temp"] = battery.faults.batteryTherm1Temp;
        doc["faults"]["batteryTherm2Temp"] = battery.faults.batteryTherm2Temp;
        doc["faults"]["batteryTherm3Temp"] = battery.faults.batteryTherm3Temp;
        doc["faults"]["batteryTherm4Temp"] = battery.faults.batteryTherm4Temp;
        doc["faults"]["batteryCurrent"] = battery.faults.batteryCurrent;
        doc["faults"]["overPower"] = battery.faults.overPower;
        doc["faults"]["coreZeroWatch"] = battery.faults.coreZeroWatch;

        doc["pFaults"]["batteryMinVoltage"] = battery.pFaults.batteryMinVoltage;
        doc["pFaults"]["batteryMaxVoltage"] = battery.pFaults.batteryMaxVoltage;
        doc["pFaults"]["batteryAverageVoltage"] = battery.pFaults.batteryAverageVoltage;
        doc["pFaults"]["batteryVoltageDiff"] = battery.pFaults.batteryVoltageDiff;
        doc["pFaults"]["batteryTherm1Temp"] = battery.pFaults.batteryTherm1Temp;
        doc["pFaults"]["batteryTherm2Temp"] = battery.pFaults.batteryTherm2Temp;
        doc["pFaults"]["batteryTherm3Temp"] = battery.pFaults.batteryTherm3Temp;
        doc["pFaults"]["batteryTherm4Temp"] = battery.pFaults.batteryTherm4Temp;
        doc["pFaults"]["batteryCurrent"] = battery.pFaults.batteryCurrent;
        doc["pFaults"]["overPower"] = battery.pFaults.overPower;
        doc["pFaults"]["coreZeroWatch"] = battery.pFaults.coreZeroWatch;

        doc["name"] = apSSID;

        xSemaphoreGive(xMutex);

        String output;
        serializeJson(doc, output);
        server.send(200, "application/json", output);
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    void handleIdle()
    {
        if (checkCorsPreflight())
            return;

        xSemaphoreTake(xMutex, portMAX_DELAY);

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

        runState = IDLE;

        xSemaphoreGive(xMutex);

        digitalWrite(CONTACTOR_GPIO, LOW);
        digitalWrite(SS_SWITCH_GPIO, LOW);
        server.send(200, "text/plain", printState(runState));
    }

    void handleMonitor()
    {
        if (checkCorsPreflight())
            return;

        xSemaphoreTake(xMutex, portMAX_DELAY);

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

        runState = MONITOR;

        xSemaphoreGive(xMutex);

        digitalWrite(CONTACTOR_GPIO, HIGH);
        server.send(200, "text/plain", printState(runState));
    }

    void handleBalancing()
    {
        if (checkCorsPreflight())
            return;

        xSemaphoreTake(xMutex, portMAX_DELAY);
        runState = BALANCING;
        xSemaphoreGive(xMutex);

        digitalWrite(SS_SWITCH_GPIO, HIGH);
        server.send(200, "text/plain", printState(runState));
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    void handleState()
    {
        if (checkCorsPreflight())
            return;

        float fileTotalKB = (float)SPIFFS.totalBytes() / 1024.0;
        float fileUsedKB = (float)SPIFFS.usedBytes() / 1024.0;

        // float flashChipSize = (float)ESP.getFlashChipSize() / 1024.0 / 1024.0;
        // float realFlashChipSize = (float)ESP.getFlashChipRealSize() / 1024.0 / 1024.0;
        // float flashFreq = (float)ESP.getFlashChipSpeed() / 1000.0 / 1000.0;

        xSemaphoreTake(xMutex, portMAX_DELAY);

        int index = 0;
        char response[300];
        index = snprintf(response, 300, "%s<br>%.2f/%.2f KB", printState(runState), fileUsedKB, fileTotalKB);

        // for (int i = 0; i < TOTAL_IC; i++) {
        //     index += snprintf(
        //         response + index,
        //         300 - index,
        //         "<br>RAWbypass: %.2f Bypasss: %.2f",
        //         (float)bms_ic[i].aux.a_codes[THERM_FET_GPIO-1] * 0.0001,
        //         battery.pack[i].thermFETTemp
        //     );
        // }

        xSemaphoreGive(xMutex);

        server.send(200, "text/plain", response);
    }

    const char *printState(RunState state)
    {
        switch (state)
        {
        case IDLE:
            return "idle";
        case MONITOR:
            return "monitor";
        case BALANCING:
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

    // -------------------------------------------------------------------------- //
    void saveData()
    {
        // Make sure we create the file
        File cellLog = SPIFFS.open(LOG_FILE, FILE_READ);
        if (!cellLog)
        {
            cellLog = SPIFFS.open(LOG_FILE, FILE_WRITE);
            cellLog.close();
        }

        bool logNow = true;
        float used_percent = (float)SPIFFS.usedBytes() / (float)SPIFFS.totalBytes();
        if (used_percent > LOG_CAPCITY_PERCENT)
        {
            Serial.println("SPIFFS is full!");

            xSemaphoreTake(xMutex, portMAX_DELAY);
            bool deleteLog = parameters.deleteLog;
            xSemaphoreGive(xMutex);

            if (deleteLog)
            {
                SPIFFS.remove(LOG_FILE);
                Serial.println("Log Deleted");
            }
            else
            {
                logNow = false;
            }
        }

        if (logNow)
        {
            File cellLog = SPIFFS.open(LOG_FILE, FILE_APPEND); // APPEND = add

            for (int i = 0; i < battery.logFaults.count; i++)
            {
                cellLog.print(battery.logFaults.lines[i]);
            }
            battery.logFaults.count = 0;

            writeToFile(cellLog);
            cellLog.close();
            Serial.println("Data Logged");
        }
    }

    String generateLogLine()
    {
        String output = String(millis()) + ",";

        for (int current_ic = 0; current_ic < TOTAL_IC; current_ic++)
        {
            for (int i = 0; i < bms_ic[0].ic_reg.cell_channels; i++)
            {
                output += String(battery.pack[current_ic].cells[i]) + ",";
            }
        }

        output += String(battery.temps.therm1) + ",";
        output += String(battery.temps.therm2) + ",";
        output += String(battery.temps.therm3) + ",";
        output += String(battery.temps.therm4) + ",";
        output += String(battery.temps.thermFET) + ",";
        output += String(battery.temps.thermBalBot) + ",";
        output += String(battery.temps.thermBalTop) + ",";

        output += String(battery.current) + ",";

        output += battery.faults.batteryMinVoltage ? "1" : "0";
        output += battery.faults.batteryMaxVoltage ? "1" : "0";
        output += battery.faults.batteryAverageVoltage ? "1" : "0";
        output += battery.faults.batteryVoltageDiff ? "1" : "0";
        output += battery.faults.batteryTherm1Temp ? "1" : "0";
        output += battery.faults.batteryTherm2Temp ? "1" : "0";
        output += battery.faults.batteryTherm3Temp ? "1" : "0";
        output += battery.faults.batteryTherm4Temp ? "1" : "0";
        output += battery.faults.batteryCurrent ? "1" : "0";
        output += battery.faults.overPower ? "1" : "0";
        output += battery.faults.coreZeroWatch ? "1" : "0";

        output += "\n";

        return output;
    }

    void writeToFile(File &file)
    {
        xSemaphoreTake(xMutex, portMAX_DELAY);

        String output = generateLogLine();

        xSemaphoreGive(xMutex);

        file.print(output);
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    void sendDefaultHTML()
    {
        server.setContentLength(strlen_P(INDEX_HTML));
        server.send(200, "text/html", ""); // Start response

        // Send content in chunks, too big to send as one constant string
        const char *ptr = INDEX_HTML;
        char buffer[512];
        size_t len = strlen_P(INDEX_HTML);

        while (len > 0)
        {
            size_t chunkSize = (len < sizeof(buffer)) ? len : sizeof(buffer);
            memcpy_P(buffer, ptr, chunkSize);
            server.sendContent(buffer, chunkSize);
            ptr += chunkSize;
            len -= chunkSize;
        }
    }

    void handleDefault()
    {
        if (checkCorsPreflight())
            return;

        sendDefaultHTML();
    }

    void handleRoot()
    {
        if (checkCorsPreflight())
            return;

        // If there is a SPIFFS version, use that. Otherwise, use the default embedded HTML.
        if (SPIFFS.exists(HTML_FILE))
        {
            File html_file = SPIFFS.open(HTML_FILE, "r");
            server.streamFile(html_file, "text/html");
            html_file.close();
        }
        else
        {
            sendDefaultHTML();
        }
    }

    FILE fFrontend; // Global variable to handle the file upload

    void handleFrontend(httpd_req_t *req)
    {
        if (checkCorsPreflight(req))
        {
            return;
        }

        std::vector<uint8_t> body;
        esp_err_t read_status = read_request_body(req, body);
        if (read_status == ESP_ERR_INVALID_SIZE)
        {
            send_text(req, "413 Payload Too Large", "text/plain", "error");
            return;
        }
        if (read_status != ESP_OK)
        {
            send_text(req, "400 Bad Request", "text/plain", "error");
            return;
        }

        std::string filename;
        const uint8_t *file_data = nullptr;
        size_t file_size = 0;

        std::string content_type = content_type_header(req);
        if (!parse_multipart_file(body, content_type, "data", filename, file_data, file_size))
        {
            send_text(req, "400 Bad Request", "text/plain", "error");
            return;
        }

        std::FILE *file = std::fopen(HTML_FILE, "wb");
        if (file == nullptr)
        {
            send_text(req, "500 Internal Server Error", "text/plain", "error");
            return;
        }

        size_t written = std::fwrite(file_data, 1, file_size, file);
        std::fclose(file);

        send_text(req, "200 OK", "text/plain", written == file_size ? "ok" : "error");
        return;
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    void handleFileUpload(httpd_req_t *req)
    {
        if (checkCorsPreflight(req))
        {
            return ESP_OK;
        }

        std::vector<uint8_t> body;
        esp_err_t read_status = read_request_body(req, body);
        if (read_status == ESP_ERR_INVALID_SIZE)
        {
            send_text(req, "413 Payload Too Large", "text/plain", "error");
            return;
        }
        if (read_status != ESP_OK)
        {
            send_text(req, "400 Bad Request", "text/plain", "error");
            return;
        }

        std::string filename;
        const uint8_t *file_data = nullptr;
        size_t file_size = 0;

        std::string content_type = content_type_header(req);
        if (!parse_multipart_file(body, content_type, "data", filename, file_data, file_size))
        {
            send_text(req, "400 Bad Request", "text/plain", "error");
            return;
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
            send_text(req, "500 Internal Server Error", "text/plain", "error");
            return;
        }

        size_t written = std::fwrite(file_data, 1, file_size, file);
        std::fclose(file);

        send_text(req, "200 OK", "text/plain", written == file_size ? "ok" : "error");
        return;
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
    void handleNotFound(httpd_req_t *req, httpd_err_code_t err)
    {
        if (checkCorsPreflight())
            return;

        if (!handleFileRead(req, req->uri))
        { // check if the file exists in the flash memory (SPIFFS), if so, send it
            send_text(req, "404 Not Found", "text/plain", "404: File Not Found");
        }
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
}
#endif