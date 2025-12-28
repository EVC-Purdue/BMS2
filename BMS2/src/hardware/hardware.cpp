
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"

#include "hardware/pins.hpp"

#include "hardware/hardware.hpp"

namespace hardware {

void configure_gpio_output() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pins::ESP::CONTACTOR) |
                        (1ULL << pins::ESP::SS_SWITCH) |
                        (1ULL << pins::ESP::PWR_EN) |
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

void configure_ledc() {
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_SPEED_MODE_MAX,
        .duty_resolution  = LEDC_TIMER_10_BIT, // 0–1023
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 1000,               // Initial dummy frequency
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t channel = {};
    channel.gpio_num = pins::ESP::BUZZER;
    channel.speed_mode = LEDC_SPEED_MODE_MAX;
    channel.channel = LEDC_CHANNEL_0;
    channel.intr_type = LEDC_INTR_DISABLE;
    channel.timer_sel = LEDC_TIMER_0;
    channel.duty = 0;
    channel.hpoint = 0;
    ledc_channel_config(&channel);
}

void configure(spi_device_handle_t* spi_handle) {
    configure_gpio_output();
    configure_spi(spi_handle);
    configure_ledc();
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

void play_buzzer_tone(uint32_t frequency_hz, uint32_t duration_ms) {
    ledc_set_freq(LEDC_SPEED_MODE_MAX, LEDC_TIMER_0, frequency_hz);

    ledc_set_duty(LEDC_SPEED_MODE_MAX, LEDC_CHANNEL_0, LEDC_DUTY);
    ledc_update_duty(LEDC_SPEED_MODE_MAX, LEDC_CHANNEL_0);

    vTaskDelay(pdMS_TO_TICKS(duration_ms));

    ledc_set_duty(LEDC_SPEED_MODE_MAX, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_SPEED_MODE_MAX, LEDC_CHANNEL_0);
}

} // namespace hardware