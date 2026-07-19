#pragma once

// Compatibility adapters for std::chrono and std::tm.  Formatting itself is
// performed by the fast_io-native time grammar/emitter.
#include "fast_io_time.h"

#include <ctime>

namespace fast_io::fmt::details
{

template <typename rep_type, typename period_type>
struct is_chrono_duration<::std::chrono::duration<rep_type, period_type>>
	: ::std::true_type
{};

template <>
struct is_time_format_source<::std::tm> : ::std::true_type
{};

template <typename duration_type>
struct is_time_format_source<
	::std::chrono::time_point<::std::chrono::system_clock, duration_type>>
	: ::std::true_type
{};

[[nodiscard]] inline basic_chrono_calendar_state<false, false>
make_chrono_calendar_state(::std::tm value) noexcept
{
	basic_chrono_calendar_state<false, false> state{};
	// Copy every member independently.  No mktime-like normalization is allowed:
	// a selected conversion observes only the corresponding std::tm field.
	state.value = {static_cast<::std::int_least64_t>(value.tm_year) + 1900,
				   static_cast<unsigned>(value.tm_mon) + 1u,
				   static_cast<unsigned>(value.tm_mday),
				   static_cast<unsigned>(value.tm_wday),
				   static_cast<unsigned>(value.tm_yday),
				   static_cast<unsigned>(value.tm_hour),
				   static_cast<unsigned>(value.tm_min),
				   static_cast<unsigned>(value.tm_sec)};
	return state;
}

template <typename duration_type>
[[nodiscard]] inline ::std::uint_least64_t
chrono_fraction_to_fast_io_subseconds(duration_type fraction) noexcept
{
	using rep_type = typename duration_type::rep;
	using period_type = typename duration_type::period;
	if constexpr (::std::numeric_limits<rep_type>::is_integer)
	{
		auto const count{fraction.count()};
		if constexpr (::std::numeric_limits<rep_type>::is_signed)
		{
			if (count < 0)
			{
				::fast_io::fast_terminate();
			}
		}
		if constexpr (::std::numeric_limits<rep_type>::digits >
					  ::std::numeric_limits<::std::uint_least64_t>::digits)
		{
			if (count > static_cast<rep_type>(
						(::std::numeric_limits<::std::uint_least64_t>::max)()))
			{
				::fast_io::fast_terminate();
			}
		}
		auto const count_u{static_cast<::std::uint_least64_t>(count)};
		constexpr auto numerator{static_cast<::std::uint_least64_t>(period_type::num)};
		constexpr auto denominator{static_cast<::std::uint_least64_t>(period_type::den)};
		if (numerator == 0u || count_u > denominator / numerator)
		{
			::fast_io::fast_terminate();
		}
		auto const fractional_numerator{count_u * numerator};
		::std::uint_least64_t product_high{};
		auto const product_low{::fast_io::intrinsics::umul(
			fractional_numerator,
			::fast_io::uint_least64_subseconds_per_second, product_high)};
		constexpr ::std::uint_least64_t zero{};
		auto [quotient, quotient_high, remainder, remainder_high]{
			::fast_io::intrinsics::udivmod(
				product_low, product_high, denominator, zero)};
		if (quotient_high != 0u || remainder_high != 0u)
		{
			::fast_io::fast_terminate();
		}
		auto const half{denominator / 2u};
		if (remainder > half ||
			(remainder == half && (denominator % 2u) == 0u &&
			 (quotient & 1u) != 0u))
		{
			++quotient;
		}
		return quotient;
	}
	else
	{
		long double const fractional_seconds{
			::std::chrono::duration<long double>(fraction).count()};
		long double const scaled{
			fractional_seconds * static_cast<long double>(
								 ::fast_io::uint_least64_subseconds_per_second)};
		return static_cast<::std::uint_least64_t>(scaled + 0.5L);
	}
}

template <typename duration_type>
[[nodiscard]] inline basic_chrono_calendar_state<true, true>
make_chrono_calendar_state(
	::std::chrono::time_point<::std::chrono::system_clock, duration_type> value)
{
	using namespace ::std::chrono;
	auto const epoch{value.time_since_epoch()};
	auto const whole_seconds{floor<seconds>(epoch)};
	auto const fraction{epoch - whole_seconds};
	auto const whole_count{whole_seconds.count()};
	using whole_rep = ::std::remove_cv_t<decltype(whole_count)>;
	if constexpr (::std::numeric_limits<whole_rep>::digits >
				  ::std::numeric_limits<::std::int_least64_t>::digits)
	{
		if (whole_count < static_cast<whole_rep>(
						  (::std::numeric_limits<::std::int_least64_t>::min)()) ||
			whole_count > static_cast<whole_rep>(
						  (::std::numeric_limits<::std::int_least64_t>::max)()))
		{
			::fast_io::fast_terminate();
		}
	}
	auto subseconds{chrono_fraction_to_fast_io_subseconds(fraction)};
	auto seconds_count{static_cast<::std::int_least64_t>(whole_count)};
	if (subseconds == ::fast_io::uint_least64_subseconds_per_second)
	{
		subseconds = 0u;
		if (seconds_count ==
			(::std::numeric_limits<::std::int_least64_t>::max)())
		{
			::fast_io::fast_terminate();
		}
		++seconds_count;
	}
	// The compatibility boundary is deliberately libc-free: standard time is
	// first represented as a native unix_timestamp, then normalized by utc().
	auto const timestamp{
		::fast_io::utc(::fast_io::unix_timestamp{seconds_count, subseconds})};
	return make_chrono_calendar_state_from_iso<true, true>(
		timestamp, chrono_fractional_digits<typename duration_type::period>(), 0);
}

template <::fast_io::fmt::basic_fixed_string format_literal,
		  source_slice specification, bool has_precision,
		  typename rep_type, typename period_type>
[[nodiscard]] inline constexpr auto make_chrono_field(
	::std::chrono::duration<rep_type, period_type> value,
	::std::size_t precision = 0u) noexcept
{
	static_assert(::std::integral<rep_type> || ::std::floating_point<rep_type>,
				  "fast_io chrono: duration representation must be an arithmetic scalar");
	static_assert(!has_precision || ::std::floating_point<rep_type>,
				  "fast_io chrono: precision is permitted only for a floating duration representation");
	return ::fast_io::manipulators::basic_chrono_field_t<
		format_literal, specification, has_precision,
		::std::chrono::duration<rep_type, period_type>>{value, precision};
}

template <::fast_io::fmt::basic_fixed_string format_literal,
		  source_slice specification, bool has_precision>
[[nodiscard]] inline auto make_chrono_field(
	::std::tm value, ::std::size_t precision = 0u) noexcept
{
	static_assert(!has_precision,
				  "fast_io chrono: precision is not permitted for std::tm");
	auto state{make_chrono_calendar_state(value)};
	return ::fast_io::manipulators::basic_chrono_field_t<
		format_literal, specification, false, decltype(state)>{state, precision};
}

template <::fast_io::fmt::basic_fixed_string format_literal,
		  source_slice specification, bool has_precision, typename duration_type>
[[nodiscard]] inline auto make_chrono_field(
	::std::chrono::time_point<::std::chrono::system_clock, duration_type> value,
	::std::size_t precision = 0u)
{
	static_assert(!has_precision,
				  "fast_io chrono: precision is not permitted for a system_clock time point");
	auto state{make_chrono_calendar_state(value)};
	return ::fast_io::manipulators::basic_chrono_field_t<
		format_literal, specification, false, decltype(state)>{state, precision};
}

} // namespace fast_io::fmt::details
