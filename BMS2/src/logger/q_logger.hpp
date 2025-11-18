#ifndef Q_LOGGER_HPP
#define Q_LOGGER_HPP

#include <variant>

#include "freertos/FreeRTOS.h"



namespace q_logger {

extern QueueHandle_t g_logger_queue;

constexpr UBaseType_t QUEUE_SIZE = 10;

namespace msg {
	struct Write {
		const char* message;
	};

	struct Read {
		char* buffer;
		size_t length;
	};

	struct Flush {
		// No additional data needed for flush
	};
} // namespace msg

using Message = std::variant<msg::Write, msg::Read, msg::Flush>;


} // namespace q_logger

#endif // Q_LOGGER_HPP