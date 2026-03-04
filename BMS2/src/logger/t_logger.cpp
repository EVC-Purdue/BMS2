#include <cstdio>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "esp_littlefs.h"
#include "esp_err.h"
#include "logger/q_logger.hpp"
#include "battery/faults.hpp"
#include "util/overloaded.hpp"
#include "util/err.hpp"
#include "logger/t_logger.hpp"
#include "logger/Web/web.hpp"



namespace t_logger {

TLogger::TLogger(uint32_t period)
    : task_base::TaskBase(period),
    param_delete_log_if_full(false),
    filesystem_usage_ratio(0.0f),
    filesystem_usage_write_count(0),
    write_buffer_index(0),
    write_buffer{0},
    log_line_buffer{0}
    {}


void TLogger::write_buffer_to_filesystem() {
    if (this->write_buffer_index == 0) {
        return; // Nothing to write
    }

    // Check Filesystem usage
    if (this->filesystem_usage_write_count >= FILESYSTEM_RECHECK_USAGE_WRITES_COUNT) {
        size_t total = 0;
        size_t used = 0;
        if(esp_littlefs_info("storage", &total, &used) == ESP_OK && total > 0) {
             this->filesystem_usage_ratio = static_cast<float>(used) / static_cast<float>(total);
        } else {
             // If info fails, we can't determine usage. 
             // Safest to assume 0 to avoid blocking writes if it's just a query error,
             // or handle as full? If not mounted, write will fail anyway.
             this->filesystem_usage_ratio = 0.0f; 
        }
        this->filesystem_usage_write_count = 0;
    } else {
        this->filesystem_usage_write_count++;
    }

    if (this->filesystem_usage_ratio > FILESYSTEM_MAX_USAGE_RATIO) {
        if (this->param_delete_log_if_full) {
                // If full and allowed to delete, delete file and check if successful
                if(std::remove(LOG_FILE_PATH) != 0) {
                    // if delete failed, do not write.
                    // This is safer than aborting if file is locked or other issues
                     this->write_buffer_index = 0; 
                     return;
                }
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
            [this](const q_logger::msg::LogLine& m) {
                // Safety: assume log line fits in buffer
                int written = std::snprintf(
                    this->log_line_buffer,
                    LOG_LINE_MAX_SIZE,
                    "%lld,", m.timestamp
                );
                for (size_t i = 0; i < battery::IC_COUNT * battery::CELL_COUNT_PER_IC; i++) {
                    written += std::snprintf(
                        this->log_line_buffer + written,
                        LOG_LINE_MAX_SIZE - written,
                        "%lu,",
                        m.voltages[i]
                    );
                }
                for (size_t i = 0; i < battery::THERM_COUNT; i++) {
                    written += std::snprintf(
                        this->log_line_buffer + written,
                        LOG_LINE_MAX_SIZE - written,
                        "%.2f,",
                        m.temps.therms[i]
                    );
                }
                written += std::snprintf(
                    this->log_line_buffer + written,
                    LOG_LINE_MAX_SIZE - written,
                    "%.2f,%.2f,%.2f,%.2f,",
                    m.temps.fet,
                    m.temps.bal_bot,
                    m.temps.bal_top,
                    m.current
                );
                for (size_t i = 0; i < faults::WarningFault::WARNING_FAULTS_END; i++) {
                    bool fault_active = (m.faults & (1 << i)) != 0;
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
                    this->write_buffer_to_filesystem();
                }
                // Copy log line to write buffer
                std::memcpy(
                    this->write_buffer + this->write_buffer_index,
                    this->log_line_buffer,
                    written
                );
                this->write_buffer_index += written;
            },
            [this](const q_logger::msg::ReadStart& _m) {
                // Handle read start
            },
            [this](const q_logger::msg::ReadEnd& _m) {
                // Handle read end
            },
            [this](const q_logger::msg::Flush& _m) {
                this->write_buffer_to_filesystem();
            },
            [this](const q_logger::msg::SetDeleteLog& m) {
                this->param_delete_log_if_full = m.delete_log;
            }
        }, rx_msg);
    }

    // TODO: check for web requests
}


} // namespace t_logger
