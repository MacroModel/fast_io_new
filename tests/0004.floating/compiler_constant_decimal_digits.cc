#include <cstddef>
#include <cstdint>
#include <limits>

#include <fast_io_freestanding.h>

namespace
{

template <typename unsigned_type>
[[nodiscard]] consteval bool check_decimal_digit_boundaries() noexcept
{
	constexpr auto maximum{
		(::std::numeric_limits<unsigned_type>::max)()};
	if (::fast_io::details::compiler_constant_floating_decimal_digits(
			unsigned_type{}) != 1u)
	{
		return false;
	}

	unsigned_type power{10u};
	::std::size_t digits_below_power{1u};
	for (;;)
	{
		if (::fast_io::details::compiler_constant_floating_decimal_digits(
				static_cast<unsigned_type>(power - 1u)) != digits_below_power ||
			::fast_io::details::compiler_constant_floating_decimal_digits(
				power) != digits_below_power + 1u)
		{
			return false;
		}
		++digits_below_power;
		if (maximum / 10u < power)
		{
			break;
		}
		power = static_cast<unsigned_type>(power * 10u);
	}
	return ::fast_io::details::compiler_constant_floating_decimal_digits(
		maximum) == digits_below_power;
}

static_assert(check_decimal_digit_boundaries<::std::uint_least8_t>());
static_assert(check_decimal_digit_boundaries<::std::uint_least16_t>());
static_assert(check_decimal_digit_boundaries<::std::uint_least32_t>());
static_assert(check_decimal_digit_boundaries<::std::uint_least64_t>());

#if defined(__SIZEOF_INT128__)
static_assert(check_decimal_digit_boundaries<__uint128_t>());
#endif

} // namespace

int main() {}
