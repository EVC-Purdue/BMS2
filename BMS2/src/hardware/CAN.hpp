#ifndef CAN_HPP
#define CAN_HPP

#include <cstdint>
#include "freertos/FreeRTOS.h"

namespace can
{
    // Full 29-bit extended CAN IDs per CH4100/TSM2500 protocol
    constexpr uint32_t BMS_ID = 0x18E54024;     // BMS → Charger
    constexpr uint32_t CHARGER_ID = 0x18EB2440; // Charger → BMS

    // Struct for BMS to Charger message
    struct BMSChargingInfo
    {
        uint8_t chargerControl;    // Bit 0: Start charging, Bit 1: Stop charging
        uint16_t chargingVoltage;  // In 0.1V units (little-endian)
        uint16_t chargingCurrent;  // In 0.1A units, offset -3200 (little-endian)
        uint8_t chargerLEDDisplay; // LED display state
    };

    // Struct for Charger to BMS message
    struct ChargerStatus
    {
        bool highTempFault;
        bool inputVoltageFault;
        bool hardwareErrorFault;
        bool communicationFault;
        bool hasFault;

        bool batteryConnected;
        bool chargingActive;
        float outputVoltage;
        float outputCurrent;
        int16_t temperature; // Temp of charger in °C (range −40–150)
    };

    void init();

    // Charger communication API
    bool receiveChargerStatus(ChargerStatus &status, TickType_t timeoutTicks = 0);
    void requestCharging(float voltage, float current, bool startCharging, uint8_t ledDisplay = 0);
    void stopCharging();

    // Returns true when a charger was newly detected and a charge request was transmitted.
    bool autoRequestChargingOnPlugIn(float voltage, float current, uint8_t ledDisplay = 0, TickType_t rxTimeoutTicks = 0);
}

#endif // CAN_HPP