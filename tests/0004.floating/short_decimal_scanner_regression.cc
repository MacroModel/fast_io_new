#include <cstddef>
#include <initializer_list>
#include <system_error>

#include <fast_io_freestanding.h>

namespace
{

[[nodiscard]] inline constexpr bool same_fields(
	double left, double right) noexcept
{
	auto const lhs{::fast_io::details::get_punned_result(left)};
	auto const rhs{::fast_io::details::get_punned_result(right)};
	return lhs.mantissa == rhs.mantissa &&
		   lhs.exponent == rhs.exponent &&
		   lhs.sign == rhs.sign;
}

template <::std::size_t extent>
[[nodiscard]] bool check_token(
	char const (&text)[extent], bool expected_short,
	double expected_value) noexcept
{
	constexpr auto flags{
		::fast_io::details::from_chars_floating_flags<
			::std::chars_format::general,
			::fast_io::manipulators::floating_rounding::
				nearest_to_even>()};
	auto const first{text};
	auto const last{text + extent - 1u};
	double short_value{};
	auto const short_result{
		::fast_io::details::scan_decfloat_contiguous_short_define_impl<
			char, flags>(first, last, false, short_value)};
	if (short_result.handled != expected_short)
	{
		return false;
	}

	double fast_value{};
	auto const fast_result{
		::fast_io::from_chars(first, last, fast_value)};
	if (fast_result.ec != ::std::errc{} ||
		fast_result.ptr != last - 1u ||
		!same_fields(fast_value, expected_value))
	{
		return false;
	}
	if (expected_short &&
		(short_result.code != ::fast_io::parse_code::ok ||
		 short_result.iter != fast_result.ptr ||
		 !same_fields(short_value, fast_value)))
	{
		return false;
	}
	return true;
}

/*
The scalar peel is allowed to change control flow, but not the decimal
recurrence or its exact-fallback boundary.  These cases straddle the 19-digit
uint64 coefficient limit in both the integer and fractional loops.  `!`
proves that a nondigit immediately after the last storable digit is consumed
by neither path; the 20th digit proves that another digit, including zero,
selects the exact scanner rather than silently truncating the coefficient.
The exponent case additionally proves that successful short scanning resumes
the common grammar at the correct cursor.
*/
[[nodiscard]] bool check_scalar_peel_boundaries() noexcept
{
	return check_token(
			   "123456789012345678!", true, 123456789012345678.0) &&
		   check_token(
			   "1234567890123456789!", true, 1234567890123456789.0) &&
		   check_token(
			   "12345678901234567890!", false, 12345678901234567890.0) &&
		   check_token(
			   "1.23456789012345678!", true, 1.23456789012345678) &&
		   check_token(
			   "1.234567890123456789!", true, 1.234567890123456789) &&
		   check_token(
			   "1.2345678901234567890!", false, 1.2345678901234567890) &&
		   check_token(
			   "1234567890123456789e-18!", true,
			   1.234567890123456789);
}

[[nodiscard]] bool check_transactional_prefix_delegation() noexcept
{
	constexpr auto flags{
		::fast_io::details::from_chars_floating_flags<
			::std::chars_format::general,
			::fast_io::manipulators::floating_rounding::
				nearest_to_even>()};
	constexpr char infinity[]{"-inf"};
	double speculative{42.0};
	auto const short_infinity{
		::fast_io::details::scan_decfloat_contiguous_short_define_impl<
			char, flags>(
			infinity + 1u, infinity + 4u, true,
			speculative)};
	if (short_infinity.handled || speculative != 42.0)
	{
		return false;
	}
	double parsed_infinity{};
	auto const public_infinity{
		::fast_io::from_chars(
			infinity, infinity + 4u, parsed_infinity)};
	auto const infinity_fields{
		::fast_io::details::get_punned_result(parsed_infinity)};
	if (public_infinity.ec != ::std::errc{} ||
		public_infinity.ptr != infinity + 4u ||
		infinity_fields.mantissa != 0u ||
		infinity_fields.exponent != 0x7ffu ||
		!infinity_fields.sign)
	{
		return false;
	}

	/*
	An isolated radix point and an unrelated code unit contain no decimal
	digit.  The speculative short scanner must expose neither a cursor nor a
	value update; the complete scanner then supplies the canonical public
	invalid_argument result from the original begin pointer.  Together with
	the Inf case above, this pins every no-digit delegation class introduced
	by removal of the duplicate leading-digit precheck.
	*/
	for (char const token : {'.', 'x'})
	{
		char const text[]{token};
		speculative = 42.0;
		auto const short_invalid{
			::fast_io::details::
				scan_decfloat_contiguous_short_define_impl<
					char, flags>(
					text, text + 1u, false,
					speculative)};
		if (short_invalid.handled || speculative != 42.0)
		{
			return false;
		}
		auto parsed_invalid{42.0};
		auto const public_invalid{
			::fast_io::from_chars(
				text, text + 1u, parsed_invalid)};
		if (public_invalid.ec != ::std::errc::invalid_argument ||
			public_invalid.ptr != text ||
			parsed_invalid != 42.0)
		{
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool check_extended_ieee_exponent_metadata() noexcept
{
#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
	/*
	Clang can provide a complete native __float128 field while the selected
	libstdc++ does not specialize numeric_limits<__float128>.  The scanner must
	therefore derive its exponent bounds from iec559_traits, just as it derives
	the mantissa and encoded exponent fields.  An unspecialized max_exponent is
	zero and used to make this ordinary exponent-zero value report overflow.
	The exact fields below pin the frontend/library-independent result.
	*/
	constexpr char text[]{"1.25"};
	__float128 parsed{};
	auto const result{
		::fast_io::from_chars(text, text + 4u, parsed)};
	auto const fields{
		::fast_io::details::get_punned_result(parsed)};
	auto const expected_fields{
		::fast_io::details::get_punned_result(
			static_cast<__float128>(1.25))};
	return result.ec == ::std::errc{} &&
		   result.ptr == text + 4u &&
		   fields.mantissa == expected_fields.mantissa &&
		   fields.exponent == expected_fields.exponent &&
		   fields.sign == expected_fields.sign;
#else
	return true;
#endif
}

} // namespace

int main()
{
	return !(check_scalar_peel_boundaries() &&
			 check_transactional_prefix_delegation() &&
			 check_extended_ieee_exponent_metadata());
}
