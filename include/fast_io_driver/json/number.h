#pragma once

#include "../../fast_io_freestanding.h"
#include "concepts.h"

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace fast_io::json
{

enum class json_number_token_kind : char unsigned
{
	integer,
	floating
};

template <typename iterator>
struct basic_json_number_token
{
	iterator first{};
	iterator last{};
	json_number_token_kind kind{json_number_token_kind::integer};
	bool negative{};
	bool has_fraction{};
	bool has_exponent{};
};

template <typename iterator>
struct json_number_scan_result
{
	iterator iter{};
	::fast_io::parse_code code{::fast_io::parse_code::invalid};
	basic_json_number_token<iterator> token{};
};

namespace details
{

template <typename char_type>
[[nodiscard]] inline constexpr bool json_number_is_digit(char_type value) noexcept
{
	return value >= ::fast_io::char_literal_v<u8'0', char_type> &&
		   value <= ::fast_io::char_literal_v<u8'9', char_type>;
}

template <typename char_type>
[[nodiscard]] inline constexpr bool json_number_is_delimiter(char_type value) noexcept
{
	return value == ::fast_io::char_literal_v<u8' ', char_type> ||
		   value == ::fast_io::char_literal_v<u8'\t', char_type> ||
		   value == ::fast_io::char_literal_v<u8'\n', char_type> ||
		   value == ::fast_io::char_literal_v<u8'\r', char_type> ||
		   value == ::fast_io::char_literal_v<u8',', char_type> ||
		   value == ::fast_io::char_literal_v<u8']', char_type> ||
		   value == ::fast_io::char_literal_v<u8'}', char_type>;
}

/*
RFC 8259 number recognition is the deterministic automaton

  minus? -> (zero | nonzero digits*) -> (dot digits+)?
		 -> ([eE] sign? digits+)? -> delimiter-or-EOF.

There is exactly one transition out of every state for an accepted code unit:
`zero` cannot transition to another integer digit, and both `dot` and exponent
states require at least one following digit.  Thus every successful path spells
exactly `-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?`, and every string in
that language follows the same unique path.  Requiring a JSON delimiter or EOF
after the accepting state also prevents accepting a valid numeric prefix of an
invalid token such as `1x`.

This recognizer intentionally performs no arithmetic.  Conversion below is
delegated to fast_io's bounded integer and decimal scanners.  Their accumulation
checks compare the current magnitude with `(limit-digit)/10` before the update
`value=value*10+digit`; the inequality is necessary and sufficient for that
update to fit, so neither intermediate multiplication nor addition can wrap.
The float scanner consumes the same exact span and supplies its established
nearest-rounding path.  Assignment is delayed until both conversion and the
end-pointer check succeed, giving every helper a strong no-partial-write rule.
*/
template <::std::integral char_type>
[[nodiscard]] inline constexpr json_number_scan_result<char_type const *>
scan_json_number(char_type const *first, char_type const *last) noexcept
{
	using result_type = json_number_scan_result<char_type const *>;
	result_type result{};
	result.iter = first;
	result.token.first = first;
	result.token.last = first;
	if (first == last)
	{
		result.code = ::fast_io::parse_code::end_of_file;
		return result;
	}

	auto current{first};
	constexpr auto minus{::fast_io::char_literal_v<u8'-', char_type>};
	constexpr auto zero{::fast_io::char_literal_v<u8'0', char_type>};
	constexpr auto one{::fast_io::char_literal_v<u8'1', char_type>};
	constexpr auto nine{::fast_io::char_literal_v<u8'9', char_type>};
	constexpr auto dot{::fast_io::char_literal_v<u8'.', char_type>};
	constexpr auto lower_e{::fast_io::char_literal_v<u8'e', char_type>};
	constexpr auto upper_e{::fast_io::char_literal_v<u8'E', char_type>};
	constexpr auto plus{::fast_io::char_literal_v<u8'+', char_type>};

	if (*current == minus)
	{
		result.token.negative = true;
		++current;
		if (current == last)
		{
			result.iter = current;
			return result;
		}
	}

	if (*current == zero)
	{
		++current;
		if (current != last && json_number_is_digit(*current))
		{
			result.iter = current;
			return result;
		}
	}
	else if (*current >= one && *current <= nine)
	{
		do
		{
			++current;
		} while (current != last && json_number_is_digit(*current));
	}
	else
	{
		result.iter = current;
		return result;
	}

	if (current != last && *current == dot)
	{
		result.token.kind = json_number_token_kind::floating;
		result.token.has_fraction = true;
		++current;
		if (current == last || !json_number_is_digit(*current))
		{
			result.iter = current;
			return result;
		}
		do
		{
			++current;
		} while (current != last && json_number_is_digit(*current));
	}

	if (current != last && (*current == lower_e || *current == upper_e))
	{
		result.token.kind = json_number_token_kind::floating;
		result.token.has_exponent = true;
		++current;
		if (current != last && (*current == plus || *current == minus))
		{
			++current;
		}
		if (current == last || !json_number_is_digit(*current))
		{
			result.iter = current;
			return result;
		}
		do
		{
			++current;
		} while (current != last && json_number_is_digit(*current));
	}

	if (current != last && !json_number_is_delimiter(*current))
	{
		result.iter = current;
		return result;
	}

	result.iter = current;
	result.code = ::fast_io::parse_code::ok;
	result.token.last = current;
	return result;
}

template <::fast_io::details::my_integral integer_type, ::std::integral char_type>
	requires(!::std::same_as<::std::remove_cv_t<integer_type>, bool>)
[[nodiscard]] inline constexpr ::fast_io::parse_code
parse_json_integer(basic_json_number_token<char_type const *> const &token,
				   integer_type &destination) noexcept
{
	if (token.kind != json_number_token_kind::integer || token.first == token.last)
	{
		return ::fast_io::parse_code::invalid;
	}
	integer_type temporary{};
	/*
	The JSON automaton has already proved that the complete span is one base-10
	integer with no leading '+', radix prefix, separator, or whitespace.  Enter
	the bounded decimal kernel directly instead of rebuilding those policies in
	`from_chars` (including its run-time base argument and error-code adapter).
	*/
	auto const conversion{
		::fast_io::details::scan_int_contiguous_none_space_part_define_impl<
			10, false, false, false, false>(token.first, token.last, temporary)};
	if (conversion.code != ::fast_io::parse_code::ok)
	{
		return conversion.code;
	}
	if (conversion.iter != token.last)
	{
		return ::fast_io::parse_code::invalid;
	}
	destination = temporary;
	return ::fast_io::parse_code::ok;
}

template <::fast_io::details::my_floating_point floating_type, ::std::integral char_type>
[[nodiscard]] inline constexpr ::fast_io::parse_code
parse_json_floating(basic_json_number_token<char_type const *> const &token,
					floating_type &destination) noexcept
{
	if (token.first == token.last)
	{
		return ::fast_io::parse_code::invalid;
	}
	floating_type temporary{};
	/*
	Calling the no-whitespace decimal implementation is sound here even though
	the public default flags have `noskipws == false`: that flag is consumed only
	by `scan_decfloat_contiguous_define`, the wrapper deliberately bypassed
	here.  The RFC automaton supplies an exact nonempty decimal token and excludes
	leading '+', hexadecimal and special values before this conversion starts.
	*/
	auto const conversion{
		::fast_io::details::scan_decfloat_contiguous_define_impl<
			char_type, ::fast_io::manipulators::floating_point_default_scalar_flags>(
			token.first, token.last, temporary)};
	if (conversion.code != ::fast_io::parse_code::ok)
	{
		return conversion.code;
	}
	if (conversion.iter != token.last)
	{
		return ::fast_io::parse_code::invalid;
	}
	destination = temporary;
	return ::fast_io::parse_code::ok;
}

template <::std::integral char_type, typename number_type>
using json_custom_number_scanner_t = decltype(::fast_io::io_scan_forward<char_type>(
	::fast_io::io_scan_alias(::std::declval<number_type &>())));

/*
Probe the normalized proxy's actual protocols instead of placing
`parse_by_scan` in a requires-expression.  The latter contains a diagnostic
static_assert in its non-scannable branch, so it is intentionally not a
SFINAE-capable recognition API.  Alias and status forwarding are part of the
type calculation: a user may put the scan CPO on either resulting proxy rather
than directly on the numeric class.
*/
template <typename char_type, typename number_type>
concept json_custom_number_scannable =
	::std::integral<char_type> &&
	(::fast_io::precise_reserve_scannable<
		 char_type, json_custom_number_scanner_t<char_type, number_type>> ||
	 ::fast_io::contiguous_scannable<
		 char_type, json_custom_number_scanner_t<char_type, number_type>> ||
	 ::fast_io::context_scannable<
		 char_type, json_custom_number_scanner_t<char_type, number_type>>);

template <typename number_type, ::std::integral char_type>
	requires(!::fast_io::details::my_integral<number_type> &&
			 !::fast_io::details::my_floating_point<number_type> &&
			 json_custom_number_scannable<char_type, number_type>)
[[nodiscard]] inline constexpr ::fast_io::parse_code
parse_json_custom_number(basic_json_number_token<char_type const *> const &token,
						 number_type &destination)
{
	number_type temporary{};
	decltype(auto) scanner{::fast_io::io_scan_forward<char_type>(
		::fast_io::io_scan_alias(temporary))};
	/*
	The capability concept above gates the internal terminal-range dispatcher,
	so its diagnostic non-scannable branch is never instantiated.  This retains
	the full precise/contiguous/context CPO semantics (including iterator-range
	validation and context EOF completion) without re-entering the public alias
	wrapper or paying for built-in scalar policy dispatch.
	*/
	auto const conversion{::fast_io::details::parse_by_scan_impl(
		token.first, token.last, scanner)};
	if (conversion.code != ::fast_io::parse_code::ok)
	{
		return conversion.code;
	}
	if (conversion.iter != token.last)
	{
		return ::fast_io::parse_code::invalid;
	}
	destination = ::std::move(temporary);
	return ::fast_io::parse_code::ok;
}

template <typename number_type, ::std::integral char_type>
	requires(::fast_io::json_signed_integer<number_type> ||
			 ::fast_io::json_unsigned_integer<number_type> ||
			 ::fast_io::json_floating_point<number_type>)
[[nodiscard]] inline constexpr ::fast_io::parse_code
parse_json_number_into(basic_json_number_token<char_type const *> const &token,
					   number_type &destination)
{
	if constexpr (::fast_io::details::my_integral<number_type>)
	{
		return parse_json_integer(token, destination);
	}
	else if constexpr (::fast_io::details::my_floating_point<number_type>)
	{
		return parse_json_floating(token, destination);
	}
	else
	{
		if constexpr (json_custom_number_scannable<char_type, number_type>)
		{
			return parse_json_custom_number(token, destination);
		}
		else
		{
			return ::fast_io::parse_code::invalid;
		}
	}
}

} // namespace details

} // namespace fast_io::json
