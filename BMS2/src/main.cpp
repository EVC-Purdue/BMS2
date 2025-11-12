#include "logger/t_logger.hpp"

extern "C" void app_main() {
	int a = 5;
	int b = 10;
	int result = logger::sum(a, b);
}