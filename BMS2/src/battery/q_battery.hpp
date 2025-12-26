#ifndef Q_BATTERY_HPP
#define Q_BATTERY_HPP

#include <variant>

#include "freertos/FreeRTOS.h"



namespace q_battery {

extern QueueHandle_t g_battery_queue;

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


} // namespace q_battery

#endif // Q_BATTERY_HPP