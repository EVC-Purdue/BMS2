
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include <cstdio>
#include <cstring>
#include "esp_spiffs.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "hardware/pins.hpp"
#include "hardware/spi.hpp"
#include "hardware/hardware.hpp"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "battery/parameters.hpp"
#include "hardware/CAN.hpp"

namespace hardware
{

    static bool s_wifi_initialized = false;
    static bool s_wifi_ap_started = false;

    void configure_gpio_output()
    {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << pins::ESP::CONTACTOR) |
                            (1ULL << pins::ESP::SS_SWITCH) |
                            (1ULL << pins::ESP::PWR_EN) |
                            (1ULL << pins::ESP::GS0) |
                            (1ULL << pins::ESP::GS1) |
                            (1ULL << pins::ESP::LED) |
                            (1ULL << pins::ESP::CAN_S) |
                            (1ULL << pins::ESP::SPI_CS),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE};
        ESP_ERROR_CHECK(gpio_config(&io_conf));
    }

    void configure_spi(spi_device_handle_t *spi_handle)
    {
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
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

        spi_device_interface_config_t devcfg = {};
        devcfg.mode = SPI_MODE;
        devcfg.clock_speed_hz = SPI_CLOCK_SPEED_HZ;
        devcfg.spics_io_num = -1;
        devcfg.queue_size = 1;
        ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, spi_handle));
        spi::saveHandle(*spi_handle);
    }

    // void configure_ledc() {
    //     ledc_timer_config_t timer = {
    //         .speed_mode = LEDC_SPEED_MODE_MAX,
    //         .duty_resolution  = LEDC_TIMER_10_BIT, // 0–1023
    //         .timer_num = LEDC_TIMER_0,
    //         .freq_hz = 1000,               // Initial dummy frequency
    //         .clk_cfg = LEDC_AUTO_CLK,
    //         .deconfigure = false
    //     };
    //     ESP_ERROR_CHECK(ledc_timer_config(&timer));

    //     ledc_channel_config_t channel = {};
    //     channel.gpio_num = pins::ESP::BUZZER;
    //     channel.speed_mode = LEDC_SPEED_MODE_MAX;
    //     channel.channel = LEDC_CHANNEL_0;
    //     channel.intr_type = LEDC_INTR_DISABLE;
    //     channel.timer_sel = LEDC_TIMER_0;
    //     channel.duty = 0;
    //     channel.hpoint = 0;
    //     ESP_ERROR_CHECK(ledc_channel_config(&channel));
    // }

    // void configure_spiffs() {
    //     esp_vfs_spiffs_conf_t conf = {
    //         .base_path = SPIFFS_BASE_PATH,
    //         .partition_label = nullptr,
    //         .max_files = SPIFFS_MAX_FILES,
    //         .format_if_mount_failed = SPIFFS_FORMAT_IF_MOUNT_FAILED
    //     };
    //     ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));
    // }

    void configure(spi_device_handle_t *spi_handle)
    {
        configure_gpio_output();
        configure_spi(spi_handle);
        // configure_ledc();
        // configure_spiffs();
        can::init();

        // Configure and mount LittleFS (file system)
        esp_vfs_littlefs_conf_t conf = {
            .base_path = FILESYSTEM_BASE_PATH,
            .partition_label = "storage",
            .partition = nullptr, // unused
            .format_if_mount_failed = true,
            .read_only = false,
            .dont_mount = false,
            .grow_on_mount = false,
        };
        esp_err_t littlefs_ret = esp_vfs_littlefs_register(&conf);
        if (littlefs_ret != ESP_OK)
        {
            ESP_LOGE("main", "Failed to initialize LittleFS: %s", esp_err_to_name(littlefs_ret));
        }

        ESP_ERROR_CHECK(start_wifi_hotspot());
    }

    esp_err_t start_wifi_hotspot()
    {
        if (s_wifi_ap_started)
        {
            return ESP_OK;
        }

        if (!s_wifi_initialized)
        {
            esp_err_t ret = nvs_flash_init();
            if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
            {
                ESP_ERROR_CHECK(nvs_flash_erase());
                ret = nvs_flash_init();
            }
            if (ret != ESP_OK)
            {
                return ret;
            }

            ESP_RETURN_ON_ERROR(esp_netif_init(), "hardware", "esp_netif_init failed");

            esp_err_t loop_ret = esp_event_loop_create_default();
            if (loop_ret != ESP_OK && loop_ret != ESP_ERR_INVALID_STATE)
            {
                return loop_ret;
            }

            ESP_RETURN_ON_ERROR(esp_netif_create_default_wifi_ap() ? ESP_OK : ESP_FAIL, "hardware", "wifi ap netif creation failed");

            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), "hardware", "wifi init failed");

            s_wifi_initialized = true;
        }

        wifi_config_t wifi_config = {};
        std::snprintf(reinterpret_cast<char *>(wifi_config.ap.ssid), sizeof(wifi_config.ap.ssid), "%s", params::PARAMETER_WIFI_AP_SSID);
        std::snprintf(reinterpret_cast<char *>(wifi_config.ap.password), sizeof(wifi_config.ap.password), "%s", params::PARAMETER_WIFI_AP_PASSWORD);
        wifi_config.ap.ssid_len = std::strlen(params::PARAMETER_WIFI_AP_SSID);
        wifi_config.ap.channel = WIFI_AP_CHANNEL;
        wifi_config.ap.max_connection = WIFI_AP_MAX_CONNECTIONS;
        wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

        if (std::strlen(params::PARAMETER_WIFI_AP_PASSWORD) == 0)
        {
            wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        }

        ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), "hardware", "wifi set mode failed");
        ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &wifi_config), "hardware", "wifi set config failed");
        ESP_RETURN_ON_ERROR(esp_wifi_start(), "hardware", "wifi start failed");

        s_wifi_ap_started = true;
        return ESP_OK;
    }

    void setup_initial_gpio_states()
    {
        gpio_set_level(pins::ESP::PWR_EN, 1);
        gpio_set_level(pins::ESP::LED, 0); // on is low
        gpio_set_level(pins::ESP::SS_SWITCH, 0);
        gpio_set_level(pins::ESP::CONTACTOR, 1);
        gpio_set_level(pins::ESP::GS0, 1);
        gpio_set_level(pins::ESP::GS1, 1);
        gpio_set_level(pins::ESP::CAN_S, 0);
        // gpio_set_level(pins::ESP::CAN_ON, 1);
    }

    // void play_buzzer_tone(uint32_t frequency_hz, uint32_t duration_ms) {
    //     ledc_set_freq(LEDC_SPEED_MODE_MAX, LEDC_TIMER_0, frequency_hz);

    //     ledc_set_duty(LEDC_SPEED_MODE_MAX, LEDC_CHANNEL_0, LEDC_DUTY);
    //     ledc_update_duty(LEDC_SPEED_MODE_MAX, LEDC_CHANNEL_0);

    //     vTaskDelay(pdMS_TO_TICKS(duration_ms));

    //     ledc_set_duty(LEDC_SPEED_MODE_MAX, LEDC_CHANNEL_0, 0);
    //     ledc_update_duty(LEDC_SPEED_MODE_MAX, LEDC_CHANNEL_0);
    // }

} // namespace hardware