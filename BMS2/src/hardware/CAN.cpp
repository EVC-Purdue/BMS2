#include "driver/twai.h"
#include "CAN.hpp"
#include "pins.hpp"
#include "string.h"

namespace can
{
    void init()
    {
        twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
            pins::ESP::CAN_TX, pins::ESP::CAN_RX, TWAI_MODE_NORMAL);
        twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
        twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
        ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
        ESP_ERROR_CHECK(twai_start());
    }

    void send_message(uint32_t id, uint8_t data[], uint8_t len)
    {
        // Send a CAN message with the specified ID and data
        twai_message_t message = {};
        message.extd = 1; // 29-bit extended frame (CAN 2.0B / J1939)
        message.identifier = id;
        message.data_length_code = len;
        memcpy(message.data, data, len);
        twai_transmit(&message, portMAX_DELAY);
    }

    void receive_message(uint32_t *id, uint8_t data[], uint8_t *len)
    {
        // Receive a CAN message and populate the provided ID, data, and length
        twai_message_t message;
        twai_receive(&message, portMAX_DELAY);
        *id = message.identifier;
        *len = message.data_length_code;
        memcpy(data, message.data, *len);
    }

    // Helper functions for encoding/decoding values
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

    uint8_t encodeTemperature(int16_t temp)
    {
        return static_cast<uint8_t>(temp + 40);
    }

    int16_t decodeTemperature(uint8_t encoded)
    {
        return static_cast<int16_t>(encoded) - 40;
    }

    void sendChargingInfo(const BMSChargingInfo &info)
    {
        uint8_t data[8] = {0};
        data[0] = info.chargerControl;
        // Little-endian: LSB first
        data[1] = info.chargingVoltage & 0xFF;
        data[2] = (info.chargingVoltage >> 8) & 0xFF;
        data[3] = info.chargingCurrent & 0xFF;
        data[4] = (info.chargingCurrent >> 8) & 0xFF;
        data[5] = info.chargerLEDDisplay;
        // data[6] and data[7] are reserved (0)
        send_message(BMS_ID, data, 8);
    }

    void receiveChargerStatus(ChargerStatus &status)
    {
        uint32_t id;
        uint8_t data[8]; // TODO: check for potential buffer overflow if len > 8
        uint8_t len;
        receive_message(&id, data, &len);
        if (id == CHARGER_ID && len == 8)
        {
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
                printf("Warning: Received charger temperature of %d°C, \
                    which is above expected maximum.\n ",
                       status.temperature);
            }
            // data[7] is reserved
        }
    }

    void requestCharging(float voltage, float current, bool startCharging, uint8_t ledDisplay)
    { // potentially reccomend 4 for display?
        /*
        ledDisplay codes:
        0x00 R-
        0x01 R
        0x02 Y-
        0x03 Y
        0x04 G-
        0x05 G
        0x06-0XFF R-G-
        Note：
        1."-" represents led that does not light for 0.5s, a color
        word represents that the LED of this color lights for 0.2s.
        2. R --red G —green Y—yellow
        */

        BMSChargingInfo info;
        info.chargerControl = startCharging ? 0 : 1; // yes, the protocol is backwards
        info.chargingVoltage = encodeVoltage(voltage);
        info.chargingCurrent = encodeCurrent(current);
        info.chargerLEDDisplay = ledDisplay;
        sendChargingInfo(info);
    }

    void stopCharging()
    {
        requestCharging(0.0f, 0.0f, false);
    }
}