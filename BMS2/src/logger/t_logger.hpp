#ifndef T_LOGGER_HPP
#define T_LOGGER_HPP

#include <cstdint>

#include "freertos/FreeRTOS.h"

#include "task/task_base.hpp"


namespace t_logger {

constexpr uint32_t TASK_PERIOD_MS = 1000;
constexpr uint32_t TASK_STACK_SIZE = 4096;
constexpr UBaseType_t TASK_PRIORITY = 2;
constexpr BaseType_t TASK_CORE_ID = 0;
constexpr const char* TASK_NAME = "LoggingTask";

constexpr const char* LOG_FILE_PATH = "/spiffs/log.csv";

constexpr size_t WRITE_BUFFER_SIZE = 512; // Size of buffer for writing log lines
constexpr size_t LOG_LINE_MAX_SIZE = 120; // Maximum size of a single log line



class TLogger : public task_base::TaskBase {
	private:
        size_t write_buffer_index;
        char write_buffer[WRITE_BUFFER_SIZE];
        char log_line_buffer[LOG_LINE_MAX_SIZE];

        void write_buffer_to_spiffs();

	public:
		TLogger(uint32_t period);

		void task() override;
};



} // namespace t_logger





#endif // T_LOGGER_HPP