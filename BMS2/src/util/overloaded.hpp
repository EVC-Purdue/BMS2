#ifndef UTIL_OVERLOADED_HPP
#define UTIL_OVERLOADED_HPP

namespace util
{

	template <class... Ts>
	struct OverloadedVisit : Ts...
	{
		using Ts::operator()...;
	};
	template <class... Ts>
	OverloadedVisit(Ts...) -> OverloadedVisit<Ts...>;

} // namespace util

#endif // UTIL_OVERLOADED_HPP