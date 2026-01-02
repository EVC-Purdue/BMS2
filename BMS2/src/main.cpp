
#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "driver/spi_master.h"
#include "esp_log.h"

#include "hardware/hardware.hpp"
#include "battery/t_battery.hpp"
#include "battery/q_battery.hpp"
#include "logger/t_logger.hpp"
#include "logger/q_logger.hpp"
#include "util/err.hpp"

// Tasks
static t_battery::TBattery gs_t_battery(t_battery::TASK_PERIOD_MS);
static t_logger::TLogger gs_t_logger(t_logger::TASK_PERIOD_MS);

// Task control blocks (TCBs)
static StaticTask_t gs_battery_tcb = {};
static StaticTask_t gs_logger_tcb = {};


// Task stacks
static StackType_t gs_battery_stack[t_battery::TASK_STACK_SIZE];
static StackType_t gs_logger_stack[t_logger::TASK_STACK_SIZE];

// Hardware peripherals
static spi_device_handle_t gs_spi_handle = nullptr;



extern "C" void app_main() {
    // Hardware configuration and setup
    hardware::configure(&gs_spi_handle); // GPIO, SPI, LEDC, SPIFFS
    hardware::setup_initial_gpio_states();

    // Queue initialization
    q_battery::g_battery_queue = xQueueCreate(q_battery::QUEUE_SIZE, sizeof(q_battery::Message));
    UTIL_CHECK_REQUIRE(q_battery::g_battery_queue != nullptr);

    q_logger::g_logger_queue = xQueueCreate(q_logger::QUEUE_SIZE, sizeof(q_logger::Message));
    UTIL_CHECK_REQUIRE(q_logger::g_logger_queue != nullptr);


    // Start tasks
    TaskHandle_t battery_task_handle = xTaskCreateStaticPinnedToCore(
        &t_battery::TBattery::taskWrapper,
        t_battery::TASK_NAME,
        t_battery::TASK_STACK_SIZE,
        &gs_t_battery,
        t_battery::TASK_PRIORITY,
        gs_battery_stack,
        &gs_battery_tcb,
        t_battery::TASK_CORE_ID
    );

    UTIL_CHECK_REQUIRE(battery_task_handle != nullptr);
    TaskHandle_t logger_task_handle = xTaskCreateStaticPinnedToCore(
        &t_logger::TLogger::taskWrapper,
        t_logger::TASK_NAME,
        t_logger::TASK_STACK_SIZE,
        &gs_t_logger,
        t_logger::TASK_PRIORITY,
        gs_logger_stack,
        &gs_logger_tcb,
        t_logger::TASK_CORE_ID
    );
    UTIL_CHECK_REQUIRE(logger_task_handle != nullptr);
}