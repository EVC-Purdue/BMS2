#if false // disable entire file for now, as it's not being used and is incomplete

#include "logger/Web/web.hpp"
#include "stdint.h"
#include "battery/battery.hpp"
#include "battery/parameters.hpp"
#include "esp_http_server.h"

namespace web
{
    void setupServer() {
        server.on("/", HTTP_GET, handleRoot);
        server.on("/default", HTTP_GET, handleDefault);
        server.on("/frontend", HTTP_POST, handleFrontend, []() { server.send(200, "text/plain", ""); });

        server.on("/name", HTTP_GET, handleName);

        server.on("/readCells", HTTP_GET, handleReadCells);
        server.on("/data", HTTP_GET, handleData);

        server.on("/state", HTTP_GET, handleState);

        server.on("/state/balancing", HTTP_GET, handleBalancing);
        server.on("/state/monitor",   HTTP_GET, handleMonitor);
        server.on("/state/idle",      HTTP_GET, handleIdle);

        server.on(UriBraces("/canMode/{}"), HTTP_GET, handleCanMode);

        server.on("/fullShutdown", HTTP_GET, handleFullShutdown);

        server.on("/upload", HTTP_POST, handleFileUpload, []() { server.send(200, "text/plain", ""); });

        server.on("/log/download", HTTP_GET, handleLogDownload);
        server.on("/log/delete",   HTTP_GET, handleLogDelete);

        server.on(UriBraces("/parameters/{}/{}"), handleParameters);

        server.on(UriBraces("/acknowledge/{}"), handleAcknowledge);

        server.on("/forceDischarge/enable",  HTTP_GET, hangleForceDischargeEnable);
        server.on("/forceDischarge/disable", HTTP_GET, handleForceDischargeDisable);

        server.onNotFound(handleNotFound);

        server.begin();
    }

