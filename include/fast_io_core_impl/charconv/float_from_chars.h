#pragma once

/*
Floating from_chars: proof and interface boundary
=================================================

This header is included after fast_io's floating scanner.  It deliberately
contains only the bounded charconv grammar and dispatch layer; the arithmetic
engine remains in fast_io_unit/floating/{decfloat,hexfloat}.h so core-only
builds do not acquire the floating tables.

Mathematical model
------------------

After removing the optional minus sign, a decimal token with digits d[0..n)
and k digits after the radix point denotes exactly

                    x = M * 10^(E-k),
        M = sum(d[i] * 10^(n-1-i)).

Leading and trailing zeroes change the pair (M,E-k), never x.  The scanner
therefore keeps a bounded leading significand, the total decimal exponent, and
a sticky predicate saying whether any omitted digit is nonzero.  If the kept
prefix is A and r decimal positions were omitted, the exact magnitude lies in

        [A*10^r, (A+1)*10^r)                    (sticky unknown),

or is the single left endpoint when no nonzero digit was omitted.  Multiplying
by 10^q is multiplying by 5^q and shifting by q binary places.  The cached
power path computes the high product with integer arithmetic.  It accepts only
when both endpoints round to the same target representation.  Monotonicity of
every supported IEEE rounding map proves that every x in the interval then
has that result.  If the endpoints disagree, scan_decfloat_assign_big (or the
native wide exact path) evaluates the retained decimal integer and power
exactly; thus the fast path is an optimization, not an approximation.

Let a positive finite target binade have adjacent values a<b.  The ten explicit
policies implemented by floating_rounding choose:

  * nearest policies: a or b according as x is below or above (a+b)/2;
    at equality the policy selects even, odd, +infinity, -infinity, zero, or
    away from zero as named;
  * directed policies: the appropriate endpoint of [a,b], with the direction
    reversed only by the sign where required.

The scanner's guard/round/sticky predicates are exactly the three comparisons
with that midpoint/endpoints.  Carry from the significand is followed by an
exponent increment; at the normal/subnormal seam this produces the least
normal value, and at the top binade it produces the policy's overflow result.
Consequently the integer decision is the definition of the selected rounding
map, including signed zero, underflow, and overflow.

For hexadecimal input, digits h[0..n) with k fractional digits denote

                    x = H * 2^(P-4k).

No cached irrational scale is needed.  Keeping p+guard bits plus a sticky bit
is sufficient because division by a power of two exposes the discarded suffix
exactly.  scan_hexfloat_assign_ieee_result applies the same endpoint decision.
Unlike a C++ hexadecimal literal, std::from_chars permits an omitted `p`
exponent; the adapter below therefore treats a missing or incomplete exponent
as P=0 and leaves the cursor at `p`.  The stream scanner retains its stricter
token grammar and is not weakened by this charconv-specific rule.

Grammar and representation proof
--------------------------------

All syntax comparisons use char_literal_v/char_digit_to_literal.  Therefore
the accepted abstract characters are identical for char, wchar_t, char8_t,
char16_t, and char32_t; only their execution-code-unit representations differ.
The eight-byte digit path is enabled only for one-byte ASCII execution
characters.  EBCDIC char/wchar_t and every wide code unit take the scalar
mapping, so no ASCII subtraction is smuggled into the semantic path.

The destination is updated only after an `ok` result.  Hence invalid input and
result_out_of_range preserve the caller's value, as required by charconv, even
though the lower scanner also serves APIs whose overflow policy materializes a
boundary value.  A decimal general token ending in an incomplete exponent
(`1e`, `1e+`) is reparsed with the fixed grammar; this proves the standard
longest-prefix result `1` and cursor at `e` without adding a second numeric
conversion algorithm.

Dispatch equivalence
--------------------

For a fixed Format and Rounding, from_chars_floating_fixed instantiates one
immutable scalar_flags value.  The public runtime-format overload is the
four-case sum of those same instantiations.  On GNU-family compilers,
__builtin_constant_p merely lets a call-site literal select its instantiation
before the dynamic switch is formed.  Substitution of the known enumerator
shows that the selected call has identical pointers, flags, arithmetic, and
result mapping.  If the builtin returns false, execution reaches the switch.
Thus optimizer-constant and runtime-format entry points are observationally
equivalent; constant evaluation also uses the same constexpr integer engine
while runtime-only SIMD branches are excluded by their own consteval guards.
*/

