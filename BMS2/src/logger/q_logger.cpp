#include <variant>

#include "freertos/FreeRTOS.h"


namespace q_logger {

QueueHandle_t g_logger_queue = nullptr;

}