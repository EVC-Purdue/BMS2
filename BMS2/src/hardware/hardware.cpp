
#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "hardware/pins.hpp"

#include "hardware/hardware.hpp"

namespace hardware {

void configure_gpio_output() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pins::ESP::CONTACTOR) |
                        (1ULL << pins::ESP::SS_SWITCH) |
                        (1ULL << pins::ESP::PWR_EN) |
                        (1ULL << pins::ESP::BUZZER) |
                        (1ULL << pins::ESP::GS0) |
                        (1ULL << pins::ESP::GS1) |
                        (1ULL << pins::ESP::LED) |
                        (1ULL << pins::ESP::CAN_ON) |
                        (1ULL << pins::ESP::CAN_S),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

void configure_spi(spi_device_handle_t* spi_handle) {
    spi_bus_config_t buscfg = {
        .mosi_io_num = pins::ESP::SPI_MOSI,
        .miso_io_num = pins::ESP::SPI_MISO,
        .sclk_io_num = pins::ESP::SPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1,
        .data_io_default_level = 0,
        .max_transfer_sz = MAX_SPI_TRANSFER_SZ,
        .flags = 0,
        .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,
        .intr_flags = 0,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {};
    devcfg.mode = SPI_MODE;
    devcfg.clock_speed_hz = SPI_CLOCK_SPEED_HZ;
    devcfg.spics_io_num = -1;
    devcfg.queue_size = 1;
    spi_bus_add_device(SPI2_HOST, &devcfg, spi_handle);
}

void configure(spi_device_handle_t* spi_handle) {
    configure_gpio_output();
    configure_spi(spi_handle);
}

void setup_initial_gpio_states() {
    gpio_set_level(pins::ESP::PWR_EN, 1);
    gpio_set_level(pins::ESP::LED, 1);
    gpio_set_level(pins::ESP::SS_SWITCH, 0);
    gpio_set_level(pins::ESP::CONTACTOR, 1);
    gpio_set_level(pins::ESP::GS0, 1);
    gpio_set_level(pins::ESP::GS1, 1);
    gpio_set_level(pins::ESP::CAN_S, 0);
    gpio_set_level(pins::ESP::CAN_ON, 1);
}

} // namespace hardware