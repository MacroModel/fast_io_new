#include <bit>
#include <charconv>
#include <cstdint>
#include <string_view>
#include <system_error>

#include <fast_io_freestanding.h>

namespace
{

using rounding = ::fast_io::manipulators::floating_rounding;

[[nodiscard]] inline constexpr bool same_fields(
	double left, double right) noexcept
{
	auto const lhs{::fast_io::details::get_punned_result(left)};
	auto const rhs{::fast_io::details::get_punned_result(right)};
	return lhs.mantissa == rhs.mantissa &&
		   lhs.exponent == rhs.exponent &&
		   lhs.sign == rhs.sign;
}

consteval bool constant_carrier_is_correct() noexcept
{
	constexpr auto source{
		::std::bit_cast<double>(UINT64_C(0xc3d5819119d3b7a8))};
	constexpr auto fields{
		::fast_io::details::get_punned_result(source)};
	constexpr auto result{
		::fast_io::details::dragonbox_impl<
			double, rounding::nearest_to_odd>(
			fields.mantissa,
			static_cast<::std::int_least32_t>(fields.exponent),
			fields.sign)};
	return result.m10 == UINT64_C(6198717147617599) &&
		   result.e10 == 3;
}

static_assert(constant_carrier_is_correct());

/*
The rejected carrier 61987171476176*10^5 is exactly the open midpoint above
this negative binary64 under nearest-to-odd.  The exact endpoint divisibility
test must continue to the finer grid and emit 6198717147617599*10^3.  Checking
the constexpr integer kernel, the public formatter spelling, and an
independent public parse pins the same theorem at all three interfaces,
including MSVC targets without a native uint128 type.
*/
[[nodiscard]] bool check_public_roundtrip() noexcept
{
	constexpr auto source{
		::std::bit_cast<double>(UINT64_C(0xc3d5819119d3b7a8))};
	char buffer[64u]{};
	auto const formatted{
		::fast_io::to_chars<rounding::nearest_to_odd>(
			buffer, buffer + sizeof(buffer), source,
			::std::chars_format::scientific)};
	constexpr ::std::string_view expected{
		"-6.198717147617599e+18"};
	if (formatted.ec != ::std::errc{} ||
		::std::string_view(
			buffer, static_cast<::std::size_t>(formatted.ptr - buffer)) !=
			expected)
	{
		return false;
	}

	double parsed{};
	auto const converted{
		::fast_io::from_chars<rounding::nearest_to_odd>(
			buffer, formatted.ptr, parsed,
			::std::chars_format::scientific)};
	return converted.ec == ::std::errc{} &&
		   converted.ptr == formatted.ptr &&
		   same_fields(parsed, source);
}

} // namespace

int main()
{
	return !check_public_roundtrip();
}
