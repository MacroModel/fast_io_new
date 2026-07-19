#pragma once

#include "program.h"
#include "../types.h"
// Chrono lowering consumes only the freestanding print CPOs.  Pulling hosted
// filesystem and legacy-stream adapters into a string-only format operation
// would make that operation depend on facilities it can never call.
#include "../../fast_io_freestanding.h"

#include <chrono>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <limits>
#include <ratio>
#include <type_traits>
#include <utility>

namespace fast_io::fmt::details
{

enum class chrono_padding : unsigned char;

struct chrono_calendar_state
{
	::std::tm value{};
	long double fractional_second{};
	unsigned fractional_precision{};
	bool utc{};
};

struct chrono_iso_week_fields
{
	int year{};
	unsigned week{};
};

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr char_type chrono_basic_latin(char8_t value) noexcept;

template <::fast_io::fmt::format_character char_type, ::std::size_t extent>
inline constexpr char_type *write_chrono_ascii(
	char_type *output, char8_t const (&text)[extent]) noexcept;

[[nodiscard]] inline int chrono_tm_year(::std::tm const &value) noexcept;
[[nodiscard]] inline unsigned chrono_tm_month(::std::tm const &value) noexcept;
[[nodiscard]] inline unsigned chrono_tm_month_day(::std::tm const &value) noexcept;
[[nodiscard]] inline unsigned chrono_tm_weekday(::std::tm const &value) noexcept;
[[nodiscard]] inline unsigned chrono_tm_year_day(::std::tm const &value) noexcept;
[[nodiscard]] inline unsigned chrono_tm_hour(::std::tm const &value) noexcept;
[[nodiscard]] inline unsigned chrono_tm_minute(::std::tm const &value) noexcept;
[[nodiscard]] inline unsigned chrono_tm_second(::std::tm const &value) noexcept;
[[nodiscard]] inline chrono_iso_week_fields chrono_make_iso_week_fields(
	::std::tm const &value) noexcept;
[[nodiscard]] inline constexpr ::std::int64_t chrono_floor_divide(
	::std::int64_t numerator, ::std::int64_t denominator) noexcept;
[[nodiscard]] inline constexpr unsigned chrono_positive_modulo(
	::std::int64_t value, unsigned modulus) noexcept;
[[nodiscard]] inline constexpr int chrono_floor_century(int year) noexcept;
[[nodiscard]] inline constexpr unsigned chrono_short_year(int year) noexcept;

template <::fast_io::fmt::format_character char_type, typename integer_type>
inline constexpr char_type *write_chrono_integer(
	char_type *output, integer_type value) noexcept;

template <::fast_io::fmt::format_character char_type, typename integer_type>
inline constexpr char_type *write_chrono_padded_integer(
	char_type *output, integer_type value, unsigned width,
	chrono_padding padding) noexcept;

template <typename integer_type>
[[nodiscard]] inline constexpr ::std::size_t chrono_padded_integer_size(
	integer_type value, unsigned width, chrono_padding padding) noexcept;

template <typename period_type>
[[nodiscard]] consteval unsigned chrono_fractional_digits() noexcept;

template <typename signed_type>
[[nodiscard]] inline constexpr auto chrono_unsigned_magnitude(signed_type value) noexcept;

template <::fast_io::fmt::format_character char_type>
inline constexpr char_type *write_chrono_separator(
	char_type *output, char8_t separator) noexcept;

/**
 * The semantic domain selected before a chrono specification is compiled.
 *
 * A duration and a civil/UTC calendar value deliberately do not share one
 * permissive program.  Rejecting `%Y` for a duration during constant evaluation
 * is both a better diagnostic and a useful code-generation proof: an emitted
 * duration leaf can never retain a run-time "does this opcode apply?" branch.
 */
enum class chrono_value_domain : unsigned char
{
	duration,
	calendar
};

enum class chrono_parse_error : unsigned char
{
	none,
	invalid_slice,
	dangling_percent,
	invalid_conversion,
	conversion_not_supported_for_value,
	locale_modifier_not_supported,
	brace_in_chrono_literal,
	capacity_exceeded
};

enum class chrono_padding : unsigned char
{
	zero,
	space,
	none
};

/**
 * A normalized subset of the chrono conversion vocabulary.
 *
 * Composite spellings (`%D`, `%F`, `%R`, `%T`, `%c`, `%x`, and `%X`) remain
 * single operations.  Expanding them in the parser would enlarge the NTTP IR
 * and compiler memory footprint, while expanding them with `if constexpr` in
 * the emitter produces the same straight-line stores.  There is no run-time
 * format-string or opcode interpretation in either case.
 */
enum class chrono_opcode : unsigned char
{
	literal,
	percent,
	newline,
	tab,
	abbreviated_weekday,
	full_weekday,
	abbreviated_month,
	full_month,
	date_time,
	century,
	day_of_month,
	us_date,
	space_day_of_month,
	iso_date,
	iso_week_based_short_year,
	iso_week_based_year,
	hour_24,
	hour_12,
	day_of_year,
	month,
	minute,
	am_pm,
	duration_unit,
	duration_value,
	time_12,
	time_hm,
	second,
	time_hms,
	iso_weekday,
	sunday_week,
	iso_week,
	sunday_weekday,
	monday_week,
	locale_date,
	locale_time,
	short_year,
	year,
	utc_offset,
	time_zone_name,
	default_date_time
};

struct chrono_operation
{
	chrono_opcode opcode{chrono_opcode::literal};
	chrono_padding padding{chrono_padding::zero};
	source_slice literal{};
};

template <::fast_io::fmt::format_character char_type, ::std::size_t capacity>
struct basic_chrono_program
{
	using value_type = char_type;
	static inline constexpr ::std::size_t maximum_capacity{capacity};

	fixed_capacity_array<chrono_operation, capacity> operations{};
	::std::size_t operation_count{};

	inline constexpr bool append_literal(::std::size_t offset) noexcept
	{
		if (operation_count != 0u)
		{
			auto &last{operations[operation_count - 1u]};
			if (last.opcode == chrono_opcode::literal &&
				last.literal.offset + last.literal.size == offset)
			{
				++last.literal.size;
				return true;
			}
		}
		if (operation_count == capacity)
		{
			return false;
		}
		operations[operation_count++] = {
			chrono_opcode::literal, chrono_padding::zero, {offset, 1u}};
		return true;
	}

	inline constexpr bool append_conversion(
		chrono_opcode opcode, chrono_padding padding = chrono_padding::zero) noexcept
	{
		if (operation_count == capacity)
		{
			return false;
		}
		operations[operation_count++] = {opcode, padding, {}};
		return true;
	}
};

template <typename program_type>
struct chrono_parse_result
{
	program_type program{};
	chrono_parse_error error{chrono_parse_error::none};
	::std::size_t error_position{};

