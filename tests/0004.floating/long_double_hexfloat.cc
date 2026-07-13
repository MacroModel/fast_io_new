#include <fast_io_freestanding.h>
#include <fast_io_unit/floating.h>

#include <cstring>
#include <limits>

int main()
{
	if constexpr (::std::numeric_limits<long double>::digits == 64 &&
				  ::std::numeric_limits<long double>::max_exponent == 16384)
	{
		long double value{1.0L};
		value += ::std::numeric_limits<long double>::epsilon();
		auto manipulator{::fast_io::mnp::hexfloat(value)};
		char buffer[64];
		auto const end{::fast_io::print_reserve_define(
			::fast_io::io_reserve_type_t<char, decltype(manipulator)>{}, buffer, manipulator)};
		constexpr char expected[]{"1.0000000000000002p+0"};
		auto const size{static_cast<::std::size_t>(end - buffer)};
		return size != sizeof(expected) - 1u || ::std::memcmp(buffer, expected, size) != 0;
	}
}
