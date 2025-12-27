#ifndef BATTERY_PINS_HPP
#define BATTERY_PINS_HPP


namespace pins {
    namespace LTC {
        constexpr size_t THERM1 = 2;
        constexpr size_t THERM2 = 3;
        constexpr size_t THERM3 = 4;
        constexpr size_t THERM4 = 5;
        constexpr size_t THERM_FET = 1;
        constexpr size_t THERM_BAL_BOT = 2;
        constexpr size_t THERM_BAL_TOP = 3;
        constexpr size_t CURRENT = 1;
    } // namespace LTC

    namespace ESP {
        constexpr size_t CONTACTOR = 33;
        constexpr size_t BUZZER = 25;
        constexpr size_t GS0 = 14;
        constexpr size_t GS1 = 13;
        constexpr size_t SS_SWITCH = 16;
        constexpr size_t PWR_EN = 22;
        constexpr size_t LED = 32;
        constexpr size_t CAN_TX = 26;
        constexpr size_t CAN_RX = 27;
        constexpr size_t CAN_ON = 4;
        constexpr size_t CAN_S = 17;
    } // namespace ESP
} // namespace pins


#endif // BATTERY_PINS_HPP