	[[nodiscard]] inline constexpr explicit operator bool() const noexcept
	{
		return error == chrono_parse_error::none;
	}
};

template <typename result_type>
inline constexpr bool set_chrono_error(
	result_type &result, chrono_parse_error error, ::std::size_t position) noexcept
{
	result.error = error;
	result.error_position = position;
	return false;
}

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr bool is_chrono_padding_modifier(char_type value) noexcept
{
	return is_syntax_character<u8'_'>(value) || is_syntax_character<u8'-'>(value) ||
		   is_syntax_character<u8'0'>(value);
}

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr chrono_padding decode_chrono_padding(char_type value) noexcept
{
	if (is_syntax_character<u8'_'>(value))
	{
		return chrono_padding::space;
	}
	if (is_syntax_character<u8'-'>(value))
	{
		return chrono_padding::none;
	}
	return chrono_padding::zero;
}

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr bool decode_chrono_opcode(
	char_type value, chrono_opcode &opcode) noexcept
{
	if (is_syntax_character<u8'a'>(value))
	{
		opcode = chrono_opcode::abbreviated_weekday;
	}
	else if (is_syntax_character<u8'A'>(value))
	{
		opcode = chrono_opcode::full_weekday;
	}
	else if (is_syntax_character<u8'b'>(value) || is_syntax_character<u8'h'>(value))
	{
		opcode = chrono_opcode::abbreviated_month;
	}
	else if (is_syntax_character<u8'B'>(value))
	{
		opcode = chrono_opcode::full_month;
	}
	else if (is_syntax_character<u8'c'>(value))
	{
		opcode = chrono_opcode::date_time;
	}
	else if (is_syntax_character<u8'C'>(value))
	{
		opcode = chrono_opcode::century;
	}
	else if (is_syntax_character<u8'd'>(value))
	{
		opcode = chrono_opcode::day_of_month;
	}
	else if (is_syntax_character<u8'D'>(value))
	{
		opcode = chrono_opcode::us_date;
	}
	else if (is_syntax_character<u8'e'>(value))
	{
		opcode = chrono_opcode::space_day_of_month;
	}
	else if (is_syntax_character<u8'F'>(value))
	{
		opcode = chrono_opcode::iso_date;
	}
	else if (is_syntax_character<u8'g'>(value))
	{
		opcode = chrono_opcode::iso_week_based_short_year;
	}
	else if (is_syntax_character<u8'G'>(value))
	{
		opcode = chrono_opcode::iso_week_based_year;
	}
	else if (is_syntax_character<u8'H'>(value))
	{
		opcode = chrono_opcode::hour_24;
	}
	else if (is_syntax_character<u8'I'>(value))
	{
		opcode = chrono_opcode::hour_12;
	}
	else if (is_syntax_character<u8'j'>(value))
	{
		opcode = chrono_opcode::day_of_year;
	}
	else if (is_syntax_character<u8'm'>(value))
	{
		opcode = chrono_opcode::month;
	}
	else if (is_syntax_character<u8'M'>(value))
	{
		opcode = chrono_opcode::minute;
	}
	else if (is_syntax_character<u8'n'>(value))
	{
		opcode = chrono_opcode::newline;
	}
	else if (is_syntax_character<u8'p'>(value))
	{
		opcode = chrono_opcode::am_pm;
	}
	else if (is_syntax_character<u8'q'>(value))
	{
		opcode = chrono_opcode::duration_unit;
	}
	else if (is_syntax_character<u8'Q'>(value))
	{
		opcode = chrono_opcode::duration_value;
	}
	else if (is_syntax_character<u8'r'>(value))
	{
		opcode = chrono_opcode::time_12;
	}
	else if (is_syntax_character<u8'R'>(value))
	{
		opcode = chrono_opcode::time_hm;
	}
	else if (is_syntax_character<u8'S'>(value))
	{
		opcode = chrono_opcode::second;
	}
	else if (is_syntax_character<u8't'>(value))
	{
		opcode = chrono_opcode::tab;
	}
	else if (is_syntax_character<u8'T'>(value))
	{
		opcode = chrono_opcode::time_hms;
	}
	else if (is_syntax_character<u8'u'>(value))
	{
		opcode = chrono_opcode::iso_weekday;
	}
	else if (is_syntax_character<u8'U'>(value))
	{
		opcode = chrono_opcode::sunday_week;
	}
	else if (is_syntax_character<u8'V'>(value))
	{
		opcode = chrono_opcode::iso_week;
	}
	else if (is_syntax_character<u8'w'>(value))
	{
		opcode = chrono_opcode::sunday_weekday;
	}
	else if (is_syntax_character<u8'W'>(value))
	{
		opcode = chrono_opcode::monday_week;
	}
	else if (is_syntax_character<u8'x'>(value))
	{
		opcode = chrono_opcode::locale_date;
	}
	else if (is_syntax_character<u8'X'>(value))
	{
		opcode = chrono_opcode::locale_time;
	}
	else if (is_syntax_character<u8'y'>(value))
	{
		opcode = chrono_opcode::short_year;
	}
	else if (is_syntax_character<u8'Y'>(value))
	{
		opcode = chrono_opcode::year;
	}
	else if (is_syntax_character<u8'z'>(value))
	{
		opcode = chrono_opcode::utc_offset;
	}
	else if (is_syntax_character<u8'Z'>(value))
	{
		opcode = chrono_opcode::time_zone_name;
	}
	else if (is_syntax_character<u8'%'>(value))
	{
		opcode = chrono_opcode::percent;
	}
	else
	{
		return false;
	}
	return true;
}

[[nodiscard]] inline constexpr bool chrono_opcode_supported(
	chrono_value_domain domain, chrono_opcode opcode) noexcept
{
	if (domain == chrono_value_domain::calendar)
	{
		return opcode != chrono_opcode::duration_value &&
			   opcode != chrono_opcode::duration_unit;
	}

	// fmt's duration grammar intentionally exposes clock-like decompositions but
	// rejects every calendar component at compile time.
	return opcode == chrono_opcode::literal || opcode == chrono_opcode::percent ||
		   opcode == chrono_opcode::newline || opcode == chrono_opcode::tab ||
		   opcode == chrono_opcode::hour_24 || opcode == chrono_opcode::hour_12 ||
		   opcode == chrono_opcode::day_of_year || opcode == chrono_opcode::minute ||
		   opcode == chrono_opcode::am_pm || opcode == chrono_opcode::duration_unit ||
		   opcode == chrono_opcode::duration_value || opcode == chrono_opcode::time_12 ||
		   opcode == chrono_opcode::time_hm || opcode == chrono_opcode::second ||
		   opcode == chrono_opcode::time_hms;
}

[[nodiscard]] inline constexpr bool chrono_padding_supported(chrono_opcode opcode) noexcept
{
	return opcode == chrono_opcode::hour_24 || opcode == chrono_opcode::hour_12 ||
		   opcode == chrono_opcode::minute || opcode == chrono_opcode::second ||
		   opcode == chrono_opcode::sunday_week || opcode == chrono_opcode::iso_week ||
		   opcode == chrono_opcode::monday_week || opcode == chrono_opcode::year ||
		   opcode == chrono_opcode::day_of_month || opcode == chrono_opcode::space_day_of_month ||
		   opcode == chrono_opcode::day_of_year || opcode == chrono_opcode::month;
}

/**
 * Compiles one chrono sub-specification into a compact flat program.
 *
 * `specification` addresses the original structural format literal and excludes
 * the generic brace width/precision prefix and the closing brace.  Parsing is
 * immediate-only; no non-consteval overload is provided.  The later emitter
 * expands every operation index into a distinct template instantiation, so the
 * program is compiler metadata rather than a run-time bytecode stream.
 */
template <::fast_io::fmt::basic_fixed_string format_literal, source_slice specification,
		  chrono_value_domain domain>
[[nodiscard]] consteval auto parse_chrono_program() noexcept
{
	using char_type = typename decltype(format_literal)::value_type;
	constexpr ::std::size_t format_size{format_literal.size()};
	using program_type = basic_chrono_program<char_type, specification.size == 0u ? 2u : specification.size>;
	chrono_parse_result<program_type> result{};

	if (specification.offset > format_size ||
		specification.size > format_size - specification.offset)
	{
		set_chrono_error(result, chrono_parse_error::invalid_slice, specification.offset);
		return result;
	}

	if constexpr (specification.size == 0u)
	{
		if constexpr (domain == chrono_value_domain::duration)
		{
			(void)result.program.append_conversion(chrono_opcode::duration_value);
			(void)result.program.append_conversion(chrono_opcode::duration_unit);
		}
		else
		{
			(void)result.program.append_conversion(chrono_opcode::default_date_time);
		}
		return result;
	}

	constexpr ::std::size_t end{specification.offset + specification.size};
	::std::size_t cursor{specification.offset};
	while (cursor != end)
	{
		auto const value{format_literal[cursor]};
		if (!is_syntax_character<u8'%'>(value))
		{
			if (is_syntax_character<u8'{'>(value) || is_syntax_character<u8'}'>(value))
			{
				set_chrono_error(result, chrono_parse_error::brace_in_chrono_literal, cursor);
				return result;
			}
			if (!result.program.append_literal(cursor))
			{
				set_chrono_error(result, chrono_parse_error::capacity_exceeded, cursor);
				return result;
			}
			++cursor;
			continue;
		}

		::std::size_t const percent_position{cursor++};
		if (cursor == end)
		{
			set_chrono_error(result, chrono_parse_error::dangling_percent, percent_position);
			return result;
		}

		chrono_padding padding{chrono_padding::zero};
		if (is_chrono_padding_modifier(format_literal[cursor]))
		{
			padding = decode_chrono_padding(format_literal[cursor]);
			++cursor;
			if (cursor == end)
			{
				set_chrono_error(result, chrono_parse_error::dangling_percent, percent_position);
				return result;
			}
		}

		// E/O request locale-specific alternate representations.  Silently treating
		// them as ASCII would be observably wrong on a non-C locale, so the basic,
		// allocation-free backend rejects them until an explicit locale object is
		// carried by the public format front door.
		if (is_syntax_character<u8'E'>(format_literal[cursor]) ||
			is_syntax_character<u8'O'>(format_literal[cursor]))
		{
			set_chrono_error(result, chrono_parse_error::locale_modifier_not_supported, cursor);
			return result;
		}

		chrono_opcode opcode{};
		if (!decode_chrono_opcode(format_literal[cursor], opcode))
		{
			set_chrono_error(result, chrono_parse_error::invalid_conversion, cursor);
			return result;
		}
		if (!chrono_opcode_supported(domain, opcode))
		{
			set_chrono_error(result, chrono_parse_error::conversion_not_supported_for_value, cursor);
			return result;
		}
		if (padding != chrono_padding::zero && !chrono_padding_supported(opcode))
		{
			set_chrono_error(result, chrono_parse_error::invalid_conversion, cursor);
			return result;
		}
		if (!result.program.append_conversion(opcode, padding))
		{
			set_chrono_error(result, chrono_parse_error::capacity_exceeded, cursor);
			return result;
		}
		++cursor;
	}
	return result;
}

template <chrono_parse_error error>
consteval void diagnose_chrono_parse_error()
{
	if constexpr (error == chrono_parse_error::invalid_slice)
	{
		static_assert(error == chrono_parse_error::none, "fast_io chrono: invalid source slice");
	}
	else if constexpr (error == chrono_parse_error::dangling_percent)
	{
		static_assert(error == chrono_parse_error::none, "fast_io chrono: dangling percent conversion");
	}
	else if constexpr (error == chrono_parse_error::invalid_conversion)
	{
		static_assert(error == chrono_parse_error::none, "fast_io chrono: invalid conversion specifier");
	}
	else if constexpr (error == chrono_parse_error::conversion_not_supported_for_value)
	{
		static_assert(error == chrono_parse_error::none,
					  "fast_io chrono: conversion is not supported for this chrono value domain");
	}
	else if constexpr (error == chrono_parse_error::locale_modifier_not_supported)
	{
		static_assert(error == chrono_parse_error::none,
					  "fast_io chrono: E/O locale modifiers require a locale-aware format overload");
	}
	else if constexpr (error == chrono_parse_error::brace_in_chrono_literal)
	{
		static_assert(error == chrono_parse_error::none,
					  "fast_io chrono: brace is not permitted as a chrono literal");
	}
	else if constexpr (error == chrono_parse_error::capacity_exceeded)
	{
		static_assert(error == chrono_parse_error::none, "fast_io chrono: internal program capacity exceeded");
	}
}

template <::fast_io::fmt::basic_fixed_string format_literal, source_slice specification,
		  chrono_value_domain domain>
inline constexpr auto checked_chrono_program = []() consteval {
	constexpr auto parsed{parse_chrono_program<format_literal, specification, domain>()};
	if constexpr (parsed.error != chrono_parse_error::none)
	{
		diagnose_chrono_parse_error<parsed.error>();
	}
	return parsed.program;
}();

} // namespace fast_io::fmt::details

namespace fast_io::fmt::details
{

template <typename duration_type>
struct chrono_duration_state;

template <typename rep_type, typename period_type>
struct chrono_duration_state<::std::chrono::duration<rep_type, period_type>>
{
	using duration_type = ::std::chrono::duration<rep_type, period_type>;
	using rep = rep_type;
	using period = period_type;
	using magnitude_type = ::std::conditional_t<::std::integral<rep_type>,
												::std::make_unsigned_t<rep_type>, rep_type>;

