#ifndef T_LOGGER_HPP
#define T_LOGGER_HPP

#include <cstdint>

#include "freertos/FreeRTOS.h"

#include "task/task_base.hpp"


namespace t_logger {

// TODO: if TLogger is 100% reactionary/event based on queue messages, it may be better to
// pend forever on a queue receive rather than have a periodic task
constexpr uint32_t TASK_PERIOD_MS = 1000;
constexpr uint32_t TASK_STACK_SIZE = 4096;
constexpr UBaseType_t TASK_PRIORITY = 2;
constexpr BaseType_t TASK_CORE_ID = 0;
constexpr const char* TASK_NAME = "LoggingTask";

constexpr float FILESYSTEM_MAX_USAGE_RATIO = 0.8f; 
constexpr const char* LOG_FILE_PATH = "/littlefs/log.csv";

constexpr size_t FILESYSTEM_RECHECK_USAGE_WRITES_COUNT = 20; // Number of writes after which to recheck filesystem usage

constexpr size_t WRITE_BUFFER_SIZE = 512; // Size of buffer for writing log lines
constexpr size_t LOG_LINE_MAX_SIZE = 120; // Maximum size of a single log line. This MUST be big enough to hold one full log line.



class TLogger : public task_base::TaskBase {
    private:
        // If the filesystem usage is above the max ratio, whether to delete the log file to make space
        // If false, new log data will be discarded when storage is full
        bool param_delete_log_if_full;

        // Cached filesystem usage ratio to avoid frequent checks
        float filesystem_usage_ratio;
        // Number of writes since last filesystem usage check
        size_t filesystem_usage_write_count;
        
        // Current index in the write buffer
        size_t write_buffer_index;
        // Buffer for accumulating log lines before writing to filesystem
        // Allows batching multiple log lines into one write operation
        char write_buffer[WRITE_BUFFER_SIZE];
        // Buffer for formatting a single log line. Must be large enough to hold one full log line
        // as this is not checked.
        char log_line_buffer[LOG_LINE_MAX_SIZE];

        // Flushes the current write buffer to the Filesystem log file and resets the buffer index.
        void write_buffer_to_filesystem();

    public:
        TLogger(uint32_t period);

        void task() override;
};



} // namespace t_logger





#endif // T_LOGGER_HPP