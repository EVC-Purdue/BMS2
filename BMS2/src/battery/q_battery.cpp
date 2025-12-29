#include <variant>

#include "freertos/FreeRTOS.h"


namespace q_battery {

QueueHandle_t g_battery_queue = nullptr;

}