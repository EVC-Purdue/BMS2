#include <cstdint>
#include <cmath>

#include "util/cmp.hpp"

namespace util {

bool check_within(float value, float min, float max) {
	return (value >= min && value <= max);
}

bool check_difference(float value1, float value2, float threshold) {
	return (std::abs(value1 - value2) <= threshold);
}

} // namespace util