namespace fast_io
{

namespace details
{

template <::std::chars_format format,
		  ::fast_io::manipulators::floating_rounding rounding>
inline consteval ::fast_io::manipulators::scalar_flags
from_chars_floating_flags() noexcept
{
	auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.noskipws = true;
	flags.allow_leading_plus = false;
	flags.rounding = rounding;
	if constexpr (format == ::std::chars_format::fixed)
	{
		/*
		Fixed grammar ends before an exponent marker.  Selecting the fixed
		scanner is therefore exactly the language restriction, not merely a
		presentation preference.
		*/
		flags.floating = ::fast_io::manipulators::floating_format::fixed;
	}
	else if constexpr (format == ::std::chars_format::scientific)
	{
		/*
		Scientific charconv requires a syntactically complete exponent.
		The scientific scanner enforces both the marker and at least one
		exponent digit, so no adapter-side approximation is involved.
		*/
		flags.floating = ::fast_io::manipulators::floating_format::scientific;
	}
	else if constexpr (format == ::std::chars_format::hex)
	{
		/*
		Hexadecimal input uses the exact H*2^q engine.  `showbase=false`
		encodes the charconv rule that the input has no `0x` prefix.
		*/
		flags.floating = ::fast_io::manipulators::floating_format::hexfloat;
		flags.showbase = false;
	}
	else
	{
		static_assert(format == ::std::chars_format::general);
		/*
		General admits either fixed or scientific decimal syntax.  Decimal
		mode accepts both and applies the longest-valid-prefix rule proved in
		from_chars_floating_fixed below.
		*/
		flags.floating = ::fast_io::manipulators::floating_format::decimal;
	}
	return flags;
}

template <::fast_io::details::character char_type>
[[nodiscard]] inline constexpr ::fast_io::basic_from_chars_result<char_type>
from_chars_floating_map_result(
	::fast_io::parse_result<char_type const *> result,
	char_type const *original_first) noexcept
{
	if (result.code == ::fast_io::parse_code::ok) [[likely]]
	{
		/*
		An `ok` scanner cursor is the first unconsumed abstract character.
		Returning it unchanged preserves the maximal-prefix grammar for every
		code-unit type.
		*/
		return {result.iter, {}};
	}
	if (result.code == ::fast_io::parse_code::overflow)
	{
		/*
		The arithmetic engine has consumed a valid token but its rounded value
		is outside the target range.  Charconv reports that token endpoint with
		result_out_of_range; the caller-visible value remains unchanged because
		conversion was performed into a temporary.
		*/
		return {result.iter, ::std::errc::result_out_of_range};
	}
	/*
	For a grammatical failure, charconv defines zero consumption regardless of
	how far a speculative scanner looked.  Restoring original_first is the
	strong lexical rollback required by that rule.
	*/
	return {original_first, ::std::errc::invalid_argument};
}

/*
std::chars_format::hex differs from the library's stream token in one point:
the exponent part is optional.  This is the existing scalar hexadecimal scan
through the significand stage, followed by the optional-exponent rule.  No
arithmetic or rounding code is duplicated.
*/
template <::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::character char_type,
		  ::fast_io::details::my_floating_point T>
[[nodiscard]] inline constexpr ::fast_io::parse_result<char_type const *>
from_chars_hexadecimal_optional_exponent(
	char_type const *begin, char_type const *end, T &value) noexcept
{
	using floating_type = ::std::remove_cv_t<T>;
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	constexpr ::std::size_t precision_bits{trait::mbits + 1u};
	constexpr ::std::size_t stored_hex_digits_limit{
		(precision_bits + 7u) / 4u};
	using storage_type =
		::fast_io::details::scan_hexfloat_storage_t<precision_bits>;
	static_assert(stored_hex_digits_limit * 4u <=
				  sizeof(storage_type) *
					  ::std::numeric_limits<unsigned char>::digits);

	auto first{begin};
	constexpr auto minus{::fast_io::char_literal_v<u8'-', char_type>};
	bool negative{};
	if (first != end && *first == minus)
	{
		/*
		Negation is applied after magnitude rounding.  Advancing only for `-`
		proves that `+` remains invalid under the charconv grammar.
		*/
		negative = true;
		++first;
	}

	auto const special{
		::fast_io::details::scan_hexfloat_special_value<
			flags.nan_parse_sign, flags.nan_payload_scan>(
			first, end, negative, value)};
	if (special.matched)
	{
		/*
		Infinity and NaN have no hexadecimal exponent or significand rounding;
		the shared special parser has already established their maximal valid
		prefix and exact representation policy.
		*/
		return {special.iter, special.code};
	}

	::fast_io::details::scan_hexfloat_significand_state<storage_type> state;
	first = ::fast_io::details::scan_hexfloat_significand_run<
		stored_hex_digits_limit>(first, end, false, state);
	constexpr auto dot{::fast_io::char_literal_v<u8'.', char_type>};
	if (first != end && *first == dot)
	{
		/*
		Each hexadecimal digit after the radix contributes a factor 2^-4.
		The shared state counts those digits, so removing the punctuation alone
		does not alter H*2^(P-4k).
		*/
		++first;
		first = ::fast_io::details::scan_hexfloat_significand_run<
			stored_hex_digits_limit>(first, end, true, state);
	}
	if (!state.has_digit)
	{
		/*
		Neither a sign nor a radix point denotes a number.  Returning `begin`
		rather than the speculative cursor proves the zero-consumption invalid
		result required by charconv.
		*/
		return {begin, ::fast_io::parse_code::invalid};
	}

	::std::int_least64_t exponent{};
	auto parsed_end{first};
	constexpr auto lower_p{::fast_io::char_literal_v<u8'p', char_type>};
	constexpr auto upper_p{::fast_io::char_literal_v<u8'P', char_type>};
	if (first != end && (*first == lower_p || *first == upper_p))
	{
		auto exponent_first{first + 1};
		/*
		scan_hexfloat_exponent admits exactly one optional exponent sign.
		Consequently `p+d` and `p-d` denote P=+d and P=-d, while `p+-d`,
		`p--d`, and every sign without a following digit fail as a complete
		exponent.  In the latter cases the optional-exponent grammar retains
		the already valid hexadecimal significand and leaves the cursor at
		`p`.  This is precisely the strtod subject-sequence grammar after
		removing charconv's implicit hexadecimal prefix; accepting a second
		sign would change H*2^0 into a value not denoted by the input.
		*/
		auto const exponent_result{
			::fast_io::details::scan_hexfloat_exponent(
				exponent_first, end, exponent)};
		if (exponent_result.code == ::fast_io::parse_code::ok)
		{
			/*
			A complete exponent changes H*2^-4k to H*2^(P-4k), and its returned
			cursor is therefore part of the accepted token.
			*/
			parsed_end = exponent_result.iter;
		}
		/*
		Otherwise the longest valid hexadecimal prefix ends before `p`.
		Leaving both `parsed_end` and exponent at their initial values is
		exactly the optional-exponent default P=0; no malformed exponent byte is
		consumed.
		*/
	}

	if (!state.has_nonzero_digit)
	{
		/*
		For H=0 every exponent denotes exact zero.  Only the sign bit can
		distinguish the result, so bypassing the quotient engine is exact even
		for saturated textual exponents.
		*/
		value = negative ? -static_cast<T>(0.0) : static_cast<T>(0.0);
		return {parsed_end, ::fast_io::parse_code::ok};
	}

	constexpr auto int64_max{
		(::std::numeric_limits<::std::int_least64_t>::max)()};
	constexpr auto int64_min{
		(::std::numeric_limits<::std::int_least64_t>::min)()};
	auto const fractional_exponent{
		state.fractional_hex_digits > int64_max / 4
			? int64_max
			: state.fractional_hex_digits * 4};
	/*
	When 4k is not representable, its true value is larger than INT64_MAX;
	saturating it can only preserve the classification "far below the minimum
	binary exponent".  The following guarded subtraction likewise saturates at
	INT64_MIN.  The assignment engine therefore reaches the same underflow
	decision without signed overflow.
	*/
	auto const binary_exponent{
		exponent < int64_min + fractional_exponent
			? int64_min
			: exponent - fractional_exponent};
	return {
		parsed_end,
		::fast_io::details::scan_hexfloat_assign_ieee_result<
			T, storage_type, flags.rounding>(
			value, negative, state.stored, state.stored_hex_digits,
			state.significant_hex_digits, state.truncated_nonzero,
			binary_exponent)};
}

template <::std::chars_format format,
		  ::fast_io::manipulators::floating_rounding rounding,
		  ::fast_io::details::scan_decfloat_supported_floating_point T,
		  ::fast_io::details::character char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr ::fast_io::basic_from_chars_result<char_type>
from_chars_floating_fixed(
	char_type const *first, char_type const *last, T &value) noexcept
{
	constexpr auto flags{
		::fast_io::details::from_chars_floating_flags<format, rounding>()};
	T temporary{};
	::fast_io::parse_result<char_type const *> parsed;
	if constexpr (format == ::std::chars_format::hex)
	{
		/*
		The hexadecimal adapter differs only in the optional exponent grammar;
		it ends in the same exact binary assignment routine as the stream
		scanner.
		*/
		parsed =
			::fast_io::details::from_chars_hexadecimal_optional_exponent<flags>(
				first, last, temporary);
	}
	else
	{
		parsed =
			::fast_io::details::scan_decfloat_contiguous_define<
				char_type, flags>(first, last, temporary);
		if constexpr (format == ::std::chars_format::general)
		{
			if (parsed.code == ::fast_io::parse_code::partial)
			{
				/*
				At a bounded charconv endpoint, `1e` or `1e+` cannot be extended.
				The prefix before `e` is nevertheless a complete fixed decimal.
				Reparsing with fixed grammar returns exactly that prefix; both
				parses use the same digit state and rounding function, so this
				is a lexical rollback rather than a second numeric semantics.
				*/
				constexpr auto fixed_flags{
					::fast_io::details::from_chars_floating_flags<
						::std::chars_format::fixed, rounding>()};
				parsed =
					::fast_io::details::scan_decfloat_contiguous_define<
						char_type, fixed_flags>(first, last, temporary);
			}
		}
	}
	auto result{
		::fast_io::details::from_chars_floating_map_result(parsed, first)};
	if (result.ec == ::std::errc{})
	{
		/*
		Only successful conversion commits the temporary.  Invalid and
		out-of-range branches consequently preserve every bit of the caller's
		object, including a pre-existing NaN payload.
		*/
		value = temporary;
	}
	return result;
}

} // namespace details

/*
Rounding is the leading template parameter so callers may write

    fast_io::from_chars<floating_rounding::toward_zero>(first, last, value);

The default overload remains source-compatible with std::from_chars.
*/
template <
	::fast_io::manipulators::floating_rounding rounding =
		::fast_io::manipulators::floating_rounding::nearest_to_even,
	::fast_io::details::scan_decfloat_supported_floating_point T,
	::fast_io::details::character char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr ::fast_io::basic_from_chars_result<char_type>
from_chars(char_type const *first, char_type const *last, T &value,
		   ::std::chars_format format = ::std::chars_format::general) noexcept
{
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
	/*
	Each true arm substitutes one known enumerator into the same fixed-format
	instantiation used by the switch below.  The conjunction also tests the
	value, so a compiler that merely proves `format` constant cannot select the
	wrong arm.
	*/
	if (__builtin_constant_p(format) &&
		format == ::std::chars_format::general)
	{
		return ::fast_io::details::from_chars_floating_fixed<
			::std::chars_format::general, rounding>(first, last, value);
	}
	if (__builtin_constant_p(format) &&
		format == ::std::chars_format::scientific)
	{
		return ::fast_io::details::from_chars_floating_fixed<
			::std::chars_format::scientific, rounding>(first, last, value);
	}
	if (__builtin_constant_p(format) &&
		format == ::std::chars_format::fixed)
	{
		return ::fast_io::details::from_chars_floating_fixed<
			::std::chars_format::fixed, rounding>(first, last, value);
	}
	if (__builtin_constant_p(format) &&
		format == ::std::chars_format::hex)
	{
		return ::fast_io::details::from_chars_floating_fixed<
			::std::chars_format::hex, rounding>(first, last, value);
	}
#endif
	switch (format)
	{
	/*
	The four cases are exhaustive for the standard bitmask enumerators admitted
	by this overload.  Each case is definitionally identical to its
	optimizer-constant arm above; only the time of dispatch differs.
	*/
	case ::std::chars_format::general:
		return ::fast_io::details::from_chars_floating_fixed<
			::std::chars_format::general, rounding>(first, last, value);
	case ::std::chars_format::scientific:
		return ::fast_io::details::from_chars_floating_fixed<
			::std::chars_format::scientific, rounding>(first, last, value);
	case ::std::chars_format::fixed:
		return ::fast_io::details::from_chars_floating_fixed<
			::std::chars_format::fixed, rounding>(first, last, value);
	case ::std::chars_format::hex:
		return ::fast_io::details::from_chars_floating_fixed<
			::std::chars_format::hex, rounding>(first, last, value);
	default:
		/*
		Any other bit pattern names no supported grammar.  No scanner has run,
		so returning `first` proves both zero consumption and no destination
		mutation.
		*/
		return {first, ::std::errc::invalid_argument};
	}
}

} // namespace fast_io