    // -------------------------------------------------------------------------- //
    // https://github.com/espressif/arduino-esp32/blob/master/libraries/WebServer/src/middleware/CorsMiddleware.cpp#L36
    bool checkCorsPreflight() {
        if (server.hasHeader("Origin")) {
            // server.sendHeader("Access-Control-Allow-Origin", "*");
            // server.sendHeader("Access-Control-Max-Age", "86400");
            // server.sendHeader("Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS");
            // // server.sendHeader("Access-Control-Allow-Headers", "*");
            // server.sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
            // server.sendHeader("Access-Control-Allow-Credentials", "true");

            server.sendHeader("Access-Control-Allow-Origin", "*");
            server.sendHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            
            if (server.method() == HTTP_OPTIONS) {
                server.send(204);
                return true;
            }
            // return false;
        }
        return false;
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    void handleParameters() {
        if (checkCorsPreflight()) return;

        String p = server.pathArg(0);
        String v = server.pathArg(1);

        xSemaphoreTake(xMutex, portMAX_DELAY);

        String r = "ok -> ";

        if      (p == "bypass")    { parameters.bypass = v == "true";     r += parameters.bypass;    }
        else if (p == "vBypass")   { parameters.vBypass = v.toFloat();    r += parameters.vBypass;   }
        else if (p == "vMin")      { parameters.vMin = v.toFloat();       r += parameters.vMin;      }
        else if (p == "vMax")      { parameters.vMax = v.toFloat();       r += parameters.vMax;      }
        else if (p == "vMinAvg")   { parameters.vMinAvg = v.toFloat();    r += parameters.vMinAvg;   }
        else if (p == "vMaxAvg")   { parameters.vMaxAvg = v.toFloat();    r += parameters.vMinAvg;   }
        else if (p == "vDiff")     { parameters.vDiff = v.toFloat();      r += parameters.vDiff;     }
        else if (p == "tMin")      { parameters.tMin = v.toFloat();       r += parameters.tMin;      }
        else if (p == "tMax")      { parameters.tMax = v.toFloat();       r += parameters.tMax;      }
        else if (p == "tDiff")     { parameters.tDiff = v.toFloat();      r += parameters.tDiff;     }
        else if (p == "tMaxBal")   { parameters.tMaxBal = v.toFloat();    r += parameters.tMaxBal;   }
        else if (p == "tResetBal") { parameters.tResetBal = v.toFloat();  r += parameters.tResetBal; }
        else if (p == "logSpeed")  { parameters.logSpeed = v.toInt();     r += parameters.logSpeed;  }
        else if (p == "deleteLog") { parameters.deleteLog = v == "true";  r += parameters.deleteLog; }
        else if (p == "vCanCharge"){ parameters.vCanCharge = v.toFloat(); r += parameters.vCanCharge;}
        else if (p == "iCanCharge"){ parameters.iCanCharge = v.toFloat(); r += parameters.iCanCharge;}
        else                       { r = "parameter not found"; }

        xSemaphoreGive(xMutex);

        server.send(200, "text/plain", r);
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    void handleAcknowledge() {
        if (checkCorsPreflight()) return;

        String f = server.pathArg(0);

        xSemaphoreTake(xMutex, portMAX_DELAY);

        String r = "ok";

        if      (f == "batteryMinVoltage")    { battery.pFaults.batteryMinVoltage     = false; }
        else if (f == "batteryMaxVoltage")    { battery.pFaults.batteryMaxVoltage     = false; }
        else if (f == "batteryAverageVoltage"){ battery.pFaults.batteryAverageVoltage = false; }
        else if (f == "batteryVoltageDiff")   { battery.pFaults.batteryVoltageDiff    = false; }
        else if (f == "batteryTherm1Temp")    { battery.pFaults.batteryTherm1Temp     = false; }
        else if (f == "batteryTherm2Temp")    { battery.pFaults.batteryTherm2Temp     = false; }
        else if (f == "batteryTherm3Temp")    { battery.pFaults.batteryTherm3Temp     = false; }
        else if (f == "batteryTherm4Temp")    { battery.pFaults.batteryTherm4Temp     = false; }
        else if (f == "batteryCurrent")       { battery.pFaults.batteryCurrent        = false; }
        else if (f == "overPower")            { battery.pFaults.overPower             = false; }
        else if (f == "coreZeroWatch")        { battery.pFaults.coreZeroWatch         = false; }
        else                                  { r = "fault not found"; }

        xSemaphoreGive(xMutex);

        server.send(200, "text/plain", r);
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    void handleCanMode() {
        if (checkCorsPreflight()) return;

        String m = server.pathArg(0);

        xSemaphoreTake(xMutex, portMAX_DELAY);

        if (m == "off") {
            if (canMode != CanMode::OFF) {
                stopCan();
            }

            canMode = CanMode::OFF;
            digitalWrite(CAN_ON_GPIO, LOW);
            server.send(200, "text/plain", "off");
        } else {
            if (canMode == CanMode::OFF) {
                startCan();
            }

            digitalWrite(CAN_ON_GPIO, HIGH);

            if (m == "charge") {
                canMode = CanMode::CHARGE;
                server.send(200, "text/plain", "charge");
            } else if (m == "vesc") {
                canMode = CanMode::VESC;
                server.send(200, "text/plain", "vesc");
            } else if (m == "sevcon") {
                canMode = CanMode::SEVCON;
                server.send(200, "text/plain", "sevcon");
            } else {
                server.send(400, "text/plain", "invalid mode");
            }
        }

        xSemaphoreGive(xMutex);
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    void handleFullShutdown() {
        if (checkCorsPreflight()) return;

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
    void hangleForceDischargeEnable() {
        if (checkCorsPreflight()) return;

        digitalWrite(CONTACTOR_GPIO, HIGH);

        xSemaphoreTake(xMutex, portMAX_DELAY);
        buzzOn = false;
        xSemaphoreGive(xMutex);

        ledcWrite(0, 0);
        // digitalWrite(BUZZER_GPIO, LOW);
        server.send(200, "text/plain", "enable");
    }

    void handleForceDischargeDisable() {
        if (checkCorsPreflight()) return;

        digitalWrite(CONTACTOR_GPIO, LOW);
        digitalWrite(SS_SWITCH_GPIO, LOW);

        xSemaphoreTake(xMutex, portMAX_DELAY);
        buzzOn = true;
        xSemaphoreGive(xMutex);
        
        server.send(200, "text/plain", "disable");
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    void handleLogDownload() {
        if (checkCorsPreflight()) return;

        uint8_t heartbeat = HEARTBEAT_VALUE_LONG;
        xQueueSend(heartbeatQueue, &heartbeat, HEARTBEAT_SEND_DELAY);
        File file = SPIFFS.open(LOG_FILE, "r");
        server.streamFile(file, "text/csv");
        file.close();
    }

    void handleLogDelete() {
        if (checkCorsPreflight()) return;

        uint8_t heartbeat = HEARTBEAT_VALUE_LONG;
        xQueueSend(heartbeatQueue, &heartbeat, HEARTBEAT_SEND_DELAY);
        bool success = SPIFFS.remove(LOG_FILE);
        server.send(200, "text/plain", success ? "ok" : "error");
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    void handleName() {
        if (checkCorsPreflight()) return;

        server.send(200, "text/plain", apSSID);
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    void handleReadCells() {
        String output = "apSSID: ";
        output += apSSID;
        output += "\n";

        xSemaphoreTake(xMutex, portMAX_DELAY);

        for (int current_ic = 0; current_ic < TOTAL_IC; current_ic++) {
            for (int i = 0; i < bms_ic[0].ic_reg.cell_channels; i++) {
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
        while (it != NULL) {
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

    void handleData() {
        if (checkCorsPreflight()) return;

        // TODO: not sure if I am using ArduinoJson correctly
        // It compiles be it might throw an error at runtime
        JsonDocument doc;

        xSemaphoreTake(xMutex, portMAX_DELAY);

        doc["cells"] = JsonArray();
        doc["discharge"] = JsonArray();

        for (int current_ic = 0; current_ic < TOTAL_IC; current_ic++) {
            doc["discharge"][current_ic] = battery.pack[current_ic].discharge;

            doc["cells"][current_ic] = JsonArray();        
            for (int i = 0; i < bms_ic[0].ic_reg.cell_channels; i++) {
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

        doc["therm"]["1"]      = battery.temps.therm1;
        doc["therm"]["2"]      = battery.temps.therm2;
        doc["therm"]["3"]      = battery.temps.therm3;
        doc["therm"]["4"]      = battery.temps.therm4;
        doc["therm"]["FET"]    = battery.temps.thermFET;
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

        doc["faults"]["batteryMinVoltage"]     = battery.faults.batteryMinVoltage;
        doc["faults"]["batteryMaxVoltage"]     = battery.faults.batteryMaxVoltage;
        doc["faults"]["batteryAverageVoltage"] = battery.faults.batteryAverageVoltage;
        doc["faults"]["batteryVoltageDiff"]    = battery.faults.batteryVoltageDiff;
        doc["faults"]["batteryTherm1Temp"]     = battery.faults.batteryTherm1Temp;
        doc["faults"]["batteryTherm2Temp"]     = battery.faults.batteryTherm2Temp;
        doc["faults"]["batteryTherm3Temp"]     = battery.faults.batteryTherm3Temp;
        doc["faults"]["batteryTherm4Temp"]     = battery.faults.batteryTherm4Temp;
        doc["faults"]["batteryCurrent"]        = battery.faults.batteryCurrent;
        doc["faults"]["overPower"]             = battery.faults.overPower;
        doc["faults"]["coreZeroWatch"]         = battery.faults.coreZeroWatch;

        doc["pFaults"]["batteryMinVoltage"]     = battery.pFaults.batteryMinVoltage;
        doc["pFaults"]["batteryMaxVoltage"]     = battery.pFaults.batteryMaxVoltage;
        doc["pFaults"]["batteryAverageVoltage"] = battery.pFaults.batteryAverageVoltage;
        doc["pFaults"]["batteryVoltageDiff"]    = battery.pFaults.batteryVoltageDiff;
        doc["pFaults"]["batteryTherm1Temp"]     = battery.pFaults.batteryTherm1Temp;
        doc["pFaults"]["batteryTherm2Temp"]     = battery.pFaults.batteryTherm2Temp;
        doc["pFaults"]["batteryTherm3Temp"]     = battery.pFaults.batteryTherm3Temp;
        doc["pFaults"]["batteryTherm4Temp"]     = battery.pFaults.batteryTherm4Temp;
        doc["pFaults"]["batteryCurrent"]        = battery.pFaults.batteryCurrent;
        doc["pFaults"]["overPower"]             = battery.pFaults.overPower;
        doc["pFaults"]["coreZeroWatch"]         = battery.pFaults.coreZeroWatch;

        doc["name"] = apSSID;

        xSemaphoreGive(xMutex);

        String output;
        serializeJson(doc, output);
        server.send(200, "application/json", output);
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    void handleIdle() {
        if (checkCorsPreflight()) return;

        xSemaphoreTake(xMutex, portMAX_DELAY);
        
        clear_discharge(TOTAL_IC, bms_ic);
        wakeup_sleep(TOTAL_IC);
        LTC6811_wrcfg(TOTAL_IC, bms_ic);

        for (int current_ic = 0; current_ic < TOTAL_IC; current_ic++) {
            battery.pack[current_ic].discharge = 0;
            bool gpio[5];
            // gpio[GPIO_BACKBAL] = 0;
            LTC681x_set_cfgr_gpio(current_ic,bms_ic,gpio);
        }

        runState = IDLE;

        xSemaphoreGive(xMutex);

        digitalWrite(CONTACTOR_GPIO, LOW);
        digitalWrite(SS_SWITCH_GPIO, LOW);
        server.send(200, "text/plain", printState(runState));

    }

    void handleMonitor() {
        if (checkCorsPreflight()) return;

        xSemaphoreTake(xMutex, portMAX_DELAY);

        clear_discharge(TOTAL_IC, bms_ic);
        wakeup_sleep(TOTAL_IC);
        LTC6811_wrcfg(TOTAL_IC, bms_ic);

        for (int current_ic = 0; current_ic < TOTAL_IC; current_ic++) {
            battery.pack[current_ic].discharge = 0;
            bool gpio[5];
            // gpio[GPIO_BACKBAL] = 0;
            LTC681x_set_cfgr_gpio(current_ic,bms_ic,gpio);
        }

        runState = MONITOR;

        xSemaphoreGive(xMutex);

        digitalWrite(CONTACTOR_GPIO, HIGH);
        server.send(200, "text/plain", printState(runState));
    }

    void handleBalancing() {
        if (checkCorsPreflight()) return;

        xSemaphoreTake(xMutex, portMAX_DELAY);
        runState = BALANCING;
        xSemaphoreGive(xMutex);
        
        digitalWrite(SS_SWITCH_GPIO, HIGH);
        server.send(200, "text/plain", printState(runState));
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    void handleState() {
        if (checkCorsPreflight()) return;

        float fileTotalKB = (float)SPIFFS.totalBytes() / 1024.0; 
        float fileUsedKB  = (float)SPIFFS.usedBytes()  / 1024.0; 
        
        // float flashChipSize = (float)ESP.getFlashChipSize() / 1024.0 / 1024.0;
        // float realFlashChipSize = (float)ESP.getFlashChipRealSize() / 1024.0 / 1024.0;
        // float flashFreq = (float)ESP.getFlashChipSpeed() / 1000.0 / 1000.0;

        xSemaphoreTake(xMutex, portMAX_DELAY);

        int index = 0;
        char response[300];
        index = snprintf(response, 300, "%s<br>%.2f/%.2f KB" ,printState(runState), fileUsedKB ,fileTotalKB);

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

    const char* printState(RunState state) {
        switch (state) {
            case IDLE:     return "idle";
            case MONITOR:  return "monitor";
            case BALANCING: return "balancing";
            default:       return "STATE ERROR";   
        }
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    bool handleFileRead(String path) { // send the right file to the client (if it exists)
        Serial.println("handleFileRead: " + path);
        if (path.endsWith("/")) path += "index.html";          // If a folder is requested, send the index file
        String contentType = getContentType(path);             // Get the MIME type
        String pathWithGz = path + ".gz";
        if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) { // If the file exists, either as a compressed archive, or normal
            if (SPIFFS.exists(pathWithGz)) {                    // If there's a compressed version available
                path += ".gz";                                  // Use the compressed verion
            }
            File file = SPIFFS.open(path, "r");                 // Open the file
            size_t sent = server.streamFile(file, contentType); // Send it to the client
            file.close();                                       // Close the file again
            Serial.println(String("\tSent file: ") + path);
            return true;
        }
        Serial.println(String("\tFile Not Found: ") + path);    // If the file doesn't exist, return false
        return false;
    }

    String getContentType(String filename) { // determine the filetype of a given filename, based on the extension
        if (filename.endsWith(".html")) return "text/html";
        else if (filename.endsWith(".css")) return "text/css";
        else if (filename.endsWith(".js")) return "application/javascript";
        else if (filename.endsWith(".ico")) return "image/x-icon";
        else if (filename.endsWith(".gz")) return "application/x-gzip";
        return "text/plain";
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    void saveData() {
        // Make sure we create the file
        File cellLog = SPIFFS.open(LOG_FILE, FILE_READ);
        if (!cellLog) {
            cellLog = SPIFFS.open(LOG_FILE, FILE_WRITE);
            cellLog.close();
        }

        bool logNow = true;
        float used_percent = (float)SPIFFS.usedBytes() / (float)SPIFFS.totalBytes();
        if (used_percent > LOG_CAPCITY_PERCENT) {
            Serial.println("SPIFFS is full!");

            xSemaphoreTake(xMutex, portMAX_DELAY);
            bool deleteLog = parameters.deleteLog;
            xSemaphoreGive(xMutex);

            if (deleteLog) {
                SPIFFS.remove(LOG_FILE);
                Serial.println("Log Deleted");
            } else {
                logNow = false;
            }   
        }

        if (logNow) {
            File cellLog = SPIFFS.open(LOG_FILE, FILE_APPEND); // APPEND = add

            for (int i = 0; i < battery.logFaults.count; i++) {
                cellLog.print(battery.logFaults.lines[i]);
            }
            battery.logFaults.count = 0;

            writeToFile(cellLog);
            cellLog.close();
            Serial.println("Data Logged");
        }
    }

    String generateLogLine() {
        String output = String(millis()) + ",";

        for (int current_ic = 0; current_ic < TOTAL_IC; current_ic++) {
            for (int i = 0; i < bms_ic[0].ic_reg.cell_channels; i++) {
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

        output += battery.faults.batteryMinVoltage     ? "1" : "0";
        output += battery.faults.batteryMaxVoltage     ? "1" : "0";
        output += battery.faults.batteryAverageVoltage ? "1" : "0";
        output += battery.faults.batteryVoltageDiff    ? "1" : "0";
        output += battery.faults.batteryTherm1Temp     ? "1" : "0";
        output += battery.faults.batteryTherm2Temp     ? "1" : "0";
        output += battery.faults.batteryTherm3Temp     ? "1" : "0";
        output += battery.faults.batteryTherm4Temp     ? "1" : "0";
        output += battery.faults.batteryCurrent        ? "1" : "0";
        output += battery.faults.overPower             ? "1" : "0";
        output += battery.faults.coreZeroWatch         ? "1" : "0";

        output += "\n";

        return output;
    }

    void writeToFile(File& file) {
        xSemaphoreTake(xMutex, portMAX_DELAY);

        String output = generateLogLine();

        xSemaphoreGive(xMutex);

        file.print(output);
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    void sendDefaultHTML() {
        server.setContentLength(strlen_P(INDEX_HTML));
        server.send(200, "text/html", ""); // Start response

        // Send content in chunks, too big to send as one constant string
        const char* ptr = INDEX_HTML;
        char buffer[512];
        size_t len = strlen_P(INDEX_HTML);

        while (len > 0) {
            size_t chunkSize = (len < sizeof(buffer)) ? len : sizeof(buffer);
            memcpy_P(buffer, ptr, chunkSize);
            server.sendContent(buffer, chunkSize);
            ptr += chunkSize;
            len -= chunkSize;
        }
    }

    void handleDefault() {
        if (checkCorsPreflight()) return;

        sendDefaultHTML();
    }

    void handleRoot() {
        if (checkCorsPreflight()) return;

        // If there is a SPIFFS version, use that. Otherwise, use the default embedded HTML.
        if (SPIFFS.exists(HTML_FILE)) {
            File html_file = SPIFFS.open(HTML_FILE, "r");
            server.streamFile(html_file, "text/html");
            html_file.close();
        } else {
            sendDefaultHTML();
        }
    }


    File fFrontend; // Global variable to handle the file upload

    void handleFrontend() {
        if (checkCorsPreflight()) return;

        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            if (SPIFFS.exists(HTML_FILE)) {
                SPIFFS.remove(HTML_FILE);
            }
            fFrontend = SPIFFS.open(HTML_FILE, "w");
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (fFrontend) {
                fFrontend.write(upload.buf, upload.currentSize);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (fFrontend) {
                fFrontend.close();
                server.send(200, "text/plain", "ok");
            } else {
                server.send(200, "text/plain", "error");
            }
        }
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    File fsUploadFile; // a File variable to temporarily store the received file

    void handleFileUpload() { // upload a new file to the SPIFFS
        if (checkCorsPreflight()) return;

        HTTPUpload& upload = server.upload();
        String path;
        if (upload.status == UPLOAD_FILE_START) {
            path = upload.filename;
            if (!path.startsWith("/")) path = "/" + path;
            if (!path.endsWith(".gz")) {                         // The file server always prefers a compressed version of a file
                String pathWithGz = path + ".gz";                // So if an uploaded file is not compressed, the existing compressed
                if (SPIFFS.exists(pathWithGz)) {                 // version of that file must be deleted (if it exists)
                    SPIFFS.remove(pathWithGz);
                }
            }
            Serial.print("handleFileUpload Name: "); Serial.println(path);
            fsUploadFile = SPIFFS.open(path, "w");               // Open the file for writing in SPIFFS (create if it doesn't exist)
            path = String();
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (fsUploadFile) {
                fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (fsUploadFile) {                                   // If the file was successfully created
                fsUploadFile.close();                               // Close the file again
                Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
                server.send(200, "text/plain", "ok"); // Send a success response
            } else {
                server.send(200, "text/plain", "error");
            }
        }
    }
    // -------------------------------------------------------------------------- //

    // -------------------------------------------------------------------------- //
    void handleNotFound() {
        if (checkCorsPreflight()) return;

        if (!handleFileRead(server.uri())) {        // check if the file exists in the flash memory (SPIFFS), if so, send it
            server.send(404, "text/plain", "404: File Not Found");
        }
    }
    // -------------------------------------------------------------------------- //
}
#endif