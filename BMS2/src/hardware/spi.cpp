#include "hardware/spi.hpp"

#include <cstring>
#include <vector>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hardware/gpio.hpp"

namespace spi
{
    namespace
    {
        spi_device_handle_t gs_spi_handle = nullptr;
    }

    void saveHandle(spi_device_handle_t handle)
    {
        gs_spi_handle = handle;
    }

    void cs_low(uint8_t pin)
    {
        OUTPUT_LOW(pin);
    }

    void cs_high(uint8_t pin)
    {
        OUTPUT_HIGH(pin);
    }

    void delay_u(uint16_t micro)
    {
        // TODO: BLOCKING DELAY
        esp_rom_delay_us(micro);
    }

    void delay_m(uint16_t milli)
    {
        // TODO: BLOCKING DELAY
        vTaskDelay(pdMS_TO_TICKS(milli));
    }

    /*
    Writes an array of bytes out of the SPI port
    */
    void spi_write_array(uint8_t len,   // Option: Number of bytes to be written on the SPI port
                         uint8_t data[] // Array of bytes to be written on the SPI port
    )
    {
        if (gs_spi_handle == nullptr || len == 0 || data == nullptr)
        {
            return;
        }

        spi_transaction_t trans = {};
        trans.length = static_cast<size_t>(len) * 8U;
        trans.tx_buffer = data;
        ESP_ERROR_CHECK(spi_device_transmit(gs_spi_handle, &trans));
    }

    /*
    Writes and read a set number of bytes using the SPI port.
    */
    void spi_write_read(uint8_t tx_Data[], // array of data to be written on SPI port
                        uint8_t tx_len,    // length of the tx data arry
                        uint8_t *rx_data,  // Input: array that will store the data read by the SPI port
                        uint8_t rx_len     // Option: number of bytes to be read from the SPI port
    )
    {
        if (gs_spi_handle == nullptr)
        {
            return;
        }

        const size_t total_len = static_cast<size_t>(tx_len) + static_cast<size_t>(rx_len);
        if (total_len == 0)
        {
            return;
        }

        std::vector<uint8_t> tx_buf(total_len, 0xFF);
        std::vector<uint8_t> rx_buf(total_len, 0x00);

        if (tx_Data != nullptr && tx_len > 0)
        {
            std::memcpy(tx_buf.data(), tx_Data, tx_len);
        }

        spi_transaction_t trans = {};
        trans.length = total_len * 8U;
        trans.tx_buffer = tx_buf.data();
        trans.rx_buffer = rx_buf.data();
        ESP_ERROR_CHECK(spi_device_transmit(gs_spi_handle, &trans));

        if (rx_data != nullptr && rx_len > 0)
        {
            std::memcpy(rx_data, rx_buf.data() + tx_len, rx_len);
        }
    }

    uint8_t spi_read_byte(uint8_t tx_dat)
    {
        if (gs_spi_handle == nullptr)
        {
            return 0;
        }

        uint8_t rx_data = 0;
        spi_transaction_t trans = {};
        trans.length = 8U;
        trans.tx_buffer = &tx_dat;
        trans.rx_buffer = &rx_data;
        ESP_ERROR_CHECK(spi_device_transmit(gs_spi_handle, &trans));
        return rx_data;
    }
}