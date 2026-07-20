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

template <typename unsigned_type>
[[nodiscard]] consteval unsigned_type reference_power_of_ten(
	::std::size_t exponent) noexcept
{
	unsigned_type value{1u};
	while (exponent != 0u)
	{
		value = static_cast<unsigned_type>(value * 10u);
		--exponent;
	}
	return value;
}

template <typename unsigned_type, ::std::size_t maximum_exponent>
[[nodiscard]] consteval bool check_power_of_ten_boundaries() noexcept
{
	// The implementation has direct cases through 10^19 and a generic fallback
	// beginning at 10^20.  Check a continuous interval on both sides so a case
	// typo, the switch/fallback boundary, and unsigned wrap semantics all remain
	// identical to the original counted implementation.
	for (::std::size_t exponent{}; exponent != maximum_exponent + 1u;
		 ++exponent)
	{
		if (::fast_io::details::compiler_constant_floating_power_of_ten<
				unsigned_type>(exponent) !=
			reference_power_of_ten<unsigned_type>(exponent))
		{
			return false;
		}
	}
	return true;
}

static_assert(check_decimal_digit_boundaries<::std::uint_least8_t>());
static_assert(check_decimal_digit_boundaries<::std::uint_least16_t>());
static_assert(check_decimal_digit_boundaries<::std::uint_least32_t>());
static_assert(check_decimal_digit_boundaries<::std::uint_least64_t>());
static_assert(check_power_of_ten_boundaries<::std::uint_least8_t, 24u>());
static_assert(check_power_of_ten_boundaries<::std::uint_least16_t, 24u>());
static_assert(check_power_of_ten_boundaries<::std::uint_least32_t, 24u>());
static_assert(check_power_of_ten_boundaries<::std::uint_least64_t, 24u>());

#if defined(__SIZEOF_INT128__)
static_assert(check_decimal_digit_boundaries<__uint128_t>());
static_assert(check_power_of_ten_boundaries<__uint128_t, 40u>());
#endif

} // namespace

int main() {}
