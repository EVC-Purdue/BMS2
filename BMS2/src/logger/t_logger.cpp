#include <cstdio>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "esp_spiffs.h"

#include "logger/q_logger.hpp"
#include "util/overloaded.hpp"


#include "logger/t_logger.hpp"



namespace t_logger {

TLogger::TLogger(uint32_t period)
	: task_base::TaskBase(period),
    write_buffer_index(0) {}


void TLogger::write_buffer_to_spiffs() {
    if (this->write_buffer_index == 0) {
        return; // Nothing to write
    }

    // TODO: handle full check and delete parameter

    // Open log file in append mode
    FILE* file = std::fopen(LOG_FILE_PATH, "a");
    if (file == nullptr) {
        // TODO: Handle error
        return;
    }

    // Write buffer to file
    int written = std::fprintf(file, "%.*s", static_cast<int>(this->write_buffer_index), this->write_buffer);

    if (written < 0) {
        // TODO: Handle error
    }


    std::fclose(file);
    this->write_buffer_index = 0; // Reset buffer index after writing
}

void TLogger::task() {

    // TODO: should there be a flush_requested system

    // Read and process all messages from the logger queue
	q_logger::Message msg = {};
    while (xQueueReceive(q_logger::g_logger_queue, &msg, 0) == pdTRUE) {
        std::visit(util::OverloadedVisit {
            [this](const q_logger::msg::LogLine& log_line) {
                // Safety: assume log line fits in buffer
                // TODO: make sure no null bytes are written
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
                    "%.2f,%.2f,%.2f,%.2f,%lu\n",
                    log_line.temps.fet,
                    log_line.temps.bal_bot,
                    log_line.temps.bal_top,
                    log_line.current,
                    log_line.faults
                );

                // If the log line doesn't fit in the remaining buffer, flush first
                if (this->write_buffer_index + written >= WRITE_BUFFER_SIZE) {
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
            [this](const q_logger::msg::ReadStart& read_start) {
                // Handle read start
            },
            [this](const q_logger::msg::ReadEnd& read_end) {
                // Handle read end
            },
            [this](const q_logger::msg::Flush& flush) {
                this->write_buffer_to_spiffs();
            }
        }, msg);
    }
	
}


} // namespace t_logger