	magnitude_type magnitude{};
	long double absolute_seconds{};
	bool negative_pending{};
	bool finite{true};
	bool not_a_number{};
	unsigned default_fractional_precision{};
};

template <typename rep_type, typename period_type>
[[nodiscard]] inline chrono_duration_state<::std::chrono::duration<rep_type, period_type>>
make_chrono_duration_state(::std::chrono::duration<rep_type, period_type> value) noexcept
{
	static_assert(::std::integral<rep_type> || ::std::floating_point<rep_type>,
				  "fast_io chrono: duration representation must be an arithmetic scalar");
	using state_type = chrono_duration_state<::std::chrono::duration<rep_type, period_type>>;
	state_type state{};
	if constexpr (::std::integral<rep_type>)
	{
		state.negative_pending = ::std::is_signed_v<rep_type> && value.count() < 0;
		state.magnitude = chrono_unsigned_magnitude(value.count());
		state.absolute_seconds = static_cast<long double>(state.magnitude) *
								 static_cast<long double>(period_type::num) /
								 static_cast<long double>(period_type::den);
		state.default_fractional_precision = chrono_fractional_digits<period_type>();
	}
	else
	{
		auto const count{value.count()};
		state.finite = ::std::isfinite(count);
		state.not_a_number = ::std::isnan(count);
		state.negative_pending = ::std::signbit(count) && !state.not_a_number;
		state.magnitude = static_cast<rep_type>(::std::fabs(count));
		state.absolute_seconds = static_cast<long double>(state.magnitude) *
								 static_cast<long double>(period_type::num) /
								 static_cast<long double>(period_type::den);
		state.default_fractional_precision = chrono_fractional_digits<period_type>();
		if (state.finite && state.default_fractional_precision < 6u &&
			::std::round(state.absolute_seconds) != state.absolute_seconds)
		{
			state.default_fractional_precision = 6u;
		}
	}
	return state;
}

template <::fast_io::fmt::format_character char_type, typename state_type>
inline constexpr char_type *consume_chrono_duration_sign(
	char_type *output, state_type &state) noexcept
{
	if (state.negative_pending)
	{
		*output++ = chrono_basic_latin<char_type>(u8'-');
		state.negative_pending = false;
	}
	return output;
}

template <typename state_type>
[[nodiscard]] inline constexpr ::std::size_t consume_chrono_duration_sign_size(
	state_type &state) noexcept
{
	auto const size{static_cast<::std::size_t>(state.negative_pending)};
	state.negative_pending = false;
	return size;
}

template <::fast_io::fmt::format_character char_type, typename state_type>
inline char_type *write_chrono_nonfinite(char_type *output, state_type &state) noexcept
{
	output = consume_chrono_duration_sign(output, state);
	return state.not_a_number ? write_chrono_ascii(output, u8"nan") : write_chrono_ascii(output, u8"inf");
}

template <typename state_type>
[[nodiscard]] inline ::std::uintmax_t chrono_duration_component(
	state_type const &state, long double divisor, ::std::uintmax_t modulus = 0u)
{
	auto result{::std::floor(state.absolute_seconds / divisor)};
	if (modulus != 0u)
	{
		result = ::std::fmod(result, static_cast<long double>(modulus));
	}
	if (result < 0.0L ||
		result > static_cast<long double>((::std::numeric_limits<::std::uintmax_t>::max)()))
	{
		::fast_io::fast_terminate();
	}
	return static_cast<::std::uintmax_t>(result);
}

template <bool fixed, ::fast_io::fmt::format_character char_type,
	typename floating_type>
inline char_type *write_chrono_floating(
	char_type *output, floating_type value, ::std::size_t precision)
{
	using alias_type = ::fast_io::details::float_alias_type<floating_type>;
	if constexpr (fixed)
	{
		constexpr auto flags{::fast_io::manipulators::scalar_flags{
			.floating = ::fast_io::manipulators::floating_format::fixed,
			.precision = ::fast_io::manipulators::floating_precision::fractional_preserve_trailing_zero}};
		auto scalar{::fast_io::manipulators::scalar_manip_precision_t<flags, alias_type>{
			static_cast<alias_type>(value), precision}};
		static_assert(::fast_io::dynamic_reserve_printable<
			char_type, decltype(scalar)>,
			"fast_io chrono: the floating representation has no exact fixed-precision print concept");
		return ::fast_io::print_reserve_define(
			::fast_io::io_reserve_type<char_type, decltype(scalar)>, output, scalar);
	}
	else
	{
		constexpr auto flags{
			::fast_io::manipulators::floating_point_default_scalar_flags};
		auto scalar{::fast_io::manipulators::scalar_manip_t<flags, alias_type>{
			static_cast<alias_type>(value)}};
		static_assert(::fast_io::reserve_printable<char_type, decltype(scalar)>,
			"fast_io chrono: the floating representation has no shortest-decimal print concept; request an explicit precision");
		return ::fast_io::print_reserve_define(
			::fast_io::io_reserve_type<char_type, decltype(scalar)>, output, scalar);
	}
}

template <bool fixed, ::fast_io::fmt::format_character char_type,
	typename floating_type>
[[nodiscard]] inline ::std::size_t chrono_floating_capacity(
	floating_type value, ::std::size_t precision) noexcept
{
	using alias_type = ::fast_io::details::float_alias_type<floating_type>;
	if constexpr (fixed)
	{
		constexpr auto flags{::fast_io::manipulators::scalar_flags{
			.floating = ::fast_io::manipulators::floating_format::fixed,
			.precision = ::fast_io::manipulators::floating_precision::fractional_preserve_trailing_zero}};
		auto scalar{::fast_io::manipulators::scalar_manip_precision_t<flags, alias_type>{
			static_cast<alias_type>(value), precision}};
		static_assert(::fast_io::dynamic_reserve_printable<
			char_type, decltype(scalar)>,
			"fast_io chrono: the floating representation has no exact fixed-precision print concept");
		return ::fast_io::print_reserve_size(
			::fast_io::io_reserve_type<char_type, decltype(scalar)>, scalar);
	}
	else
	{
		constexpr auto flags{
			::fast_io::manipulators::floating_point_default_scalar_flags};
		using scalar_type =
			::fast_io::manipulators::scalar_manip_t<flags, alias_type>;
		static_assert(::fast_io::reserve_printable<char_type, scalar_type>,
			"fast_io chrono: the floating representation has no shortest-decimal print concept; request an explicit precision");
		return ::fast_io::print_reserve_size(
			::fast_io::io_reserve_type<char_type, scalar_type>);
	}
}

template <typename period_type>
[[nodiscard]] inline constexpr ::std::size_t chrono_duration_unit_capacity() noexcept
{
	if constexpr (::std::same_as<period_type, ::std::atto> ||
				  ::std::same_as<period_type, ::std::femto> ||
				  ::std::same_as<period_type, ::std::pico> ||
				  ::std::same_as<period_type, ::std::nano> ||
				  ::std::same_as<period_type, ::std::micro> ||
				  ::std::same_as<period_type, ::std::milli> ||
				  ::std::same_as<period_type, ::std::centi> ||
				  ::std::same_as<period_type, ::std::deci> ||
				  ::std::same_as<period_type, ::std::ratio<1>>)
	{
		return 2u;
	}
	else if constexpr (::std::same_as<period_type, ::std::deca> ||
					   ::std::same_as<period_type, ::std::hecto> ||
					   ::std::same_as<period_type, ::std::kilo> ||
					   ::std::same_as<period_type, ::std::mega> ||
					   ::std::same_as<period_type, ::std::giga> ||
					   ::std::same_as<period_type, ::std::tera> ||
					   ::std::same_as<period_type, ::std::peta> ||
					   ::std::same_as<period_type, ::std::exa> ||
					   ::std::same_as<period_type, ::std::ratio<60>>)
	{
		return 3u;
	}
	else if constexpr (::std::same_as<period_type, ::std::ratio<3600>> ||
					   ::std::same_as<period_type, ::std::ratio<86400>>)
	{
		return 1u;
	}
	else
	{
		return 2u * print_reserve_size(
						::fast_io::io_reserve_type<char, ::std::intmax_t>) +
			   4u;
	}
}

template <::fast_io::fmt::format_character char_type, typename period_type>
inline char_type *write_chrono_duration_unit(char_type *output) noexcept
{
	if constexpr (::std::same_as<period_type, ::std::atto>)
	{
		return write_chrono_ascii(output, u8"as");
	}
	else if constexpr (::std::same_as<period_type, ::std::femto>)
	{
		return write_chrono_ascii(output, u8"fs");
	}
	else if constexpr (::std::same_as<period_type, ::std::pico>)
	{
		return write_chrono_ascii(output, u8"ps");
	}
	else if constexpr (::std::same_as<period_type, ::std::nano>)
	{
		return write_chrono_ascii(output, u8"ns");
	}
	else if constexpr (::std::same_as<period_type, ::std::micro>)
	{
		return write_chrono_ascii(output, u8"us");
	}
	else if constexpr (::std::same_as<period_type, ::std::milli>)
	{
		return write_chrono_ascii(output, u8"ms");
	}
	else if constexpr (::std::same_as<period_type, ::std::centi>)
	{
		return write_chrono_ascii(output, u8"cs");
	}
	else if constexpr (::std::same_as<period_type, ::std::deci>)
	{
		return write_chrono_ascii(output, u8"ds");
	}
	else if constexpr (::std::same_as<period_type, ::std::ratio<1>>)
	{
		return write_chrono_ascii(output, u8"s");
	}
	else if constexpr (::std::same_as<period_type, ::std::deca>)
	{
		return write_chrono_ascii(output, u8"das");
	}
	else if constexpr (::std::same_as<period_type, ::std::hecto>)
	{
		return write_chrono_ascii(output, u8"hs");
	}
	else if constexpr (::std::same_as<period_type, ::std::kilo>)
	{
		return write_chrono_ascii(output, u8"ks");
	}
	else if constexpr (::std::same_as<period_type, ::std::mega>)
	{
		return write_chrono_ascii(output, u8"Ms");
	}
	else if constexpr (::std::same_as<period_type, ::std::giga>)
	{
		return write_chrono_ascii(output, u8"Gs");
	}
	else if constexpr (::std::same_as<period_type, ::std::tera>)
	{
		return write_chrono_ascii(output, u8"Ts");
	}
	else if constexpr (::std::same_as<period_type, ::std::peta>)
	{
		return write_chrono_ascii(output, u8"Ps");
	}
	else if constexpr (::std::same_as<period_type, ::std::exa>)
	{
		return write_chrono_ascii(output, u8"Es");
	}
	else if constexpr (::std::same_as<period_type, ::std::ratio<60>>)
	{
		return write_chrono_ascii(output, u8"min");
	}
	else if constexpr (::std::same_as<period_type, ::std::ratio<3600>>)
	{
		return write_chrono_ascii(output, u8"h");
	}
	else if constexpr (::std::same_as<period_type, ::std::ratio<86400>>)
	{
		return write_chrono_ascii(output, u8"d");
	}
	else
	{
		output = write_chrono_separator(output, u8'[');
		output = write_chrono_integer(output, static_cast<::std::intmax_t>(period_type::num));
		if constexpr (period_type::den != 1)
		{
			output = write_chrono_separator(output, u8'/');
			output = write_chrono_integer(output, static_cast<::std::intmax_t>(period_type::den));
		}
		output = write_chrono_separator(output, u8']');
		return write_chrono_separator(output, u8's');
	}
}

template <bool has_precision, ::fast_io::fmt::format_character char_type,
		  typename state_type>
inline char_type *write_chrono_duration_value(
	char_type *output, state_type &state, ::std::size_t precision)
{
	output = consume_chrono_duration_sign(output, state);
	if constexpr (::std::integral<typename state_type::rep>)
	{
		static_assert(!has_precision,
					  "fast_io chrono: precision is not permitted for an integral duration representation");
		return write_chrono_integer(output, state.magnitude);
	}
	else
	{
		return write_chrono_floating<has_precision, char_type>(
			output, state.magnitude, precision);
	}
}

template <bool has_precision, ::fast_io::fmt::format_character char_type,
		  typename state_type>
[[nodiscard]] inline ::std::size_t chrono_duration_value_capacity(
	state_type &state, ::std::size_t precision) noexcept
{
	auto capacity{consume_chrono_duration_sign_size(state)};
	if constexpr (::std::integral<typename state_type::rep>)
	{
		static_assert(!has_precision,
					  "fast_io chrono: precision is not permitted for an integral duration representation");
		return capacity + print_reserve_size(
							  ::fast_io::io_reserve_type<char, typename state_type::magnitude_type>);
	}
	else
	{
		return capacity + chrono_floating_capacity<has_precision, char_type>(
							  state.magnitude, precision);
	}
}

template <bool has_precision, ::fast_io::fmt::format_character char_type,
		  typename state_type>
inline char_type *write_chrono_duration_second(
	char_type *output, state_type &state, ::std::size_t precision,
	chrono_padding padding)
{
	if (!state.finite)
	{
		return write_chrono_nonfinite(output, state);
	}
	output = consume_chrono_duration_sign(output, state);
	auto const second{::std::fmod(state.absolute_seconds, 60.0L)};
	auto const actual_precision{
		has_precision ? static_cast<unsigned>(precision) : state.default_fractional_precision};
	if (second < 10.0L && padding != chrono_padding::none)
	{
		*output++ = chrono_basic_latin<char_type>(
			padding == chrono_padding::space ? u8' ' : u8'0');
	}
	if (actual_precision == 0u)
	{
		return write_chrono_integer(output, static_cast<::std::uintmax_t>(second));
	}
	return write_chrono_floating<true, char_type>(
		output, second, actual_precision);
}

template <bool has_precision, ::fast_io::fmt::format_character char_type,
		  typename state_type>
[[nodiscard]] inline ::std::size_t chrono_duration_second_capacity(
	state_type &state, ::std::size_t precision, chrono_padding padding) noexcept
{
	auto capacity{consume_chrono_duration_sign_size(state)};
	if (!state.finite)
	{
		return capacity + 3u;
	}
	auto const second{::std::fmod(state.absolute_seconds, 60.0L)};
	auto const actual_precision{
		has_precision ? static_cast<unsigned>(precision) : state.default_fractional_precision};
	capacity += static_cast<::std::size_t>(second < 10.0L && padding != chrono_padding::none);
	if (actual_precision == 0u)
	{
		return capacity + 2u;
	}
	return capacity + chrono_floating_capacity<true, char_type>(
						  second, actual_precision);
}

template <chrono_opcode opcode, chrono_padding padding, bool has_precision,
		  ::fast_io::fmt::format_character char_type, typename state_type>
inline char_type *emit_chrono_duration_operation(
	char_type *output, state_type &state, ::std::size_t precision)
{
	if constexpr (opcode == chrono_opcode::duration_unit)
	{
		return write_chrono_duration_unit<char_type, typename state_type::period>(output);
	}
	else if constexpr (opcode == chrono_opcode::duration_value)
	{
		return write_chrono_duration_value<has_precision>(output, state, precision);
	}
	else if constexpr (opcode == chrono_opcode::second)
	{
		return write_chrono_duration_second<has_precision>(output, state, precision, padding);
	}
	else if constexpr (opcode == chrono_opcode::am_pm)
	{
		if (!state.finite)
		{
			return write_chrono_nonfinite(output, state);
		}
		auto const hour{chrono_duration_component(state, 3600.0L, 24u)};
		return write_chrono_ascii(output, hour < 12u ? u8"AM" : u8"PM");
	}
	else if constexpr (opcode == chrono_opcode::day_of_year ||
					   opcode == chrono_opcode::hour_24 || opcode == chrono_opcode::hour_12 ||
					   opcode == chrono_opcode::minute)
	{
		if (!state.finite)
		{
			return write_chrono_nonfinite(output, state);
		}
		output = consume_chrono_duration_sign(output, state);
		auto component = [&]() {
			if constexpr (opcode == chrono_opcode::day_of_year)
			{
				return chrono_duration_component(state, 86400.0L);
			}
			else if constexpr (opcode == chrono_opcode::hour_24)
			{
				return chrono_duration_component(state, 3600.0L, 24u);
			}
			else if constexpr (opcode == chrono_opcode::hour_12)
			{
				auto result{chrono_duration_component(state, 3600.0L, 12u)};
				return result == 0u ? static_cast<::std::uintmax_t>(12u) : result;
			}
			else
			{
				return chrono_duration_component(state, 60.0L, 60u);
			}
		}();
		constexpr unsigned width{opcode == chrono_opcode::day_of_year ? 0u : 2u};
		return write_chrono_padded_integer(output, component, width, padding);
	}
	else if constexpr (opcode == chrono_opcode::time_hm ||
					   opcode == chrono_opcode::time_hms || opcode == chrono_opcode::time_12)
	{
		if (!state.finite)
		{
			return write_chrono_nonfinite(output, state);
		}
		output = consume_chrono_duration_sign(output, state);
		auto hour{chrono_duration_component(state, 3600.0L,
											opcode == chrono_opcode::time_12 ? 12u : 24u)};
		if constexpr (opcode == chrono_opcode::time_12)
		{
			if (hour == 0u)
			{
				hour = 12u;
			}
		}
		output = write_chrono_padded_integer(output, hour, 2u, chrono_padding::zero);
		output = write_chrono_separator(output, u8':');
		output = write_chrono_padded_integer(output,
											 chrono_duration_component(state, 60.0L, 60u), 2u, chrono_padding::zero);
		if constexpr (opcode != chrono_opcode::time_hm)
		{
			output = write_chrono_separator(output, u8':');
			output = write_chrono_duration_second<has_precision>(
				output, state, precision, chrono_padding::zero);
		}
		if constexpr (opcode == chrono_opcode::time_12)
		{
			output = write_chrono_separator(output, u8' ');
			output = write_chrono_ascii(output,
										chrono_duration_component(state, 3600.0L, 24u) < 12u ? u8"AM" : u8"PM");
		}
		return output;
	}
	else
	{
		static_assert(opcode == chrono_opcode::literal || opcode == chrono_opcode::percent ||
						  opcode == chrono_opcode::newline || opcode == chrono_opcode::tab,
					  "fast_io chrono: unhandled duration opcode");
		return output;
	}
}

template <chrono_opcode opcode, chrono_padding padding, bool has_precision,
		  ::fast_io::fmt::format_character char_type, typename state_type>
[[nodiscard]] inline ::std::size_t chrono_duration_operation_capacity(
	state_type &state, ::std::size_t precision)
{
	if constexpr (opcode == chrono_opcode::duration_unit)
	{
		return chrono_duration_unit_capacity<typename state_type::period>();
	}
	else if constexpr (opcode == chrono_opcode::duration_value)
	{
		return chrono_duration_value_capacity<has_precision, char_type>(state, precision);
	}
	else if constexpr (opcode == chrono_opcode::second)
	{
		return chrono_duration_second_capacity<has_precision, char_type>(state, precision, padding);
	}
	else if constexpr (opcode == chrono_opcode::am_pm)
	{
		return state.finite ? 2u : consume_chrono_duration_sign_size(state) + 3u;
	}
	else if constexpr (opcode == chrono_opcode::day_of_year)
	{
		return consume_chrono_duration_sign_size(state) +
			   print_reserve_size(::fast_io::io_reserve_type<char, ::std::uintmax_t>);
	}
	else if constexpr (opcode == chrono_opcode::hour_24 ||
					   opcode == chrono_opcode::hour_12 || opcode == chrono_opcode::minute)
	{
		return consume_chrono_duration_sign_size(state) + 3u;
	}
	else if constexpr (opcode == chrono_opcode::time_hm)
	{
		return consume_chrono_duration_sign_size(state) + 5u;
	}
	else if constexpr (opcode == chrono_opcode::time_hms)
	{
		return consume_chrono_duration_sign_size(state) + 6u +
			   chrono_duration_second_capacity<has_precision, char_type>(state, precision, chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::time_12)
	{
		return consume_chrono_duration_sign_size(state) + 9u +
			   chrono_duration_second_capacity<has_precision, char_type>(state, precision, chrono_padding::zero);
	}
	else
	{
		return 1u;
	}
}

} // namespace fast_io::fmt::details

namespace fast_io::fmt::details
{

template <::fast_io::fmt::format_character char_type>
inline char_type *write_chrono_weekday_name(
	char_type *output, unsigned weekday, bool full)
{
	if (weekday > 6u)
	{
		::fast_io::fast_terminate();
	}
	if (full)
	{
		switch (weekday)
		{
		case 0u:
			return write_chrono_ascii(output, u8"Sunday");
		case 1u:
			return write_chrono_ascii(output, u8"Monday");
		case 2u:
			return write_chrono_ascii(output, u8"Tuesday");
		case 3u:
			return write_chrono_ascii(output, u8"Wednesday");
		case 4u:
			return write_chrono_ascii(output, u8"Thursday");
		case 5u:
			return write_chrono_ascii(output, u8"Friday");
		default:
			return write_chrono_ascii(output, u8"Saturday");
		}
	}
	switch (weekday)
	{
	case 0u:
		return write_chrono_ascii(output, u8"Sun");
	case 1u:
		return write_chrono_ascii(output, u8"Mon");
	case 2u:
		return write_chrono_ascii(output, u8"Tue");
	case 3u:
		return write_chrono_ascii(output, u8"Wed");
	case 4u:
		return write_chrono_ascii(output, u8"Thu");
	case 5u:
		return write_chrono_ascii(output, u8"Fri");
	default:
		return write_chrono_ascii(output, u8"Sat");
	}
}

[[nodiscard]] inline constexpr ::std::size_t chrono_weekday_name_size(
	unsigned weekday, bool full)
{
	if (!full)
	{
		return 3u;
	}
	switch (weekday)
	{
	case 0u:
		return 6u;
	case 1u:
		return 6u;
	case 2u:
		return 7u;
	case 3u:
		return 9u;
	case 4u:
		return 8u;
	case 5u:
		return 6u;
	default:
		return 8u;
	}
}

template <::fast_io::fmt::format_character char_type>
inline char_type *write_chrono_month_name(
	char_type *output, unsigned month, bool full)
{
	if (month < 1u || month > 12u)
	{
		::fast_io::fast_terminate();
	}
	if (full)
	{
		switch (month)
		{
		case 1u:
			return write_chrono_ascii(output, u8"January");
		case 2u:
			return write_chrono_ascii(output, u8"February");
		case 3u:
			return write_chrono_ascii(output, u8"March");
		case 4u:
			return write_chrono_ascii(output, u8"April");
		case 5u:
			return write_chrono_ascii(output, u8"May");
		case 6u:
			return write_chrono_ascii(output, u8"June");
		case 7u:
			return write_chrono_ascii(output, u8"July");
		case 8u:
			return write_chrono_ascii(output, u8"August");
		case 9u:
			return write_chrono_ascii(output, u8"September");
		case 10u:
			return write_chrono_ascii(output, u8"October");
		case 11u:
			return write_chrono_ascii(output, u8"November");
		default:
			return write_chrono_ascii(output, u8"December");
		}
	}
	switch (month)
	{
	case 1u:
		return write_chrono_ascii(output, u8"Jan");
	case 2u:
		return write_chrono_ascii(output, u8"Feb");
	case 3u:
		return write_chrono_ascii(output, u8"Mar");
	case 4u:
		return write_chrono_ascii(output, u8"Apr");
	case 5u:
		return write_chrono_ascii(output, u8"May");
	case 6u:
		return write_chrono_ascii(output, u8"Jun");
	case 7u:
		return write_chrono_ascii(output, u8"Jul");
	case 8u:
		return write_chrono_ascii(output, u8"Aug");
	case 9u:
		return write_chrono_ascii(output, u8"Sep");
	case 10u:
		return write_chrono_ascii(output, u8"Oct");
	case 11u:
		return write_chrono_ascii(output, u8"Nov");
	default:
		return write_chrono_ascii(output, u8"Dec");
	}
}

[[nodiscard]] inline constexpr ::std::size_t chrono_month_name_size(
	unsigned month, bool full)
{
	if (!full)
	{
		return 3u;
	}
	switch (month)
	{
	case 1u:
		return 7u;
	case 2u:
		return 8u;
	case 3u:
		return 5u;
	case 4u:
		return 5u;
	case 5u:
		return 3u;
	case 6u:
		return 4u;
	case 7u:
		return 4u;
	case 8u:
		return 6u;
	case 9u:
		return 9u;
	case 10u:
		return 7u;
	case 11u:
		return 8u;
	default:
		return 8u;
	}
}

[[nodiscard]] inline constexpr int chrono_floor_century(int year) noexcept
{
	return static_cast<int>(chrono_floor_divide(year, 100));
}

[[nodiscard]] inline constexpr unsigned chrono_short_year(int year) noexcept
{
	return chrono_positive_modulo(year, 100u);
}

template <::fast_io::fmt::format_character char_type>
inline constexpr char_type *write_chrono_separator(char_type *output, char8_t separator) noexcept
{
	*output++ = chrono_basic_latin<char_type>(separator);
	return output;
}

template <::fast_io::fmt::format_character char_type>
inline char_type *write_chrono_calendar_second(
	char_type *output, chrono_calendar_state const &state, chrono_padding padding)
{
	auto const second{chrono_tm_second(state.value)};
	if (state.fractional_precision == 0u)
	{
		return write_chrono_padded_integer(output, second, 2u, padding);
	}
	long double const seconds{
		static_cast<long double>(second) + state.fractional_second};
	constexpr auto flags{::fast_io::manipulators::scalar_flags{
		.floating = ::fast_io::manipulators::floating_format::fixed,
		.precision = ::fast_io::manipulators::floating_precision::fractional_preserve_trailing_zero}};
	using alias_type = ::fast_io::details::float_alias_type<long double>;
	auto scalar{::fast_io::manipulators::scalar_manip_precision_t<flags, alias_type>{
		static_cast<alias_type>(seconds), state.fractional_precision}};
	if (seconds < 10.0L && padding != chrono_padding::none)
	{
		*output++ = chrono_basic_latin<char_type>(
			padding == chrono_padding::space ? u8' ' : u8'0');
	}
	return print_reserve_define(
		::fast_io::io_reserve_type<char_type, decltype(scalar)>, output, scalar);
}

[[nodiscard]] inline ::std::size_t chrono_calendar_second_capacity(
	chrono_calendar_state const &state, chrono_padding padding) noexcept
{
	auto const second{chrono_tm_second(state.value)};
	if (state.fractional_precision == 0u)
	{
		return chrono_padded_integer_size(second, 2u, padding);
	}
	constexpr auto flags{::fast_io::manipulators::scalar_flags{
		.floating = ::fast_io::manipulators::floating_format::fixed,
		.precision = ::fast_io::manipulators::floating_precision::fractional_preserve_trailing_zero}};
	using alias_type = ::fast_io::details::float_alias_type<long double>;
	auto scalar{::fast_io::manipulators::scalar_manip_precision_t<flags, alias_type>{
		static_cast<alias_type>(second + state.fractional_second),
		state.fractional_precision}};
	return static_cast<::std::size_t>(second < 10u && padding != chrono_padding::none) +
		   print_reserve_size(::fast_io::io_reserve_type<char, decltype(scalar)>, scalar);
}

template <chrono_opcode opcode, chrono_padding padding,
		  ::fast_io::fmt::format_character char_type>
inline char_type *emit_chrono_calendar_operation(
	char_type *output, chrono_calendar_state const &state)
{
	auto const &time{state.value};
	if constexpr (opcode == chrono_opcode::abbreviated_weekday)
	{
		return write_chrono_weekday_name(output, chrono_tm_weekday(time), false);
	}
	else if constexpr (opcode == chrono_opcode::full_weekday)
	{
		return write_chrono_weekday_name(output, chrono_tm_weekday(time), true);
	}
	else if constexpr (opcode == chrono_opcode::abbreviated_month)
	{
		return write_chrono_month_name(output, chrono_tm_month(time), false);
	}
	else if constexpr (opcode == chrono_opcode::full_month)
	{
		return write_chrono_month_name(output, chrono_tm_month(time), true);
	}
	else if constexpr (opcode == chrono_opcode::century)
	{
		return write_chrono_padded_integer(output,
										   chrono_floor_century(chrono_tm_year(time)), 2u,
										   chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::day_of_month)
	{
		return write_chrono_padded_integer(
			output, chrono_tm_month_day(time), 2u, padding);
	}
	else if constexpr (opcode == chrono_opcode::space_day_of_month)
	{
		return write_chrono_padded_integer(output, chrono_tm_month_day(time), 2u,
										   padding == chrono_padding::zero ? chrono_padding::space : padding);
	}
	else if constexpr (opcode == chrono_opcode::iso_week_based_short_year)
	{
		auto const iso{chrono_make_iso_week_fields(time)};
		return write_chrono_padded_integer(
			output, chrono_short_year(iso.year), 2u, chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::iso_week_based_year)
	{
		return write_chrono_padded_integer(
			output, chrono_make_iso_week_fields(time).year, 4u,
			chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::hour_24)
	{
		return write_chrono_padded_integer(
			output, chrono_tm_hour(time), 2u, padding);
	}
	else if constexpr (opcode == chrono_opcode::hour_12)
	{
		auto hour{chrono_tm_hour(time) % 12u};
		if (hour == 0u)
		{
			hour = 12u;
		}
		return write_chrono_padded_integer(output, hour, 2u, padding);
	}
	else if constexpr (opcode == chrono_opcode::day_of_year)
	{
		return write_chrono_padded_integer(
			output, chrono_tm_year_day(time) + 1u, 3u, padding);
	}
	else if constexpr (opcode == chrono_opcode::month)
	{
		return write_chrono_padded_integer(
			output, chrono_tm_month(time), 2u, padding);
	}
	else if constexpr (opcode == chrono_opcode::minute)
	{
		return write_chrono_padded_integer(
			output, chrono_tm_minute(time), 2u, padding);
	}
	else if constexpr (opcode == chrono_opcode::am_pm)
	{
		return write_chrono_ascii(
			output, chrono_tm_hour(time) < 12u ? u8"AM" : u8"PM");
	}
	else if constexpr (opcode == chrono_opcode::second)
	{
		return write_chrono_calendar_second(output, state, padding);
	}
	else if constexpr (opcode == chrono_opcode::iso_weekday)
	{
		auto const weekday{chrono_tm_weekday(time)};
		return write_chrono_integer(output, weekday == 0u ? 7u : weekday);
	}
	else if constexpr (opcode == chrono_opcode::sunday_weekday)
	{
		return write_chrono_integer(output, chrono_tm_weekday(time));
	}
	else if constexpr (opcode == chrono_opcode::sunday_week)
	{
		auto const year_day{chrono_tm_year_day(time)};
		auto const weekday{chrono_tm_weekday(time)};
		auto const week{(year_day + 7u - weekday) / 7u};
		return write_chrono_padded_integer(output, week, 2u, padding);
	}
	else if constexpr (opcode == chrono_opcode::monday_week)
	{
		auto const year_day{chrono_tm_year_day(time)};
		auto const monday_based{(chrono_tm_weekday(time) + 6u) % 7u};
		auto const week{(year_day + 7u - monday_based) / 7u};
		return write_chrono_padded_integer(output, week, 2u, padding);
	}
	else if constexpr (opcode == chrono_opcode::iso_week)
	{
		return write_chrono_padded_integer(
			output, chrono_make_iso_week_fields(time).week, 2u, padding);
	}
	else if constexpr (opcode == chrono_opcode::short_year)
	{
		return write_chrono_padded_integer(output,
										   chrono_short_year(chrono_tm_year(time)), 2u,
										   chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::year)
	{
		return write_chrono_padded_integer(
			output, chrono_tm_year(time), 4u, padding);
	}
	else if constexpr (opcode == chrono_opcode::us_date || opcode == chrono_opcode::locale_date)
	{
		output = write_chrono_padded_integer(
			output, chrono_tm_month(time), 2u, chrono_padding::zero);
		output = write_chrono_separator(output, u8'/');
		output = write_chrono_padded_integer(
			output, chrono_tm_month_day(time), 2u, chrono_padding::zero);
		output = write_chrono_separator(output, u8'/');
		return write_chrono_padded_integer(output,
										   chrono_short_year(chrono_tm_year(time)), 2u,
										   chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::iso_date)
	{
		output = write_chrono_padded_integer(
			output, chrono_tm_year(time), 4u, chrono_padding::zero);
		output = write_chrono_separator(output, u8'-');
		output = write_chrono_padded_integer(
			output, chrono_tm_month(time), 2u, chrono_padding::zero);
		output = write_chrono_separator(output, u8'-');
		return write_chrono_padded_integer(
			output, chrono_tm_month_day(time), 2u, chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::time_hm)
	{
		output = write_chrono_padded_integer(
			output, chrono_tm_hour(time), 2u, chrono_padding::zero);
		output = write_chrono_separator(output, u8':');
		return write_chrono_padded_integer(
			output, chrono_tm_minute(time), 2u, chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::time_hms || opcode == chrono_opcode::locale_time)
	{
		output = write_chrono_padded_integer(
			output, chrono_tm_hour(time), 2u, chrono_padding::zero);
		output = write_chrono_separator(output, u8':');
		output = write_chrono_padded_integer(
			output, chrono_tm_minute(time), 2u, chrono_padding::zero);
		output = write_chrono_separator(output, u8':');
		return write_chrono_calendar_second(output, state, chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::time_12)
	{
		auto const hour_24{chrono_tm_hour(time)};
		auto hour{hour_24 % 12u};
		if (hour == 0u)
		{
			hour = 12u;
		}
		output = write_chrono_padded_integer(output, hour, 2u, chrono_padding::zero);
		output = write_chrono_separator(output, u8':');
		output = write_chrono_padded_integer(
			output, chrono_tm_minute(time), 2u, chrono_padding::zero);
		output = write_chrono_separator(output, u8':');
		output = write_chrono_calendar_second(output, state, chrono_padding::zero);
		output = write_chrono_separator(output, u8' ');
		return write_chrono_ascii(output, hour_24 < 12u ? u8"AM" : u8"PM");
	}
	else if constexpr (opcode == chrono_opcode::date_time)
	{
		output = write_chrono_weekday_name(
			output, chrono_tm_weekday(time), false);
		output = write_chrono_separator(output, u8' ');
		output = write_chrono_month_name(output, chrono_tm_month(time), false);
		output = write_chrono_separator(output, u8' ');
		output = write_chrono_padded_integer(
			output, chrono_tm_month_day(time), 2u, chrono_padding::space);
		output = write_chrono_separator(output, u8' ');
		output = emit_chrono_calendar_operation<chrono_opcode::time_hms,
												chrono_padding::zero>(output, state);
		output = write_chrono_separator(output, u8' ');
		return write_chrono_padded_integer(
			output, chrono_tm_year(time), 4u, chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::default_date_time)
	{
		output = emit_chrono_calendar_operation<chrono_opcode::iso_date,
												chrono_padding::zero>(output, state);
		output = write_chrono_separator(output, u8' ');
		return emit_chrono_calendar_operation<chrono_opcode::time_hms,
											  chrono_padding::zero>(output, state);
	}
	else if constexpr (opcode == chrono_opcode::utc_offset)
	{
		if (!state.utc)
		{
			::fast_io::fast_terminate();
		}
		return write_chrono_ascii(output, u8"+0000");
	}
	else if constexpr (opcode == chrono_opcode::time_zone_name)
	{
		if (!state.utc)
		{
			::fast_io::fast_terminate();
		}
		return write_chrono_ascii(output, u8"UTC");
	}
	else
	{
		static_assert(opcode == chrono_opcode::literal || opcode == chrono_opcode::percent ||
						  opcode == chrono_opcode::newline || opcode == chrono_opcode::tab,
					  "fast_io chrono: unhandled calendar opcode");
		return output;
	}
}

/**
 * Validate exactly the `std::tm` members read by one compiled conversion.
 *
 * Reserve sizing runs before emission for dynamically sized printables.  An
 * unconditional civil-date conversion here would make the nominally harmless
 * size pass stricter than the selected opcode.  This compile-time dispatch is
 * intentionally parallel to the emitter: discarded branches neither read nor
 * validate unrelated tuple members.
 */
template <chrono_opcode opcode>
inline void validate_chrono_calendar_operation_fields(::std::tm const &time) noexcept
{
	if constexpr (opcode == chrono_opcode::abbreviated_weekday ||
				  opcode == chrono_opcode::full_weekday ||
				  opcode == chrono_opcode::iso_weekday ||
				  opcode == chrono_opcode::sunday_weekday)
	{
		(void)chrono_tm_weekday(time);
	}
	else if constexpr (opcode == chrono_opcode::abbreviated_month ||
					   opcode == chrono_opcode::full_month ||
					   opcode == chrono_opcode::month)
	{
		(void)chrono_tm_month(time);
	}
	else if constexpr (opcode == chrono_opcode::century ||
					   opcode == chrono_opcode::short_year ||
					   opcode == chrono_opcode::year)
	{
		(void)chrono_tm_year(time);
	}
	else if constexpr (opcode == chrono_opcode::day_of_month ||
					   opcode == chrono_opcode::space_day_of_month)
	{
		(void)chrono_tm_month_day(time);
	}
	else if constexpr (opcode == chrono_opcode::iso_week_based_short_year ||
					   opcode == chrono_opcode::iso_week_based_year ||
					   opcode == chrono_opcode::iso_week)
	{
		(void)chrono_make_iso_week_fields(time);
	}
	else if constexpr (opcode == chrono_opcode::hour_24 ||
					   opcode == chrono_opcode::hour_12 ||
					   opcode == chrono_opcode::am_pm)
	{
		(void)chrono_tm_hour(time);
	}
	else if constexpr (opcode == chrono_opcode::day_of_year)
	{
		(void)chrono_tm_year_day(time);
	}
	else if constexpr (opcode == chrono_opcode::minute)
	{
		(void)chrono_tm_minute(time);
	}
	else if constexpr (opcode == chrono_opcode::second)
	{
		(void)chrono_tm_second(time);
	}
	else if constexpr (opcode == chrono_opcode::sunday_week ||
					   opcode == chrono_opcode::monday_week)
	{
		(void)chrono_tm_year_day(time);
		(void)chrono_tm_weekday(time);
	}
	else if constexpr (opcode == chrono_opcode::us_date ||
					   opcode == chrono_opcode::locale_date ||
					   opcode == chrono_opcode::iso_date)
	{
		(void)chrono_tm_year(time);
		(void)chrono_tm_month(time);
		(void)chrono_tm_month_day(time);
	}
	else if constexpr (opcode == chrono_opcode::time_hm)
	{
		(void)chrono_tm_hour(time);
		(void)chrono_tm_minute(time);
	}
	else if constexpr (opcode == chrono_opcode::time_hms ||
					   opcode == chrono_opcode::locale_time ||
					   opcode == chrono_opcode::time_12)
	{
		(void)chrono_tm_hour(time);
		(void)chrono_tm_minute(time);
		(void)chrono_tm_second(time);
	}
	else if constexpr (opcode == chrono_opcode::date_time)
	{
		(void)chrono_tm_weekday(time);
		(void)chrono_tm_month(time);
		(void)chrono_tm_month_day(time);
		(void)chrono_tm_hour(time);
		(void)chrono_tm_minute(time);
		(void)chrono_tm_second(time);
		(void)chrono_tm_year(time);
	}
	else if constexpr (opcode == chrono_opcode::default_date_time)
	{
		(void)chrono_tm_year(time);
		(void)chrono_tm_month(time);
		(void)chrono_tm_month_day(time);
		(void)chrono_tm_hour(time);
		(void)chrono_tm_minute(time);
		(void)chrono_tm_second(time);
	}
}

template <chrono_opcode opcode, chrono_padding padding>
[[nodiscard]] inline ::std::size_t chrono_calendar_operation_capacity(
	chrono_calendar_state const &state)
{
	validate_chrono_calendar_operation_fields<opcode>(state.value);
	if constexpr (opcode == chrono_opcode::abbreviated_weekday)
	{
		return 3u;
	}
	else if constexpr (opcode == chrono_opcode::full_weekday)
	{
		return chrono_weekday_name_size(chrono_tm_weekday(state.value), true);
	}
	else if constexpr (opcode == chrono_opcode::abbreviated_month)
	{
		return 3u;
	}
	else if constexpr (opcode == chrono_opcode::full_month)
	{
		return chrono_month_name_size(chrono_tm_month(state.value), true);
	}
	else if constexpr (opcode == chrono_opcode::second)
	{
		return chrono_calendar_second_capacity(state, padding);
	}
	else if constexpr (opcode == chrono_opcode::date_time)
	{
		return 3u + 1u + 3u + 1u + 2u + 1u + 8u + state.fractional_precision +
			   static_cast<::std::size_t>(state.fractional_precision != 0u) + 1u + 12u;
	}
	else if constexpr (opcode == chrono_opcode::default_date_time)
	{
		return 12u + 1u + 8u + state.fractional_precision +
			   static_cast<::std::size_t>(state.fractional_precision != 0u);
	}
	else if constexpr (opcode == chrono_opcode::us_date || opcode == chrono_opcode::locale_date)
	{
		return 8u;
	}
	else if constexpr (opcode == chrono_opcode::iso_date)
	{
		return 18u;
	}
	else if constexpr (opcode == chrono_opcode::time_hm)
	{
		return 5u;
	}
	else if constexpr (opcode == chrono_opcode::time_hms || opcode == chrono_opcode::locale_time)
	{
		return 8u + state.fractional_precision +
			   static_cast<::std::size_t>(state.fractional_precision != 0u);
	}
	else if constexpr (opcode == chrono_opcode::time_12)
	{
		return 11u + state.fractional_precision +
			   static_cast<::std::size_t>(state.fractional_precision != 0u);
	}
	else if constexpr (opcode == chrono_opcode::utc_offset)
	{
		return 5u;
	}
	else if constexpr (opcode == chrono_opcode::time_zone_name)
	{
		return 3u;
	}
	else if constexpr (opcode == chrono_opcode::year || opcode == chrono_opcode::iso_week_based_year)
	{
		return 12u;
	}
	else if constexpr (opcode == chrono_opcode::century)
	{
		return 12u;
	}
	else if constexpr (opcode == chrono_opcode::day_of_year)
	{
		return 3u;
	}
	else
	{
		return 2u;
	}
}

} // namespace fast_io::fmt::details

namespace fast_io::fmt::details
{

template <typename T>
struct is_chrono_duration : ::std::false_type
{};

template <typename rep_type, typename period_type>
struct is_chrono_duration<::std::chrono::duration<rep_type, period_type>> : ::std::true_type
{};

template <typename T>
inline constexpr bool is_chrono_duration_v{
	is_chrono_duration<::std::remove_cvref_t<T>>::value};

template <typename T>
struct is_system_time_point : ::std::false_type
{};

template <typename duration_type>
struct is_system_time_point<
	::std::chrono::time_point<::std::chrono::system_clock, duration_type>> : ::std::true_type
{};

template <typename T>
inline constexpr bool is_system_time_point_v{
	is_system_time_point<::std::remove_cvref_t<T>>::value};

/**
 * Calendar state normalized at the format boundary.
 *
 * `system_clock` conversion is intentionally performed before the object enters
 * fast_io's reserve protocol.  A dynamically sized printable is normally asked
 * for a bound and then emitted, and doing the conversion inside both CPOs would
 * call `gmtime` twice.  Storing one `tm` also prevents a C implementation's
 * internal static `tm` storage from escaping into the print layer.
 */
template <typename period_type>
[[nodiscard]] consteval unsigned chrono_fractional_digits() noexcept
{
	// Reduce the period's denominator by the numerator: only the reduced
	// denominator determines whether one tick has a terminating decimal spelling.
	::std::uintmax_t numerator{
		static_cast<::std::uintmax_t>(period_type::num < 0 ? -period_type::num : period_type::num)};
	::std::uintmax_t denominator{static_cast<::std::uintmax_t>(period_type::den)};
	auto gcd = [](auto lhs, auto rhs) constexpr {
		while (rhs != 0u)
		{
			auto const remainder{lhs % rhs};
			lhs = rhs;
			rhs = remainder;
		}
		return lhs;
	};
	auto const common{gcd(numerator, denominator)};
	denominator /= common;
	unsigned twos{};
	unsigned fives{};
	while ((denominator % 2u) == 0u)
	{
		denominator /= 2u;
		++twos;
	}
	while ((denominator % 5u) == 0u)
	{
		denominator /= 5u;
		++fives;
	}
	if (denominator != 1u)
	{
		return 6u;
	}
	auto const digits{twos < fives ? fives : twos};
	return digits < 18u ? digits : 18u;
}

[[nodiscard]] inline chrono_calendar_state make_chrono_calendar_state(::std::tm value) noexcept
{
	return {value, 0.0L, 0u, false};
}

template <typename duration_type>
[[nodiscard]] inline chrono_calendar_state make_chrono_calendar_state(
	::std::chrono::time_point<::std::chrono::system_clock, duration_type> value)
{
	using namespace ::std::chrono;
	auto const epoch{value.time_since_epoch()};
	auto const whole_seconds{floor<seconds>(epoch)};
	auto const fraction{epoch - whole_seconds};
	auto const normalized{::std::chrono::time_point<::std::chrono::system_clock,
													::std::chrono::system_clock::duration>{
		duration_cast<::std::chrono::system_clock::duration>(whole_seconds)}};
	auto const c_time{::std::chrono::system_clock::to_time_t(normalized)};

	::std::tm result{};
#if defined(_WIN32) && !defined(__CYGWIN__)
	if (::_gmtime64_s(&result, &c_time) != 0)
	{
		::fast_io::fast_terminate();
	}
#elif defined(__unix__) || defined(__APPLE__) || defined(__MACH__)
	if (::gmtime_r(&c_time, &result) == nullptr)
	{
		::fast_io::fast_terminate();
	}
#else
	auto const pointer{::std::gmtime(&c_time)};
	if (pointer == nullptr)
	{
		::fast_io::fast_terminate();
	}
	result = *pointer;
#endif

	long double const fractional_seconds{
		duration<long double>(fraction).count()};
	return {result, fractional_seconds,
			chrono_fractional_digits<typename duration_type::period>(), true};
}

template <typename T>
concept chrono_format_value = is_chrono_duration_v<T> ||
							  ::std::same_as<::std::remove_cvref_t<T>, ::std::tm> || is_system_time_point_v<T>;

template <typename T>
inline constexpr chrono_value_domain chrono_domain_v =
	is_chrono_duration_v<T> ? chrono_value_domain::duration : chrono_value_domain::calendar;

/** Converts an invariant UTF-8/ASCII code unit to the selected execution character type. */
template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr char_type chrono_basic_latin(char8_t value) noexcept
{
	// A cast from an ASCII integer is not an execution-character conversion on
	// EBCDIC.  The explicit table is verbose by design and is folded completely
	// for every literal/name call site.
	switch (value)
	{
	case u8'0':
		return ::fast_io::arithmetic_char_literal_v<u8'0', char_type>;
	case u8'1':
		return ::fast_io::arithmetic_char_literal_v<u8'1', char_type>;
	case u8'2':
		return ::fast_io::arithmetic_char_literal_v<u8'2', char_type>;
	case u8'3':
		return ::fast_io::arithmetic_char_literal_v<u8'3', char_type>;
	case u8'4':
		return ::fast_io::arithmetic_char_literal_v<u8'4', char_type>;
	case u8'5':
		return ::fast_io::arithmetic_char_literal_v<u8'5', char_type>;
	case u8'6':
		return ::fast_io::arithmetic_char_literal_v<u8'6', char_type>;
	case u8'7':
		return ::fast_io::arithmetic_char_literal_v<u8'7', char_type>;
	case u8'8':
		return ::fast_io::arithmetic_char_literal_v<u8'8', char_type>;
	case u8'9':
		return ::fast_io::arithmetic_char_literal_v<u8'9', char_type>;
	case u8'A':
		return ::fast_io::arithmetic_char_literal_v<u8'A', char_type>;
	case u8'B':
		return ::fast_io::arithmetic_char_literal_v<u8'B', char_type>;
	case u8'C':
		return ::fast_io::arithmetic_char_literal_v<u8'C', char_type>;
	case u8'D':
		return ::fast_io::arithmetic_char_literal_v<u8'D', char_type>;
	case u8'E':
		return ::fast_io::arithmetic_char_literal_v<u8'E', char_type>;
	case u8'F':
		return ::fast_io::arithmetic_char_literal_v<u8'F', char_type>;
	case u8'G':
		return ::fast_io::arithmetic_char_literal_v<u8'G', char_type>;
	case u8'H':
		return ::fast_io::arithmetic_char_literal_v<u8'H', char_type>;
	case u8'I':
		return ::fast_io::arithmetic_char_literal_v<u8'I', char_type>;
	case u8'J':
		return ::fast_io::arithmetic_char_literal_v<u8'J', char_type>;
	case u8'K':
		return ::fast_io::arithmetic_char_literal_v<u8'K', char_type>;
	case u8'L':
		return ::fast_io::arithmetic_char_literal_v<u8'L', char_type>;
	case u8'M':
		return ::fast_io::arithmetic_char_literal_v<u8'M', char_type>;
	case u8'N':
		return ::fast_io::arithmetic_char_literal_v<u8'N', char_type>;
	case u8'O':
		return ::fast_io::arithmetic_char_literal_v<u8'O', char_type>;
	case u8'P':
		return ::fast_io::arithmetic_char_literal_v<u8'P', char_type>;
	case u8'Q':
		return ::fast_io::arithmetic_char_literal_v<u8'Q', char_type>;
	case u8'R':
		return ::fast_io::arithmetic_char_literal_v<u8'R', char_type>;
	case u8'S':
		return ::fast_io::arithmetic_char_literal_v<u8'S', char_type>;
	case u8'T':
		return ::fast_io::arithmetic_char_literal_v<u8'T', char_type>;
	case u8'U':
		return ::fast_io::arithmetic_char_literal_v<u8'U', char_type>;
	case u8'V':
		return ::fast_io::arithmetic_char_literal_v<u8'V', char_type>;
	case u8'W':
		return ::fast_io::arithmetic_char_literal_v<u8'W', char_type>;
	case u8'X':
		return ::fast_io::arithmetic_char_literal_v<u8'X', char_type>;
	case u8'Y':
		return ::fast_io::arithmetic_char_literal_v<u8'Y', char_type>;
	case u8'Z':
		return ::fast_io::arithmetic_char_literal_v<u8'Z', char_type>;
	case u8'a':
		return ::fast_io::arithmetic_char_literal_v<u8'a', char_type>;
	case u8'b':
		return ::fast_io::arithmetic_char_literal_v<u8'b', char_type>;
	case u8'c':
		return ::fast_io::arithmetic_char_literal_v<u8'c', char_type>;
	case u8'd':
		return ::fast_io::arithmetic_char_literal_v<u8'd', char_type>;
	case u8'e':
		return ::fast_io::arithmetic_char_literal_v<u8'e', char_type>;
	case u8'f':
		return ::fast_io::arithmetic_char_literal_v<u8'f', char_type>;
	case u8'g':
		return ::fast_io::arithmetic_char_literal_v<u8'g', char_type>;
	case u8'h':
		return ::fast_io::arithmetic_char_literal_v<u8'h', char_type>;
	case u8'i':
		return ::fast_io::arithmetic_char_literal_v<u8'i', char_type>;
	case u8'j':
		return ::fast_io::arithmetic_char_literal_v<u8'j', char_type>;
	case u8'k':
		return ::fast_io::arithmetic_char_literal_v<u8'k', char_type>;
	case u8'l':
		return ::fast_io::arithmetic_char_literal_v<u8'l', char_type>;
	case u8'm':
		return ::fast_io::arithmetic_char_literal_v<u8'm', char_type>;
	case u8'n':
		return ::fast_io::arithmetic_char_literal_v<u8'n', char_type>;
	case u8'o':
		return ::fast_io::arithmetic_char_literal_v<u8'o', char_type>;
	case u8'p':
		return ::fast_io::arithmetic_char_literal_v<u8'p', char_type>;
	case u8'q':
		return ::fast_io::arithmetic_char_literal_v<u8'q', char_type>;
	case u8'r':
		return ::fast_io::arithmetic_char_literal_v<u8'r', char_type>;
	case u8's':
		return ::fast_io::arithmetic_char_literal_v<u8's', char_type>;
	case u8't':
		return ::fast_io::arithmetic_char_literal_v<u8't', char_type>;
	case u8'u':
		return ::fast_io::arithmetic_char_literal_v<u8'u', char_type>;
	case u8'v':
		return ::fast_io::arithmetic_char_literal_v<u8'v', char_type>;
	case u8'w':
		return ::fast_io::arithmetic_char_literal_v<u8'w', char_type>;
	case u8'x':
		return ::fast_io::arithmetic_char_literal_v<u8'x', char_type>;
	case u8'y':
		return ::fast_io::arithmetic_char_literal_v<u8'y', char_type>;
	case u8'z':
		return ::fast_io::arithmetic_char_literal_v<u8'z', char_type>;
	case u8' ':
		return ::fast_io::arithmetic_char_literal_v<u8' ', char_type>;
	case u8'-':
		return ::fast_io::arithmetic_char_literal_v<u8'-', char_type>;
	case u8'+':
		return ::fast_io::arithmetic_char_literal_v<u8'+', char_type>;
	case u8':':
		return ::fast_io::arithmetic_char_literal_v<u8':', char_type>;
	case u8'/':
		return ::fast_io::arithmetic_char_literal_v<u8'/', char_type>;
	case u8'[':
		return ::fast_io::arithmetic_char_literal_v<u8'[', char_type>;
	case u8']':
		return ::fast_io::arithmetic_char_literal_v<u8']', char_type>;
	case u8'.':
		return ::fast_io::arithmetic_char_literal_v<u8'.', char_type>;
	case u8'%':
		return ::fast_io::arithmetic_char_literal_v<u8'%', char_type>;
	case u8'\n':
		return ::fast_io::arithmetic_char_literal_v<u8'\n', char_type>;
	case u8'\t':
		return ::fast_io::arithmetic_char_literal_v<u8'\t', char_type>;
	default:
		return char_type{};
	}
}

template <::fast_io::fmt::format_character char_type, ::std::size_t extent>
inline constexpr char_type *write_chrono_ascii(
	char_type *output, char8_t const (&text)[extent]) noexcept
{
	for (::std::size_t index{}; index + 1u != extent; ++index)
	{
		*output++ = chrono_basic_latin<char_type>(text[index]);
	}
	return output;
}

template <::std::size_t extent>
inline constexpr ::std::size_t chrono_ascii_size(char8_t const (&)[extent]) noexcept
{
	return extent - 1u;
}

[[nodiscard]] inline constexpr ::std::int64_t chrono_floor_divide(
	::std::int64_t numerator, ::std::int64_t denominator) noexcept
{
	auto quotient{numerator / denominator};
	auto const remainder{numerator % denominator};
	if (remainder != 0 && ((remainder < 0) != (denominator < 0)))
	{
		--quotient;
	}
	return quotient;
}

[[nodiscard]] inline constexpr unsigned chrono_positive_modulo(
	::std::int64_t value, unsigned modulus) noexcept
{
	auto result{value % static_cast<::std::int64_t>(modulus)};
	if (result < 0)
	{
		result += modulus;
	}
	return static_cast<unsigned>(result);
}

/**
 * Read one `std::tm` semantic field without normalizing any of its neighbours.
 *
 * The C calendar structure is a tuple of independently observable fields.  In
 * particular, `%a` is specified by `tm_wday`, not by recomputing a weekday from
 * `tm_year/tm_mon/tm_mday`.  Keeping these checks separate is therefore more
 * than a performance detail: it lets a deliberately partial `tm` format `%j`
 * or `%H` without requiring an unrelated, normalized civil date.
 */
[[nodiscard]] inline int chrono_tm_year(::std::tm const &value) noexcept
{
	auto const year{static_cast<::std::int64_t>(value.tm_year) + 1900};
	if (year < static_cast<::std::int64_t>((::std::numeric_limits<int>::min)()) ||
		year > static_cast<::std::int64_t>((::std::numeric_limits<int>::max)()))
	{
		::fast_io::fast_terminate();
	}
	return static_cast<int>(year);
}

[[nodiscard]] inline unsigned chrono_tm_month(::std::tm const &value) noexcept
{
	if (value.tm_mon < 0 || value.tm_mon > 11)
	{
		::fast_io::fast_terminate();
	}
	return static_cast<unsigned>(value.tm_mon) + 1u;
}

[[nodiscard]] inline unsigned chrono_tm_month_day(::std::tm const &value) noexcept
{
	if (value.tm_mday < 1 || value.tm_mday > 31)
	{
		::fast_io::fast_terminate();
	}
	return static_cast<unsigned>(value.tm_mday);
}

[[nodiscard]] inline unsigned chrono_tm_weekday(::std::tm const &value) noexcept
{
	if (value.tm_wday < 0 || value.tm_wday > 6)
	{
		::fast_io::fast_terminate();
	}
	return static_cast<unsigned>(value.tm_wday);
}

[[nodiscard]] inline unsigned chrono_tm_year_day(::std::tm const &value) noexcept
{
	if (value.tm_yday < 0 || value.tm_yday > 365)
	{
		::fast_io::fast_terminate();
	}
	return static_cast<unsigned>(value.tm_yday);
}

[[nodiscard]] inline unsigned chrono_tm_hour(::std::tm const &value) noexcept
{
	if (value.tm_hour < 0 || value.tm_hour > 23)
	{
		::fast_io::fast_terminate();
	}
	return static_cast<unsigned>(value.tm_hour);
}

[[nodiscard]] inline unsigned chrono_tm_minute(::std::tm const &value) noexcept
{
	if (value.tm_min < 0 || value.tm_min > 59)
	{
		::fast_io::fast_terminate();
	}
	return static_cast<unsigned>(value.tm_min);
}

[[nodiscard]] inline unsigned chrono_tm_second(::std::tm const &value) noexcept
{
	// C and POSIX reserve 60 for a positive leap second.
	if (value.tm_sec < 0 || value.tm_sec > 60)
	{
		::fast_io::fast_terminate();
	}
	return static_cast<unsigned>(value.tm_sec);
}

[[nodiscard]] inline constexpr bool chrono_is_gregorian_leap_year(int year) noexcept
{
	return (year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0);
}

[[nodiscard]] inline constexpr unsigned chrono_iso_weeks_in_year(
	int year, unsigned january_first_iso_weekday) noexcept
{
	// An ISO year has week 53 exactly when January 1 is Thursday, or when a
	// leap year starts on Wednesday.
	return january_first_iso_weekday == 4u ||
				   (january_first_iso_weekday == 3u &&
					chrono_is_gregorian_leap_year(year))
			   ? 53u
			   : 52u;
}

[[nodiscard]] inline chrono_iso_week_fields chrono_make_iso_week_fields(
	::std::tm const &value) noexcept
{
	auto const year{chrono_tm_year(value)};
	auto const year_day{chrono_tm_year_day(value)};
	auto const weekday{chrono_tm_weekday(value)};
	auto const days_in_year{chrono_is_gregorian_leap_year(year) ? 366u : 365u};
	if (year_day >= days_in_year)
	{
		::fast_io::fast_terminate();
	}

	auto const iso_weekday{weekday == 0u ? 7u : weekday};
	// Moving backwards `tm_yday` days from the supplied weekday proves the
	// weekday of January 1 without consulting tm_mon or tm_mday.
	auto const january_first_sunday_weekday{chrono_positive_modulo(
		static_cast<::std::int64_t>(weekday) -
			static_cast<::std::int64_t>(year_day % 7u),
		7u)};
	auto const january_first_iso_weekday{
		january_first_sunday_weekday == 0u ? 7u : january_first_sunday_weekday};
	auto const weeks_in_year{
		chrono_iso_weeks_in_year(year, january_first_iso_weekday)};

	// For a zero-based ordinal day d and ISO weekday w, ISO 8601's Thursday
	// rule reduces to floor((d + 10 - w) / 7).  The result can only cross the
	// adjacent ISO year at the two branches below.
	auto const candidate_week{static_cast<unsigned>(
		(static_cast<int>(year_day) + 10 - static_cast<int>(iso_weekday)) / 7)};
	if (candidate_week == 0u)
	{
		if (year == (::std::numeric_limits<int>::min)())
		{
			::fast_io::fast_terminate();
		}
		auto const previous_year{year - 1};
		auto const previous_year_shift{
			chrono_is_gregorian_leap_year(previous_year) ? 2u : 1u};
		auto const previous_january_first_sunday_weekday{
			chrono_positive_modulo(
				static_cast<::std::int64_t>(january_first_sunday_weekday) -
					static_cast<::std::int64_t>(previous_year_shift),
				7u)};
		auto const previous_january_first_iso_weekday{
			previous_january_first_sunday_weekday == 0u
				? 7u
				: previous_january_first_sunday_weekday};
		return {previous_year, chrono_iso_weeks_in_year(
								   previous_year, previous_january_first_iso_weekday)};
	}
	if (candidate_week > weeks_in_year)
	{
		if (year == (::std::numeric_limits<int>::max)())
		{
			::fast_io::fast_terminate();
		}
		return {year + 1, 1u};
	}
	return {year, candidate_week};
}

template <typename unsigned_type>
[[nodiscard]] inline constexpr ::std::size_t chrono_decimal_digits(unsigned_type value) noexcept
{
	::std::size_t digits{1u};
	while (value >= static_cast<unsigned_type>(10u))
	{
		value /= static_cast<unsigned_type>(10u);
		++digits;
	}
	return digits;
}

template <typename signed_type>
[[nodiscard]] inline constexpr auto chrono_unsigned_magnitude(signed_type value) noexcept
{
	if constexpr (::std::is_signed_v<signed_type>)
	{
		using unsigned_type = ::std::make_unsigned_t<signed_type>;
		auto const converted{static_cast<unsigned_type>(value)};
		return value < 0 ? static_cast<unsigned_type>(0u - converted) : converted;
	}
	else
	{
		return value;
	}
}

template <::fast_io::fmt::format_character char_type, typename integer_type>
inline constexpr char_type *write_chrono_integer(
	char_type *output, integer_type value) noexcept
{
	return print_reserve_define(
		::fast_io::io_reserve_type<char_type, integer_type>, output, value);
}

template <typename integer_type>
[[nodiscard]] inline constexpr ::std::size_t chrono_integer_size(integer_type value) noexcept
{
	auto const magnitude{chrono_unsigned_magnitude(value)};
	return chrono_decimal_digits(magnitude) +
		   static_cast<::std::size_t>(::std::is_signed_v<integer_type> && value < 0);
}

template <::fast_io::fmt::format_character char_type, typename integer_type>
inline constexpr char_type *write_chrono_padded_integer(
	char_type *output, integer_type value, unsigned width, chrono_padding padding) noexcept
{
	auto const magnitude{chrono_unsigned_magnitude(value)};
	auto const digits{chrono_decimal_digits(magnitude)};
	bool const negative{::std::is_signed_v<integer_type> && value < 0};
	if (negative)
	{
		*output++ = chrono_basic_latin<char_type>(u8'-');
	}
	if (padding != chrono_padding::none && digits < width)
	{
		auto const fill{padding == chrono_padding::space ? u8' ' : u8'0'};
		for (auto count{width - static_cast<unsigned>(digits)}; count != 0u; --count)
		{
			*output++ = chrono_basic_latin<char_type>(fill);
		}
	}
	return write_chrono_integer(output, magnitude);
}

template <typename integer_type>
[[nodiscard]] inline constexpr ::std::size_t chrono_padded_integer_size(
	integer_type value, unsigned width, chrono_padding padding) noexcept
{
	auto const digits{chrono_decimal_digits(chrono_unsigned_magnitude(value))};
	return static_cast<::std::size_t>(::std::is_signed_v<integer_type> && value < 0) +
		   (padding == chrono_padding::none || digits >= width ? digits : width);
}

} // namespace fast_io::fmt::details

namespace fast_io::manipulators
{

/**
 * Printable semantic leaf produced by the brace chrono lowering.
 *
 * The format literal and chrono source slice are type properties.  The object
 * itself stores only normalized value state and, when the generic brace prefix
 * requested it, a resolved precision.  Consequently neither a format pointer
 * nor a token-program address crosses the public print/concat boundary.
 */
template <::fast_io::fmt::basic_fixed_string format_literal,
		  ::fast_io::fmt::details::source_slice specification,
		  bool has_precision, typename storage_type>
struct basic_chrono_field_t
{
	using manip_tag = ::fast_io::manip_tag_t;
	using value_type = storage_type;

	storage_type value;
	::std::size_t precision{};
};

} // namespace fast_io::manipulators

namespace fast_io::fmt::details
{

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
	return ::fast_io::manipulators::basic_chrono_field_t<
		format_literal, specification, false, chrono_calendar_state>{
		make_chrono_calendar_state(value), precision};
}

template <::fast_io::fmt::basic_fixed_string format_literal,
		  source_slice specification, bool has_precision, typename duration_type>
[[nodiscard]] inline auto make_chrono_field(
	::std::chrono::time_point<::std::chrono::system_clock, duration_type> value,
	::std::size_t precision = 0u)
{
	static_assert(!has_precision,
				  "fast_io chrono: precision is not permitted for a system_clock time point");
	return ::fast_io::manipulators::basic_chrono_field_t<
		format_literal, specification, false, chrono_calendar_state>{
		make_chrono_calendar_state(value), precision};
}

template <typename storage_type>
using chrono_runtime_state_t = ::std::conditional_t<
	is_chrono_duration_v<storage_type>,
	chrono_duration_state<::std::remove_cvref_t<storage_type>>,
	chrono_calendar_state>;

template <typename storage_type>
[[nodiscard]] inline auto make_chrono_runtime_state(storage_type const &value)
{
	if constexpr (is_chrono_duration_v<storage_type>)
	{
		return make_chrono_duration_state(value);
	}
	else
	{
		return value;
	}
}

template <::fast_io::fmt::basic_fixed_string format_literal,
		  source_slice specification, bool has_precision, typename storage_type,
		  ::std::size_t operation_index, ::fast_io::fmt::format_character char_type>
[[nodiscard]] inline ::std::size_t chrono_operation_capacity(
	::fast_io::manipulators::basic_chrono_field_t<
		format_literal, specification, has_precision, storage_type> const &field,
	chrono_runtime_state_t<storage_type> &state)
{
	constexpr auto domain{chrono_domain_v<storage_type>};
	constexpr auto const &program{
		checked_chrono_program<format_literal, specification, domain>};
	constexpr auto operation{program.operations[operation_index]};
	if constexpr (operation.opcode == chrono_opcode::literal)
	{
		return operation.literal.size;
	}
	else if constexpr (operation.opcode == chrono_opcode::percent ||
					   operation.opcode == chrono_opcode::newline ||
					   operation.opcode == chrono_opcode::tab)
	{
		return 1u;
	}
	else if constexpr (domain == chrono_value_domain::duration)
	{
		return chrono_duration_operation_capacity<operation.opcode,
												  operation.padding, has_precision, char_type>(state, field.precision);
	}
	else
	{
		return chrono_calendar_operation_capacity<operation.opcode,
												  operation.padding>(state);
	}
}

template <::fast_io::fmt::basic_fixed_string format_literal,
		  source_slice specification, bool has_precision, typename storage_type,
		  ::std::size_t operation_index, ::fast_io::fmt::format_character char_type>
inline char_type *emit_chrono_operation(
	char_type *output,
	::fast_io::manipulators::basic_chrono_field_t<
		format_literal, specification, has_precision, storage_type> const &field,
	chrono_runtime_state_t<storage_type> &state)
{
	constexpr auto domain{chrono_domain_v<storage_type>};
	constexpr auto const &program{
		checked_chrono_program<format_literal, specification, domain>};
	constexpr auto operation{program.operations[operation_index]};
	if constexpr (operation.opcode == chrono_opcode::literal)
	{
		for (::std::size_t index{}; index != operation.literal.size; ++index)
		{
			*output++ = format_literal[operation.literal.offset + index];
		}
		return output;
	}
	else if constexpr (operation.opcode == chrono_opcode::percent)
	{
		*output++ = chrono_basic_latin<char_type>(u8'%');
		return output;
	}
	else if constexpr (operation.opcode == chrono_opcode::newline)
	{
		*output++ = chrono_basic_latin<char_type>(u8'\n');
		return output;
	}
	else if constexpr (operation.opcode == chrono_opcode::tab)
	{
		*output++ = chrono_basic_latin<char_type>(u8'\t');
		return output;
	}
	else if constexpr (domain == chrono_value_domain::duration)
	{
		return emit_chrono_duration_operation<operation.opcode,
											  operation.padding, has_precision>(output, state, field.precision);
	}
	else
	{
		return emit_chrono_calendar_operation<operation.opcode,
											  operation.padding>(output, state);
	}
}

template <::fast_io::fmt::basic_fixed_string format_literal,
		  source_slice specification, bool has_precision, typename storage_type,
		  ::fast_io::fmt::format_character char_type, ::std::size_t... operation_index>
[[nodiscard]] inline ::std::size_t chrono_program_capacity_impl(
	::fast_io::manipulators::basic_chrono_field_t<
		format_literal, specification, has_precision, storage_type> const &field,
	::std::index_sequence<operation_index...>)
{
	auto state{make_chrono_runtime_state(field.value)};
	::std::size_t result{};
	((result += chrono_operation_capacity<format_literal, specification,
										  has_precision, storage_type, operation_index, char_type>(field, state)),
	 ...);
	return result;
}

template <::fast_io::fmt::basic_fixed_string format_literal,
		  source_slice specification, bool has_precision, typename storage_type,
		  ::fast_io::fmt::format_character char_type, ::std::size_t... operation_index>
inline char_type *emit_chrono_program_impl(
	char_type *output,
	::fast_io::manipulators::basic_chrono_field_t<
		format_literal, specification, has_precision, storage_type> const &field,
	::std::index_sequence<operation_index...>)
{
	auto state{make_chrono_runtime_state(field.value)};
	((output = emit_chrono_operation<format_literal, specification,
									 has_precision, storage_type, operation_index>(output, field, state)),
	 ...);
	return output;
}

} // namespace fast_io::fmt::details

namespace fast_io
{

template <::std::integral char_type,
		  ::fast_io::fmt::basic_fixed_string format_literal,
		  ::fast_io::fmt::details::source_slice specification,
		  bool has_precision, typename storage_type>
	requires ::std::same_as<char_type,
							typename decltype(format_literal)::value_type>
[[nodiscard]] inline ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type,
								 ::fast_io::manipulators::basic_chrono_field_t<
									 format_literal, specification, has_precision, storage_type>>,
	::fast_io::manipulators::basic_chrono_field_t<
		format_literal, specification, has_precision, storage_type> const &field)
{
	constexpr auto domain{
		::fast_io::fmt::details::chrono_domain_v<storage_type>};
	constexpr auto operation_count{
		::fast_io::fmt::details::checked_chrono_program<
			format_literal, specification, domain>
			.operation_count};
	return ::fast_io::fmt::details::chrono_program_capacity_impl<
		format_literal, specification, has_precision, storage_type, char_type>(
		field, ::std::make_index_sequence<operation_count>{});
}

template <::std::integral char_type,
		  ::fast_io::fmt::basic_fixed_string format_literal,
		  ::fast_io::fmt::details::source_slice specification,
		  bool has_precision, typename storage_type>
	requires ::std::same_as<char_type,
							typename decltype(format_literal)::value_type>
inline char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type,
								 ::fast_io::manipulators::basic_chrono_field_t<
									 format_literal, specification, has_precision, storage_type>>,
	char_type *output,
	::fast_io::manipulators::basic_chrono_field_t<
		format_literal, specification, has_precision, storage_type> const &field)
{
	constexpr auto domain{
		::fast_io::fmt::details::chrono_domain_v<storage_type>};
	constexpr auto operation_count{
		::fast_io::fmt::details::checked_chrono_program<
			format_literal, specification, domain>
			.operation_count};
	return ::fast_io::fmt::details::emit_chrono_program_impl<
		format_literal, specification, has_precision, storage_type>(
		output, field, ::std::make_index_sequence<operation_count>{});
}

} // namespace fast_io
