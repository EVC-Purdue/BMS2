#ifndef UTIL_CMP_HPP
#define UTIL_CMP_HPP

namespace util {
	// Return true if value is within [min, max], else return false
	bool check_within(float value, float min, float max);

	// Return true if the absolute difference between value1 and value2 is within the
	// threshold (inclusive), else return false
	bool check_difference(float value1, float value2, float threshold);
} // namespace util


#endif // UTIL_CMP_HPP