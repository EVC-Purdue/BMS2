#include <cstdio>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "esp_spiffs.h"
#include "esp_err.h"

#include "logger/q_logger.hpp"
#include "battery/faults.hpp"
#include "util/overloaded.hpp"
#include "util/err.hpp"


#include "logger/t_logger.hpp"



namespace t_logger {

TLogger::TLogger(uint32_t period)
    : task_base::TaskBase(period),
    param_delete_log_if_full(false),
    spiffs_usage_ratio(0.0f),
    spiffs_usage_write_count(0),
    write_buffer_index(0),
    write_buffer{0},
    log_line_buffer{0}
    {}


void TLogger::write_buffer_to_spiffs() {
    if (this->write_buffer_index == 0) {
        return; // Nothing to write
    }

    // Check SPIFFS usage
    if (this->spiffs_usage_write_count >= SPIFFS_RECHECK_USAGE_WRITES_COUNT) {
        size_t total = 0;
        size_t used = 0;
        ESP_ERROR_CHECK(esp_spiffs_info(nullptr, &total, &used));
        this->spiffs_usage_ratio = static_cast<float>(used) / static_cast<float>(total);
        this->spiffs_usage_write_count = 0;
    } else {
        this->spiffs_usage_write_count++;
    }

    if (this->spiffs_usage_ratio > SPIFFS_MAX_USAGE_RATIO) {
        if (this->param_delete_log_if_full) {
            UTIL_CHECK_REQUIRE(std::remove(LOG_FILE_PATH) == 0);
        } else {
            // Do not write if storage is full and deletion is not allowed
            this->write_buffer_index = 0; // Discard/reset buffer
            return;
        }
    }

    // Open log file in append mode
    FILE* file = std::fopen(LOG_FILE_PATH, "a");
    UTIL_CHECK_REQUIRE(file != nullptr);

    // Write buffer to file
    int written = std::fprintf(file, "%.*s", static_cast<int>(this->write_buffer_index), this->write_buffer);
    bool write_success = (written == static_cast<int>(this->write_buffer_index));

    // Close file before checking success
    std::fclose(file);
    UTIL_CHECK_REQUIRE(write_success);

    this->write_buffer_index = 0; // Reset buffer index after writing
}

void TLogger::task() {

    // TODO: should there be a flush_requested system

    // Read and process all messages from the logger queue
    q_logger::Message rx_msg = {};
    while (xQueueReceive(q_logger::g_logger_queue, &rx_msg, 0) == pdTRUE) {
        std::visit(util::OverloadedVisit {
            [this](const q_logger::msg::LogLine& log_line) {
                // Safety: assume log line fits in buffer
                int written = std::snprintf(
                    this->log_line_buffer,
                    LOG_LINE_MAX_SIZE,
                    "%lld,", log_line.timestamp
                );
                for (size_t i = 0; i < battery::IC_COUNT * battery::CELL_COUNT_PER_IC; i++) {
                    written += std::snprintf(
                        this->log_line_buffer + written,
                        LOG_LINE_MAX_SIZE - written,
                        "%lu,",
                        log_line.voltages[i]
                    );
                }
                for (size_t i = 0; i < battery::THERM_COUNT; i++) {
                    written += std::snprintf(
                        this->log_line_buffer + written,
                        LOG_LINE_MAX_SIZE - written,
                        "%.2f,",
                        log_line.temps.therms[i]
                    );
                }
                written += std::snprintf(
                    this->log_line_buffer + written,
                    LOG_LINE_MAX_SIZE - written,
                    "%.2f,%.2f,%.2f,%.2f,",
                    log_line.temps.fet,
                    log_line.temps.bal_bot,
                    log_line.temps.bal_top,
                    log_line.current
                );
                for (size_t i = 0; i < faults::WarningFault::WARNING_FAULTS_END; i++) {
                    bool fault_active = (log_line.faults & (1 << i)) != 0;
                    written += std::snprintf(
                        this->log_line_buffer + written,
                        LOG_LINE_MAX_SIZE - written,
                        "%d",
                        fault_active ? 1 : 0
                    );
                }
                written += std::snprintf(
                    this->log_line_buffer + written,
                    LOG_LINE_MAX_SIZE - written,
                    "\n"
                );

                // If the log line doesn't fit in the remaining buffer, flush first
                if (this->write_buffer_index + written > WRITE_BUFFER_SIZE) {
                    this->write_buffer_to_spiffs();
                }
                // Copy log line to write buffer
                std::memcpy(
                    this->write_buffer + this->write_buffer_index,
                    this->log_line_buffer,
                    written
                );
                this->write_buffer_index += written;
            },
            [this](const q_logger::msg::ReadStart& _read_start) {
                // Handle read start
            },
            [this](const q_logger::msg::ReadEnd& _read_end) {
                // Handle read end
            },
            [this](const q_logger::msg::Flush& _flush) {
                this->write_buffer_to_spiffs();
            },
            [this](const q_logger::msg::SetDeleteLog& sdl) {
                this->param_delete_log_if_full = sdl.delete_log;
            }
        }, rx_msg);
    }
    
}


} // namespace t_logger
