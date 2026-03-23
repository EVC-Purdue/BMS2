#include "driver/twai.h"
#include "CAN.hpp"
#include "pins.hpp"
#include "string.h"
#include "freertos/task.h"
#include <cstdio>

namespace can
{
    namespace
    {
        constexpr TickType_t CHARGER_OFFLINE_TIMEOUT_TICKS = pdMS_TO_TICKS(1500);

        TickType_t s_lastChargerSeenTick = 0;
        bool s_chargerOnline = false;
        bool s_wasChargerOnline = false;
        bool s_autoRequestSent = false;
        ChargerStatus s_lastReceivedChargerStatus = {};

        void updateChargerOnlineState()
        {
            const TickType_t now = xTaskGetTickCount();
            if (s_chargerOnline && ((now - s_lastChargerSeenTick) > CHARGER_OFFLINE_TIMEOUT_TICKS))
            {
                s_chargerOnline = false;
                s_autoRequestSent = false;
            }
        }
    } // namespace

    void init()
    {
        twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
            pins::ESP::CAN_TX, pins::ESP::CAN_RX, TWAI_MODE_NORMAL);
        twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
        twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
        ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
        ESP_ERROR_CHECK(twai_start());

        s_lastChargerSeenTick = 0;
        s_chargerOnline = false;
        s_autoRequestSent = false;
        printf("CAN initialized\n");
    }

    void send_message(uint32_t id, uint8_t data[], uint8_t len)
    {
        twai_message_t message = {};
        message.extd = 1;
        message.identifier = id;
        message.data_length_code = len;
        memcpy(message.data, data, len);
        twai_transmit(&message, portMAX_DELAY);
    }

    bool receive_message(uint32_t *id, uint8_t data[], uint8_t *len, TickType_t timeoutTicks)
    {
        twai_message_t message = {};
        const esp_err_t ret = twai_receive(&message, timeoutTicks);
        if (ret == ESP_ERR_TIMEOUT)
        {
            return false;
        }
        if (ret != ESP_OK)
        {
            return false;
        }

        *id = message.identifier;
        *len = (message.data_length_code > 8U) ? 8U : message.data_length_code;
        memcpy(data, message.data, *len);
        return true;
    }

    uint16_t encodeVoltage(float voltage)
    {
        return static_cast<uint16_t>(voltage * 10.0f);
    }

    float decodeVoltage(uint16_t encoded)
    {
        return encoded * 0.1f;
    }

    uint16_t encodeCurrent(float current)
    {
        return static_cast<uint16_t>((current + 3200.0f) * 10.0f);
    }

    float decodeCurrent(uint16_t encoded)
    {
        return (encoded * 0.1f) - 3200.0f;
    }

    int16_t decodeTemperature(uint8_t encoded)
    {
        return static_cast<int16_t>(encoded) - 40;
    }

    void sendChargingInfo(const BMSChargingInfo &info)
    {
        uint8_t data[8] = {0};
        data[0] = info.chargerControl;
        data[1] = info.chargingVoltage & 0xFF;
        data[2] = (info.chargingVoltage >> 8) & 0xFF;
        data[3] = info.chargingCurrent & 0xFF;
        data[4] = (info.chargingCurrent >> 8) & 0xFF;
        data[5] = info.chargerLEDDisplay;
        send_message(BMS_ID, data, 8);
    }

    bool receiveChargerStatus(ChargerStatus &status, TickType_t timeoutTicks)
    {
        uint32_t id = 0;
        uint8_t data[8] = {0};
        uint8_t len = 0;

        if (!receive_message(&id, data, &len, timeoutTicks))
        {
            updateChargerOnlineState();
            return false;
        }

        if (id != CHARGER_ID || len != 8)
        {
            updateChargerOnlineState();
            return false;
        }

        status.highTempFault = (data[0] & 0b11000000) != 0;
        status.inputVoltageFault = (data[0] & 0b00110000) != 0;
        status.hardwareErrorFault = (data[0] & 0b00001100) != 0;
        status.communicationFault = (data[0] & 0b00000011) != 0;
        status.hasFault = data[0] != 0;

        status.batteryConnected = (data[1] & 0b00001100) == 0;
        status.chargingActive = (data[1] & 0b00000011) == 0;

        status.outputVoltage = decodeVoltage(static_cast<uint16_t>(data[3]) << 8 | data[2]);
        status.outputCurrent = decodeCurrent(static_cast<uint16_t>(data[5]) << 8 | data[4]);
        status.temperature = decodeTemperature(data[6]);
        if (status.temperature > 150)
        {
            std::printf("Warning: Received charger temperature of %dC, which is above expected maximum.\n",
                        status.temperature);
        }

        s_lastChargerSeenTick = xTaskGetTickCount();
        s_chargerOnline = true;

        s_lastReceivedChargerStatus = status;
        return true;
    }

    void requestCharging(float voltage, float current, bool startCharging, uint8_t ledDisplay)
    {
        BMSChargingInfo info = {};
        info.chargerControl = startCharging ? 0 : 1;
        info.chargingVoltage = encodeVoltage(voltage);
        info.chargingCurrent = encodeCurrent(current);
        info.chargerLEDDisplay = ledDisplay;
        sendChargingInfo(info);
    }

    void stopCharging()
    {
        requestCharging(0.0f, 0.0f, false);
    }

    bool autoRequestChargingOnPlugIn(float voltage, float current, uint8_t ledDisplay, TickType_t rxTimeoutTicks)
    {
        ChargerStatus status = {};
        const bool hasFrame = receiveChargerStatus(status, rxTimeoutTicks);

        // Detect offline -> online transition
        const bool justWentOnline = !s_wasChargerOnline && s_chargerOnline;
        s_wasChargerOnline = s_chargerOnline;

        if (justWentOnline)
        {
            s_autoRequestSent = false; // Reset flag when charger plugs in
        }

        if (!s_autoRequestSent && s_chargerOnline && hasFrame)
        {
            requestCharging(voltage, current, true, ledDisplay);
            s_autoRequestSent = true;
            s_lastChargerSeenTick = xTaskGetTickCount();
            return true;
        }

        return false;
    }
} // namespace can