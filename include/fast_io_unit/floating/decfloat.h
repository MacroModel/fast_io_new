#pragma once
#include "hexfloat.h"
#include "roundtrip.h"

namespace fast_io
{

namespace details
{

template <typename T, ::std::size_t table_size>
inline constexpr auto scan_decfloat_generate_power10_table() noexcept
{
	using value_type = ::std::remove_cv_t<T>;
	::fast_io::freestanding::array<value_type, table_size> table{};
	value_type value{static_cast<value_type>(1)};
	for (::std::size_t i{}; i != table_size; ++i)
	{
		table.index_unchecked(i) = value;
		value = static_cast<value_type>(value * static_cast<value_type>(10));
	}
	return table;
}

template <typename T, ::std::size_t table_size>
inline constexpr auto scan_decfloat_power10_table{
	::fast_io::details::scan_decfloat_generate_power10_table<T, table_size>()};

template <typename T, ::std::size_t table_size>
inline constexpr auto scan_decfloat_generate_clinger_max_mantissa_table() noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	::fast_io::freestanding::array<::std::uint_least64_t, table_size> table{};
	::std::uint_least64_t divisor{1u};
	constexpr ::std::uint_least64_t limit{::std::uint_least64_t{2u} << trait::mbits};
	for (::std::size_t i{}; i != table_size; ++i)
	{
		table.index_unchecked(i) = limit / divisor;
		divisor *= 5u;
	}
	return table;
}

template <typename T, ::std::size_t table_size>
inline constexpr auto scan_decfloat_clinger_max_mantissa_table{
	::fast_io::details::scan_decfloat_generate_clinger_max_mantissa_table<T, table_size>()};

template <typename T>
concept scan_decfloat_has_iec559_traits = requires {
	typename ::fast_io::details::iec559_traits<::std::remove_cvref_t<T>>::mantissa_type;
};

/*
numeric_limits describes arithmetic precision, not necessarily an IEC 60559
object encoding.  PowerPC IBM double-double long double reports 106
significant bits but stores two binary64 components.  It is admitted only by
the explicit component-sum bridge in punning.h; the ordinary implicit-field
case still requires equal object/carrier sizes and therefore cannot
accidentally concatenate those components.  The remaining layouts are
binary16, bfloat16, binary32, binary64, little-endian x87 binary80, and
binary128.
*/
template <typename T, bool = ::fast_io::details::scan_decfloat_has_iec559_traits<T>>
struct scan_decfloat_layout_supported_impl
{
	inline static constexpr bool value{};
};

template <typename T>
struct scan_decfloat_layout_supported_impl<T, true>
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	using mantissa_type = typename trait::mantissa_type;
	inline static constexpr bool x87_binary80{
		::fast_io::details::fp_floating_point_is_float80<no_cvref_t> &&
		::std::endian::native == ::std::endian::little && trait::mbits == 63u &&
		trait::ebits == 15u && sizeof(mantissa_type) == sizeof(::std::uint_least64_t)};
	inline static constexpr bool implicit_integer_bit{
		sizeof(no_cvref_t) == sizeof(mantissa_type) &&
		((trait::mbits == 10u && trait::ebits == 5u) ||
		 (trait::mbits == 7u && trait::ebits == 8u) ||
		 (trait::mbits == 23u && trait::ebits == 8u) ||
		 (trait::mbits == 52u && trait::ebits == 11u) ||
		 (trait::mbits == 112u && trait::ebits == 15u))};
	inline static constexpr bool ibm_double_double{
#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
		::fast_io::details::fp_floating_point_is_ibm_double_double<no_cvref_t>
#else
		false
#endif
	};
	inline static constexpr bool value{x87_binary80 || implicit_integer_bit ||
		ibm_double_double};
};

template <typename T>
inline constexpr bool scan_decfloat_layout_supported{
	::fast_io::details::scan_decfloat_layout_supported_impl<T>::value};

template <typename T, bool = ::fast_io::details::scan_decfloat_has_iec559_traits<T>>
struct scan_decfloat_compute_supported_impl
{
	inline static constexpr bool value{};
};

template <typename T>
struct scan_decfloat_compute_supported_impl<T, true>
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	inline static constexpr bool value{sizeof(typename trait::mantissa_type) <= sizeof(::std::uint_least64_t) &&
									   trait::mbits + 3u <= 64u &&
									   trait::e10max <= ::fast_io::details::iec559_traits<double>::e10max};
};

template <typename T>
inline constexpr bool scan_decfloat_compute_supported{
	::fast_io::details::scan_decfloat_compute_supported_impl<T>::value};

template <typename T>
concept scan_decfloat_supported_floating_point =
	::fast_io::details::my_floating_point<::std::remove_cvref_t<T>> &&
	::fast_io::details::scan_decfloat_layout_supported<T>;

template <typename T, bool = ::fast_io::details::scan_decfloat_has_iec559_traits<T>>
struct scan_decfloat_binary32_like_impl
{
	inline static constexpr bool value{};
};

template <typename T>
struct scan_decfloat_binary32_like_impl<T, true>
{
	using trait = ::fast_io::details::iec559_traits<::std::remove_cvref_t<T>>;
	inline static constexpr bool value{trait::mbits == 23u && trait::ebits == 8u &&
									   sizeof(typename trait::mantissa_type) <= sizeof(::std::uint_least32_t)};
};

template <typename T, bool = ::fast_io::details::scan_decfloat_has_iec559_traits<T>>
struct scan_decfloat_binary64_like_impl
{
	inline static constexpr bool value{};
};

template <typename T>
struct scan_decfloat_binary64_like_impl<T, true>
{
	using trait = ::fast_io::details::iec559_traits<::std::remove_cvref_t<T>>;
	inline static constexpr bool value{trait::mbits == 52u && trait::ebits == 11u &&
									   sizeof(typename trait::mantissa_type) <= sizeof(::std::uint_least64_t)};
};

template <typename T>
inline constexpr bool scan_decfloat_binary32_like{
	::fast_io::details::scan_decfloat_binary32_like_impl<T>::value};

template <typename T>
inline constexpr bool scan_decfloat_binary64_like{
	::fast_io::details::scan_decfloat_binary64_like_impl<T>::value};

template <::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr bool scan_decfloat_decimal_digit(char_type ch, char8_t &digit) noexcept
{
	using unsigned_char_type = ::fast_io::details::my_make_unsigned_t<char_type>;
	auto uch{static_cast<unsigned_char_type>(ch)};
	if constexpr (sizeof(char_type) == sizeof(char8_t) && ::fast_io::details::is_ascii<char_type>)
	{
		constexpr auto zero{static_cast<unsigned_char_type>(::fast_io::char_literal_v<u8'0', char_type>)};
		auto const value{static_cast<unsigned_char_type>(uch - zero)};
		if (value < 10u)
		{
			digit = static_cast<char8_t>(value);
			return true;
		}
	}
	else
	{
		if (::fast_io::details::char_digit_to_literal<10, char_type>(uch))
		{
			return false;
		}
		digit = static_cast<char8_t>(uch);
		return true;
	}
	return false;
}

template <typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr bool scan_decfloat_try_clinger(::std::uint_least64_t significand,
															  ::std::int_least64_t exponent,
															  bool negative, T &value) noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::details::scan_decfloat_binary32_like<no_cvref_t>)
	{
		constexpr ::std::uint_least64_t significand_limit{::std::uint_least64_t{1} << 24u};
		if (significand <= significand_limit && -10 <= exponent && exponent <= 10)
		{
			no_cvref_t result{static_cast<no_cvref_t>(significand)};
			auto const &table{::fast_io::details::scan_decfloat_power10_table<no_cvref_t, 11u>};
			if (exponent < 0)
			{
				result /= table.index_unchecked(static_cast<::std::size_t>(-exponent));
			}
			else
			{
				result *= table.index_unchecked(static_cast<::std::size_t>(exponent));
			}
			value = negative ? -result : result;
			return true;
		}
		if (0 <= exponent && exponent <= 10)
		{
			auto const &max_table{::fast_io::details::scan_decfloat_clinger_max_mantissa_table<no_cvref_t, 11u>};
			auto const index{static_cast<::std::size_t>(exponent)};
			if (significand <= max_table.index_unchecked(index))
			{
				auto const &table{::fast_io::details::scan_decfloat_power10_table<no_cvref_t, 11u>};
				no_cvref_t result{static_cast<no_cvref_t>(significand)};
				result *= table.index_unchecked(index);
				value = negative ? -result : result;
				return true;
			}
		}
	}
	else if constexpr (::fast_io::details::scan_decfloat_binary64_like<no_cvref_t>)
	{
		constexpr ::std::uint_least64_t significand_limit{::std::uint_least64_t{1} << 53u};
		if (significand <= significand_limit && -22 <= exponent && exponent <= 22)
		{
			no_cvref_t result{static_cast<no_cvref_t>(significand)};
			auto const &table{::fast_io::details::scan_decfloat_power10_table<no_cvref_t, 23u>};
			if (exponent < 0)
			{
				result /= table.index_unchecked(static_cast<::std::size_t>(-exponent));
			}
			else
			{
				result *= table.index_unchecked(static_cast<::std::size_t>(exponent));
			}
			value = negative ? -result : result;
			return true;
		}
		if (0 <= exponent && exponent <= 22)
		{
			auto const &max_table{::fast_io::details::scan_decfloat_clinger_max_mantissa_table<no_cvref_t, 23u>};
			auto const index{static_cast<::std::size_t>(exponent)};
			if (significand <= max_table.index_unchecked(index))
			{
				auto const &table{::fast_io::details::scan_decfloat_power10_table<no_cvref_t, 23u>};
				no_cvref_t result{static_cast<no_cvref_t>(significand)};
				result *= table.index_unchecked(index);
				value = negative ? -result : result;
				return true;
			}
		}
	}
	else if constexpr (::fast_io::details::fp_floating_point_is_float80<no_cvref_t>)
	{
		if (-27 <= exponent && exponent <= 27)
		{
			no_cvref_t result{static_cast<no_cvref_t>(significand)};
			auto const &table{::fast_io::details::scan_decfloat_power10_table<no_cvref_t, 28u>};
			if (exponent < 0)
			{
				result /= table.index_unchecked(static_cast<::std::size_t>(-exponent));
			}
			else
			{
				result *= table.index_unchecked(static_cast<::std::size_t>(exponent));
			}
			value = negative ? -result : result;
			return true;
		}
	}
	return false;
}

/*
The Clinger shortcut evaluates a small decimal with native floating multiply
or divide.  Its theorem assumes nearest-even hardware arithmetic.  Runtime
callers may change the C floating environment even when fast_io itself was
compiled without floating-environment integration, so the absence of a library
macro cannot prove that assumption.  The three assignment sites below admit
Clinger only during constant evaluation, whose arithmetic mode is fixed by the
translation and cannot observe a runtime fenv.  Runtime input always uses the
integer cached-product/exact pipeline and is consequently a pure function of
the explicit rounding template argument.
*/

/*
The retained prefix must be long enough to contain every terminating decimal
midpoint of every supported IEEE binary format.  This is stronger than merely
retaining enough digits to print the destination: an input can lie at the
midpoint between zero and the least subnormal, whose terminating expansion is
very long for binary80 and binary128.

After powers of two are cancelled, any midpoint is A/2^k with A odd.  Its
terminating decimal significand is A*5^k.  For the widest supported format,
binary128, k<=16495 and A has at most p+1=114 bits.  Hence its significand has
at most

  floor(log10(A)+k*log10(5))+1
	< 114*log10(2)+16495*log10(5)+1 < 11565

digits.  The round value 12000 leaves 435 guard digits.  Narrow destinations
use their smaller independently sufficient bounds below, so a binary32/64 hot
parse does not pay binary128's stack and streaming-storage cost.  If a longer
input agrees with a midpoint in every retained place, the midpoint has already
terminated, so the omitted input is either all zero (equal) or has a nonzero
digit (strictly greater).  Conversely, a value below that midpoint must differ
inside the retained prefix.  Thus the prefix plus exact_truncated_nonzero is
sufficient to order every input around every rounding boundary; no unbounded
input storage is required.

For binary64, a midpoint numerator has at most p+1=54 bits and k<=1075,
making the same strict bound smaller than
54*log10(2)+1075*log10(5)+1<768.7; since a digit count is integral, 768
positions suffice.  IBM double-double shares binary64's minimum quantum
2^-1074 but an adjacent-value midpoint can carry 107 numerator bits.  Its
strict bound is

  107*log10(2)+1075*log10(5)+1 < 785.

Sparse double-double sums can have 309 integer and 1074 fractional positions.
Retain all of those digits, with room for the midpoint of the low component.
Every format
with ebits<=8 has p<=24 and k<=150, giving a bound below 114, so 128 positions
suffice for binary32, bfloat16, binary16, and narrower encodings.  The four
capacities below are therefore proved rather than empirical parser limits.
*/
template <typename T>
inline constexpr ::std::size_t scan_decfloat_exact_digit_capacity{
	::fast_io::details::fp_floating_point_is_ibm_double_double<::std::remove_cvref_t<T>> ? 1400u : ::fast_io::details::iec559_traits<::std::remove_cvref_t<T>>::ebits >= 15u ? 12000u : ::fast_io::details::iec559_traits<::std::remove_cvref_t<T>>::ebits >= 11u ? 768u
																															   : 128u};

template <typename T>
struct scan_decfloat_significand_state
{
	::std::uint_least64_t significand{};
	::std::uint_least64_t significant_digits{};
	::std::uint_least64_t stored_digits{};
	::std::uint_least64_t fractional_digits{};
	::std::uint_least16_t exact_stored_digits{};
	bool has_digit{};
	bool has_nonzero_digit{};
	bool truncated_nonzero{};
	bool exact_truncated_nonzero{};
	char8_t exact_digits[scan_decfloat_exact_digit_capacity<T>];
};

template <::std::integral char_type>
struct scan_decfloat_fast_result
{
	/// On a miss, only an over-capacity coefficient supplies its first excess digit.
	char_type const *iter{};
	::fast_io::parse_code code{};
	bool handled{};
};

inline constexpr ::std::uint_least64_t scan_decfloat_uint64_significand_digit_limit{19u};

template <typename>
inline constexpr ::std::uint_least64_t scan_decfloat_significand_digit_limit{
	::fast_io::details::scan_decfloat_uint64_significand_digit_limit};
inline constexpr ::std::int_least32_t scan_decfloat_dragonbox_min_power10{-342};
inline constexpr ::std::int_least32_t scan_decfloat_dragonbox_max_power10{326};
inline constexpr ::std::uint_least64_t scan_decfloat_pow10_0_to_8_table[]{
	1u, 10u, 100u, 1000u, 10000u, 100000u, 1000000u, 10000000u, 100000000u};

template <typename T>
inline constexpr void scan_decfloat_append_exact_digit(scan_decfloat_significand_state<T> &state,
													   char8_t digit) noexcept
{
	if (state.exact_stored_digits != ::fast_io::details::scan_decfloat_exact_digit_capacity<T>)
	{
		state.exact_digits[state.exact_stored_digits] = digit;
		++state.exact_stored_digits;
	}
	else if (digit != 0)
	{
		state.exact_truncated_nonzero = true;
	}
}

template <typename T>
inline constexpr void scan_decfloat_append_exact_eight_digits(scan_decfloat_significand_state<T> &state,
															  ::std::uint_least32_t digits) noexcept
{
	if (state.exact_stored_digits == ::fast_io::details::scan_decfloat_exact_digit_capacity<T>)
	{
		if (digits != 0)
		{
			state.exact_truncated_nonzero = true;
		}
		return;
	}
	for (auto divisor{10000000u}; divisor != 0u; divisor /= 10u)
	{
		auto const digit{static_cast<char8_t>(digits / divisor)};
		digits -= static_cast<::std::uint_least32_t>(digit) * divisor;
		::fast_io::details::scan_decfloat_append_exact_digit(state, digit);
		if (divisor == 1u)
		{
			break;
		}
	}
}

template <typename T>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr void scan_decfloat_append_exact_ascii8_digits_slow(scan_decfloat_significand_state<T> &state,
																	::std::uint_least64_t val) noexcept
{
	for (::std::size_t offset{}; offset != 8u; ++offset)
	{
		auto const ch{static_cast<char8_t>((val >> (offset * 8u)) & 0xFFu)};
		auto const digit{static_cast<char8_t>(ch - u8'0')};
		::fast_io::details::scan_decfloat_append_exact_digit(state, digit);
	}
}

template <typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void scan_decfloat_append_exact_ascii8_digits(scan_decfloat_significand_state<T> &state,
															   ::std::uint_least64_t val) noexcept
{
	if constexpr (::std::endian::native == ::std::endian::little &&
				  ::std::numeric_limits<::std::uint_least64_t>::digits == 64u)
	{
		if (state.exact_stored_digits + 8u <= ::fast_io::details::scan_decfloat_exact_digit_capacity<T>)
		{
			auto const digits{static_cast<::std::uint_least64_t>(val - 0x3030303030303030u)};
			::fast_io::freestanding::my_memcpy(
				state.exact_digits + state.exact_stored_digits, __builtin_addressof(digits),
				sizeof(::std::uint_least64_t));
			state.exact_stored_digits += 8u;
			return;
		}
	}
	::fast_io::details::scan_decfloat_append_exact_ascii8_digits_slow(state, val);
}

struct scan_decfloat_adjusted_mantissa
{
	::std::uint_least64_t mantissa{};
	::std::int_least32_t power2{};
};

[[nodiscard]] inline constexpr bool
scan_decfloat_adjusted_mantissa_equal(scan_decfloat_adjusted_mantissa left,
									  scan_decfloat_adjusted_mantissa right) noexcept
{
	return left.mantissa == right.mantissa && left.power2 == right.power2;
}

struct scan_decfloat_uint128
{
	::std::uint_least64_t low{};
	::std::uint_least64_t high{};
};

[[nodiscard]] inline constexpr ::std::int_least32_t
scan_decfloat_binary_power(::std::int_least32_t exponent) noexcept
{
	return static_cast<::std::int_least32_t>((((152170 + 65536) * exponent) >> 16) + 63);
}

template <typename T>
[[nodiscard]] inline constexpr ::std::int_least32_t scan_decfloat_smallest_power10() noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	if constexpr (trait::mbits == 10u && trait::ebits == 5u)
	{
		return -27;
	}
	else if constexpr (trait::mbits == 7u && trait::ebits == 8u)
	{
		return -60;
	}
	else if constexpr (::fast_io::details::scan_decfloat_binary32_like<no_cvref_t>)
	{
		return -64;
	}
	else if constexpr (::fast_io::details::scan_decfloat_binary64_like<no_cvref_t>)
	{
		return -342;
	}
	else
	{
		return ::fast_io::details::scan_decfloat_dragonbox_min_power10;
	}
}

template <typename T>
[[nodiscard]] inline constexpr ::std::int_least32_t scan_decfloat_largest_power10() noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	if constexpr (trait::mbits == 10u && trait::ebits == 5u)
	{
		return 4;
	}
	else if constexpr (::fast_io::details::scan_decfloat_binary32_like<no_cvref_t>)
	{
		return 38;
	}
	else if constexpr (::fast_io::details::scan_decfloat_binary64_like<no_cvref_t>)
	{
		return 308;
	}
	else
	{
		return static_cast<::std::int_least32_t>(trait::e10max);
	}
}

template <typename T>
[[nodiscard]] inline constexpr ::std::int_least32_t scan_decfloat_minimum_exponent() noexcept
{
	using trait = ::fast_io::details::iec559_traits<::std::remove_cv_t<T>>;
	return -static_cast<::std::int_least32_t>((static_cast<::std::uint_least32_t>(1u) << (trait::ebits - 1u)) - 1u);
}

template <typename T>
[[nodiscard]] inline constexpr ::std::int_least32_t scan_decfloat_infinite_power() noexcept
{
	using trait = ::fast_io::details::iec559_traits<::std::remove_cv_t<T>>;
	return static_cast<::std::int_least32_t>((static_cast<::std::uint_least32_t>(1u) << trait::ebits) - 1u);
}

template <typename T>
[[nodiscard]] inline constexpr ::std::int_least32_t scan_decfloat_min_round_to_even_power10() noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	if constexpr (trait::mbits == 10u && trait::ebits == 5u)
	{
		return -22;
	}
	else if constexpr (trait::mbits == 7u && trait::ebits == 8u)
	{
		return -24;
	}
	else if constexpr (::fast_io::details::scan_decfloat_binary32_like<no_cvref_t>)
	{
		return -17;
	}
	else if constexpr (::fast_io::details::scan_decfloat_binary64_like<no_cvref_t>)
	{
		return -4;
	}
	else
	{
		return 0;
	}
}

template <typename T>
[[nodiscard]] inline constexpr ::std::int_least32_t scan_decfloat_max_round_to_even_power10() noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	if constexpr (trait::mbits == 10u && trait::ebits == 5u)
	{
		return 5;
	}
	else if constexpr (trait::mbits == 7u && trait::ebits == 8u)
	{
		return 3;
	}
	else if constexpr (::fast_io::details::scan_decfloat_binary32_like<no_cvref_t>)
	{
		return 10;
	}
	else if constexpr (::fast_io::details::scan_decfloat_binary64_like<no_cvref_t>)
	{
		return 23;
	}
	else
	{
		return 0;
	}
}

template <::fast_io::manipulators::floating_rounding rounding>
inline constexpr bool scan_decfloat_nearest_rounding{
	rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_to_odd ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_plus_infinity ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_minus_infinity ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_zero ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_away_from_zero};

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool scan_decfloat_directed_round_up(bool negative) noexcept
{
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::toward_plus_infinity)
	{
		return !negative;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::toward_minus_infinity)
	{
		return negative;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::away_from_zero)
	{
		return true;
	}
	else
	{
		return false;
	}
}

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool scan_decfloat_nearest_tie_round_up(bool negative,
																	   ::std::uint_least64_t mantissa) noexcept
{
	auto const rounded_down{mantissa >> 1u};
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even)
	{
		return (rounded_down & 1u) != 0u;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_odd)
	{
		return (rounded_down & 1u) == 0u;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_plus_infinity)
	{
		return !negative;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_minus_infinity)
	{
		return negative;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_away_from_zero)
	{
		return true;
	}
	else
	{
		return false;
	}
}

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr ::std::uint_least64_t
scan_decfloat_round_mantissa(bool negative, ::std::uint_least64_t mantissa, bool has_tail, bool is_tie) noexcept
{
	if constexpr (::fast_io::details::scan_decfloat_nearest_rounding<rounding>)
	{
		/*
		`mantissa = 2q + g`, where q is the lower candidate and g is the
		half-ulp guard bit. Hence g=0 is strictly below the midpoint even when
		later discarded bits are nonzero, while g=1 is at or above it. All six
		nearest policies therefore share this magnitude test; they differ only
		when `is_tie` proves exact equality with the midpoint. In particular,
		nearest-to-odd is not round-to-odd jamming: parity may select a result
		only at a tie.
		*/
		if ((mantissa & 1u) != 0u)
		{
			if (!is_tie || ::fast_io::details::scan_decfloat_nearest_tie_round_up<rounding>(negative, mantissa))
			{
				++mantissa;
			}
		}
	}
	else
	{
		if (::fast_io::details::scan_decfloat_directed_round_up<rounding>(negative))
		{
			if ((mantissa & 1u) != 0u)
			{
				++mantissa;
			}
			else if (has_tail)
			{
				mantissa += ::std::uint_least64_t{2u};
			}
		}
	}
	return mantissa >> 1u;
}

[[nodiscard]] inline constexpr scan_decfloat_uint128
scan_decfloat_mul_64x128_high(::std::uint_least64_t value, ::fast_io::details::uint64x2 cache) noexcept
{
	::std::uint_least64_t high{};
	auto low{::fast_io::intrinsics::umul(value, cache.hi, high)};
	auto const middle{::fast_io::intrinsics::umulh(value, cache.lo)};
	low += middle;
	if (low < middle)
	{
		++high;
	}
	return {low, high};
}

/**
 * @brief Converts a complete 64-bit decimal coefficient to the explicit x87 significand.
 * @details Let M be the normalized coefficient and C the cached normalized power of ten.
 * The cache differs from the exact power by at most one unit in its low word. Multiplication
 * by M < 2^64 and dropping the bottom 64 product bits gives an absolute error below two
 * units in the retained low word. Normalizing the product by one more bit keeps that error
 * below four units. Accept an approximate product only when this interval stays strictly
 * between consecutive integer and half-integer rounding boundaries. For exponents 0 to 27,
 * 5^e fits in 64 bits, so the two-word product is exact and also resolves ties and carries.
 * Every accepted exponent is in the normal x87 range. On a miss the destination is untouched.
 */
#if defined(__i386__) && defined(__ELF__)
template <::fast_io::manipulators::floating_rounding rounding, typename T, bool short_i386_chunk = false>
#else
template <::fast_io::manipulators::floating_rounding rounding, typename T>
#endif
[[nodiscard]] inline constexpr bool scan_decfloat_try_x87_cached(T &value, bool negative,
	::std::uint_least64_t coefficient, ::std::int_least64_t exponent) noexcept
{
	static_assert(::fast_io::details::fp_floating_point_is_float80<T>);
	using word = ::std::uint_least64_t;
	if (!coefficient || exponent < scan_decfloat_dragonbox_min_power10 ||
		exponent > scan_decfloat_dragonbox_max_power10 ||
		rounding == ::fast_io::manipulators::floating_rounding::current_environment)
	{
		return false;
	}

	// Every signed 64-bit integer is exactly representable in binary80.
	if (!exponent && coefficient <= static_cast<word>((::std::numeric_limits<::std::int_least64_t>::max)()))
	{
		auto const exact_integer{static_cast<T>(static_cast<::std::int_least64_t>(coefficient))};
		value = negative ? -exact_integer : exact_integer;
		return true;
	}
	auto const leading{::std::countl_zero(coefficient)};
	word const normalized{coefficient << leading};
	auto const e{static_cast<::std::int_least32_t>(exponent)};
	auto power{scan_decfloat_binary_power(e) - leading};
	if (!e)
	{
		fp_assign_float80_bits(value, normalized, static_cast<::std::uint_least32_t>(power + 16383), negative);
		return true;
	}

	auto const cache{compute_pow10_float64_scan(e)};
	bool const exact{0 < e && e <= 27};
#if defined(__i386__) && defined(__ELF__)
	if constexpr (short_i386_chunk)
	{
		if (!exact)
		{
			using narrow = ::std::uint_least32_t;
			auto const multiplier_low{static_cast<narrow>(normalized)};
			auto const multiplier_high{static_cast<narrow>(normalized >> 32u)};
			auto const cache_low{static_cast<narrow>(cache.lo >> 32u)};
			auto const cache_middle{static_cast<narrow>(cache.hi)};
			auto const cache_high{static_cast<narrow>(cache.hi >> 32u)};
			auto const upper_left{word{multiplier_high} * cache_middle};
			auto const lower_right{word{multiplier_low} * cache_high};
			auto const low_sum{word{static_cast<narrow>(upper_left)} + static_cast<narrow>(lower_right) +
				::fast_io::intrinsics::umulh(multiplier_low, cache_middle) +
				::fast_io::intrinsics::umulh(multiplier_high, cache_low)};
			auto high{word{multiplier_high} * cache_high + (upper_left >> 32u) + (lower_right >> 32u) +
				(low_sum >> 32u)};
			auto residue{static_cast<narrow>(low_sum)};
			constexpr word top_bit{word{1u} << 63u};
			constexpr narrow half{narrow{1u} << 31u};
			if ((high & top_bit) != 0u)
			{
				++power;
			}
			else
			{
				high = (high << 1u) | (residue >> 31u);
				residue <<= 1u;
			}
			// Keep the top 96 cache bits. At the retained product scale,
			// omitting the bottom partial product costs less than one unit,
			// and losing the two cross products' low limbs costs less than two.
			// The cache contributes less than 1 + 2^-32 units more. A possible
			// left shift doubles that bound, so decline within nine units of an
			// integer or half-integer.
			auto const distance{residue & (half - 1u)};
			if (distance <= 9u || distance >= half - 9u)
			{
				return false;
			}
			bool increment;
			if constexpr (scan_decfloat_nearest_rounding<rounding>)
			{
				increment = (residue & half) != 0u;
			}
			else
			{
				increment = scan_decfloat_directed_round_up<rounding>(negative);
			}
			if (increment && !++high)
			{
				high = top_bit;
				++power;
			}
			fp_assign_float80_bits(value, high, static_cast<::std::uint_least32_t>(power + 16383), negative);
			return true;
		}
	}
#endif
	scan_decfloat_uint128 product;
	if (exact)
	{
		product.low = ::fast_io::intrinsics::umul(normalized, cache.hi, product.high);
	}
	else
	{
		product = scan_decfloat_mul_64x128_high(normalized, cache);
	}

	constexpr word half{word{1u} << 63u};
	if (product.high & half)
	{
		++power;
	}
	else
	{
		product.high = (product.high << 1u) | (product.low >> 63u);
		product.low <<= 1u;
	}
	auto const residue{product.low};
	if (!exact)
	{
		auto const distance{residue & (half - 1u)};
		if (distance <= 4u || distance >= half - 4u)
		{
			return false;
		}
	}
	bool increment;
	if constexpr (scan_decfloat_nearest_rounding<rounding>)
	{
		increment = residue > half || (residue == half &&
			scan_decfloat_nearest_tie_round_up<rounding>(negative, product.high << 1u));
	}
	else
	{
		increment = residue != 0u && scan_decfloat_directed_round_up<rounding>(negative);
	}
	if (increment && !++product.high)
	{
		product.high = half;
		++power;
	}
	fp_assign_float80_bits(value, product.high, static_cast<::std::uint_least32_t>(power + 16383), negative);
	return true;
}

template <typename T, ::fast_io::manipulators::floating_rounding rounding =
						  ::fast_io::manipulators::floating_rounding::nearest_to_even>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr bool scan_decfloat_compute_adjusted(::std::int_least64_t exponent,
																   ::std::uint_least64_t significand,
																   bool negative,
																   scan_decfloat_adjusted_mantissa &answer) noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (!::fast_io::details::scan_decfloat_compute_supported<no_cvref_t>)
	{
		return false;
	}
	else
	{
		using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
		constexpr auto mantissa_explicit_bits{static_cast<::std::int_least32_t>(trait::mbits)};
		constexpr auto minimum_exponent{::fast_io::details::scan_decfloat_minimum_exponent<no_cvref_t>()};
		constexpr auto infinite_power{::fast_io::details::scan_decfloat_infinite_power<no_cvref_t>()};
		constexpr auto max_finite_mantissa{(::std::uint_least64_t{1u} << mantissa_explicit_bits) - 1u};
		if (significand == 0)
		{
			answer = {};
			return true;
		}
		if (exponent < ::fast_io::details::scan_decfloat_smallest_power10<no_cvref_t>())
		{
			if constexpr (!::fast_io::details::scan_decfloat_nearest_rounding<rounding>)
			{
				if (::fast_io::details::scan_decfloat_directed_round_up<rounding>(negative))
				{
					answer = {.mantissa = 1u, .power2 = 0};
					return true;
				}
			}
			answer = {};
			return true;
		}
		if (exponent > ::fast_io::details::scan_decfloat_largest_power10<no_cvref_t>())
		{
			if constexpr (!::fast_io::details::scan_decfloat_nearest_rounding<rounding>)
			{
				if (!::fast_io::details::scan_decfloat_directed_round_up<rounding>(negative))
				{
					answer = {.mantissa = max_finite_mantissa, .power2 = infinite_power - 1};
					return true;
				}
			}
			answer = {.mantissa = 0, .power2 = infinite_power};
			return true;
		}
		if (exponent < ::fast_io::details::scan_decfloat_dragonbox_min_power10 ||
			exponent > ::fast_io::details::scan_decfloat_dragonbox_max_power10)
		{
			return false;
		}
		auto const exponent32{static_cast<::std::int_least32_t>(exponent)};
		auto const leading_zeroes{static_cast<::std::int_least32_t>(::std::countl_zero(significand))};
		significand <<= static_cast<unsigned>(leading_zeroes);
		auto const cache{::fast_io::details::compute_pow10_float64_scan(exponent32)};
		auto const product{::fast_io::details::scan_decfloat_mul_64x128_high(significand, cache)};
		auto const upperbit{static_cast<::std::int_least32_t>(product.high >> 63u)};
		auto const shift{upperbit + 64 - mantissa_explicit_bits - 3};
		auto mantissa{product.high >> static_cast<unsigned>(shift)};
		auto power2{static_cast<::std::int_least32_t>(
			::fast_io::details::scan_decfloat_binary_power(exponent32) + upperbit - leading_zeroes - minimum_exponent)};
		if (power2 <= 0)
		{
			if constexpr (::fast_io::details::scan_decfloat_nearest_rounding<rounding>)
			{
				// Half a subnormal quantum has binary exponent -bias-mbits. A midpoint is an
				// odd multiple of that quantum. For C*10^q with q<0 this requires 5^(-q)
				// to divide C and q+v2(C) to equal the half-quantum exponent. A nonzero
				// 64-bit coefficient cannot contain 5^28. Only binary16 can satisfy both
				// conditions among the supported formats. Let the exact quotient/remainder
				// parser settle these possible ties before the cached shift loses the tail.
				constexpr auto half_quantum_exponent{minimum_exponent - mantissa_explicit_bits};
				if constexpr (half_quantum_exponent >= -27)
				{
					if (exponent32 >= -27 && exponent32 <= half_quantum_exponent &&
						exponent32 + ::std::countr_zero(significand >> static_cast<unsigned>(leading_zeroes)) ==
							half_quantum_exponent)
					{
						return false;
					}
				}
			}
			if (-power2 + 1 >= 64)
			{
				answer = {};
				return true;
			}
			auto const subnormal_shift{static_cast<unsigned>(-power2 + 1)};
			if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even)
			{
				mantissa >>= subnormal_shift;
				mantissa += mantissa & 1u;
				mantissa >>= 1u;
			}
			else
			{
				auto const subnormal_tail_mask{(::std::uint_least64_t{1u} << subnormal_shift) - 1u};
				bool const has_tail{(mantissa & subnormal_tail_mask) != 0u || product.low != 0u};
				mantissa >>= subnormal_shift;
				bool const is_tie{!has_tail && (mantissa & 1u) != 0u};
				mantissa =
					::fast_io::details::scan_decfloat_round_mantissa<rounding>(negative, mantissa, has_tail, is_tie);
			}
			answer.power2 = mantissa < (::std::uint_least64_t{1} << mantissa_explicit_bits) ? 0 : 1;
			answer.mantissa = mantissa;
			return true;
		}
		if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even)
		{
			if (product.low <= 1 &&
				::fast_io::details::scan_decfloat_min_round_to_even_power10<no_cvref_t>() <= exponent &&
				exponent <= ::fast_io::details::scan_decfloat_max_round_to_even_power10<no_cvref_t>() &&
				(mantissa & 3u) == 1u &&
				(mantissa << static_cast<unsigned>(shift)) == product.high)
			{
				mantissa &= ~::std::uint_least64_t{1};
			}
			mantissa += mantissa & 1u;
			mantissa >>= 1u;
		}
		else
		{
			auto const shifted_back{mantissa << static_cast<unsigned>(shift)};
			bool const is_tie{
				product.low <= 1 &&
				::fast_io::details::scan_decfloat_min_round_to_even_power10<no_cvref_t>() <= exponent &&
				exponent <= ::fast_io::details::scan_decfloat_max_round_to_even_power10<no_cvref_t>() &&
				(mantissa & 1u) != 0u && shifted_back == product.high};
			bool const has_tail{shifted_back != product.high || product.low != 0u};
			mantissa =
				::fast_io::details::scan_decfloat_round_mantissa<rounding>(negative, mantissa, has_tail, is_tie);
		}
		if (mantissa >= (::std::uint_least64_t{2} << mantissa_explicit_bits))
		{
			mantissa = ::std::uint_least64_t{1} << mantissa_explicit_bits;
			++power2;
		}
		mantissa &= ~(::std::uint_least64_t{1} << mantissa_explicit_bits);
		if (power2 >= infinite_power)
		{
			if constexpr (!::fast_io::details::scan_decfloat_nearest_rounding<rounding>)
			{
				if (!::fast_io::details::scan_decfloat_directed_round_up<rounding>(negative))
				{
					answer = {.mantissa = max_finite_mantissa, .power2 = infinite_power - 1};
					return true;
				}
			}
			answer = {.mantissa = 0, .power2 = infinite_power};
			return true;
		}
		answer = {.mantissa = mantissa, .power2 = power2};
		return true;
	}
}

template <typename T>
inline constexpr void scan_decfloat_to_float(bool negative, scan_decfloat_adjusted_mantissa adjusted,
											 T &value) noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	using mantissa_type = typename trait::mantissa_type;
	auto word{static_cast<mantissa_type>(
		static_cast<mantissa_type>(adjusted.mantissa) |
		(static_cast<mantissa_type>(adjusted.power2) << trait::mbits))};
	if (negative)
	{
		word |= static_cast<mantissa_type>(static_cast<mantissa_type>(1u) << (trait::mbits + trait::ebits));
	}
	value = ::fast_io::bit_cast<no_cvref_t>(word);
}

inline constexpr ::std::int_least64_t scan_decfloat_saturating_add(::std::int_least64_t a,
																   ::std::int_least64_t b) noexcept
{
	constexpr auto int64_max{(::std::numeric_limits<::std::int_least64_t>::max)()};
	constexpr auto int64_min{(::std::numeric_limits<::std::int_least64_t>::min)()};
	if (b > 0 && a > int64_max - b)
	{
		return int64_max;
	}
	if (b < 0 && a < int64_min - b)
	{
		return int64_min;
	}
	return a + b;
}

template <typename T>
[[nodiscard]] inline constexpr ::std::int_least64_t
scan_decfloat_adjusted_exponent(scan_decfloat_significand_state<T> const &state,
								::std::int_least64_t exponent) noexcept
{
	auto adjusted_exponent{::fast_io::details::scan_decfloat_saturating_add(
		exponent, -static_cast<::std::int_least64_t>(state.fractional_digits))};
	return ::fast_io::details::scan_decfloat_saturating_add(
		adjusted_exponent,
		static_cast<::std::int_least64_t>(state.significant_digits - state.stored_digits));
}

/**
 * @brief Rounds a long decimal prefix to binary80 when its entire uncertainty interval rounds alike.
 * @details Append up to eight more stored digits to the scanner's 19-digit coefficient P. After
 * adjusting the decimal exponent, the full positive coefficient is P if complete, or lies in
 * [P,P+1) otherwise. Left-normalizing P by s bits scales that uncertainty to less than 2^s.
 * The top half of its 128-by-128 product uses the high-limb product and the top halves of both
 * cross products. Discarding their two low halves and the low-limb product costs less than three
 * units. The normalized power-of-ten cache contributes at most one more. A final one-bit shift
 * doubles the bound. Decline at either side of an integer or half-integer boundary within that
 * radius. Outside it the retained integer, midpoint comparison and nonzero remainder are all
 * determined, including for directed rounding. An ambiguous prefix leaves the exact parser its
 * original digits, sign and exponent. No hardware floating-point operation participates.
 */
template <::fast_io::manipulators::floating_rounding rounding, typename T>
[[nodiscard]] inline constexpr bool scan_decfloat_try_x87_long(T &value, bool negative,
	scan_decfloat_significand_state<T> const &state, ::std::int_least64_t decimal_exponent) noexcept
{
	using word = ::std::uint_least64_t;
	constexpr auto prefix_size{scan_decfloat_uint64_significand_digit_limit};
	if (state.stored_digits != prefix_size || state.exact_stored_digits <= prefix_size)
	{
		return false;
	}
	auto const available{static_cast<unsigned>(state.exact_stored_digits - prefix_size)};
	auto const extra{available < 8u ? available : 8u};
	auto const e{scan_decfloat_saturating_add(decimal_exponent, available - extra)};
	if (e < scan_decfloat_dragonbox_min_power10 || e > scan_decfloat_dragonbox_max_power10)
	{
		return false;
	}
	::std::uint_least32_t suffix{};
	for (unsigned i{}; i != extra; ++i)
	{
		suffix = suffix * 10u + state.exact_digits[prefix_size + i];
	}
	word upper{};
	auto lower{::fast_io::intrinsics::umul(state.significand, scan_decfloat_pow10_0_to_8_table[extra], upper)};
	lower += suffix;
	upper += static_cast<word>(lower < suffix);
	bool const omitted{available > extra || state.exact_truncated_nonzero};
	if (!upper)
	{
		return !omitted && scan_decfloat_try_x87_cached<rounding>(value, negative, lower, e);
	}
	auto const shift{::std::countl_zero(upper)};
	// A wide interval cannot prove the residue on this scale. This guard also bounds the shift below.
	if (omitted && shift >= 60)
	{
		return false;
	}
	word const radius{8u + (omitted ? word{1u} << (shift + 1) : 0u)};
	if (shift)
	{
		upper = (upper << shift) | (lower >> (64 - shift));
		lower <<= shift;
	}
	auto const exponent{static_cast<::std::int_least32_t>(e)};
	auto const cache{compute_pow10_float64_scan(exponent)};
	auto product{scan_decfloat_mul_64x128_high(upper, cache)};
	auto const cross{::fast_io::intrinsics::umulh(lower, cache.hi)};
	product.low += cross;
	product.high += static_cast<word>(product.low < cross);
	constexpr word half{word{1u} << 63u};
	auto power{scan_decfloat_binary_power(exponent) + 64 - shift};
	if (product.high & half)
	{
		++power;
	}
	else
	{
		product.high = (product.high << 1u) | (product.low >> 63u);
		product.low <<= 1u;
	}
	auto const distance{product.low & (half - 1u)};
	if (distance <= radius || distance >= half - radius)
	{
		return false;
	}
	bool const up{scan_decfloat_nearest_rounding<rounding> ? (product.low & half) != 0u :
		scan_decfloat_directed_round_up<rounding>(negative)};
	if (up && !++product.high)
	{
		product.high = half;
		++power;
	}
	fp_assign_float80_bits(value, product.high, static_cast<::std::uint_least32_t>(power + 16383), negative);
	return true;
}

	// The exact midpoint comparator needs arbitrary-precision multiplication but
	// not a native 128-bit scalar.  Keep its limb layer available on every target:
	// intrinsics::umul supplies the complete 64-by-64 product as two words, and
	// the explicit carry recurrence below is mathematically identical to adding
	// the incoming carry to a native-u128 product.
/*
12000 decimal digits occupy at most

  ceil(12000*log2(10)/64) = 623

64-bit limbs.  A 640-limb allocation therefore represents the complete
retained significand.  At a binary128 rounding boundary the alternative
power-of-five construction is smaller: 5^16495 times a 114-bit midpoint
uses at most 601 limbs.  The decimal top-exponent guards reject values
outside the destination range before any larger scaling is attempted.
Binary64's 768 digits and 5^1075 times a 54-bit midpoint each need fewer
than 41 limbs.  Selecting 288 limbs for narrow types and 640 for
binary80/binary128 keeps the wide-format stack cost out of the narrow formats.
*/
template <typename T>
inline constexpr ::std::size_t scan_decfloat_bigint_limb_capacity{
	::fast_io::details::iec559_traits<::std::remove_cvref_t<T>>::ebits >= 15u ? 640u : 288u};

template <::std::size_t limb_capacity>
struct scan_decfloat_bigint
{
	::std::uint_least64_t limb[limb_capacity];
	::std::size_t size{};
};

template <typename T>
using scan_decfloat_bigint_for = ::fast_io::details::scan_decfloat_bigint<
	::fast_io::details::scan_decfloat_bigint_limb_capacity<T>>;

template <::std::size_t limb_capacity>
inline constexpr void scan_decfloat_bigint_clear(scan_decfloat_bigint<limb_capacity> &value) noexcept
{
	value.size = 0u;
}

template <::std::size_t limb_capacity>
inline constexpr void scan_decfloat_bigint_copy(scan_decfloat_bigint<limb_capacity> &out,
												scan_decfloat_bigint<limb_capacity> const &in) noexcept
{
	out.size = in.size;
	for (::std::size_t index{}; index != in.size; ++index)
	{
		out.limb[index] = in.limb[index];
	}
	}

	template <::std::size_t limb_capacity>
	inline constexpr void scan_decfloat_bigint_normalize(scan_decfloat_bigint<limb_capacity> &value) noexcept
	{
		for (; value.size && value.limb[value.size - 1u] == 0u; --value.size)
		{
		}
	}

	template <::std::size_t limb_capacity>
	inline constexpr void scan_decfloat_bigint_set_u64(scan_decfloat_bigint<limb_capacity> &value,
													   ::std::uint_least64_t word) noexcept
	{
		value.limb[0] = word;
		value.size = word == 0u ? 0u : 1u;
	}

	// Let B=2^64 and x*y=p_low+B*p_high.  For an incoming carry c<B,
	// (p_low+c) mod B is the next limb and p_high+[p_low+c>=B] is the next
	// carry.  Moreover x*y+c <= (B-1)^2+(B-1)=B^2-B, so that high-word
	// addition cannot overflow.  This proves exact equivalence to the former
	// native-u128 multiply-add recurrence.
	[[nodiscard]] inline constexpr ::std::uint_least64_t
	scan_decfloat_bigint_mul_add_carry(::std::uint_least64_t left,
									 ::std::uint_least64_t right,
									 ::std::uint_least64_t &carry) noexcept
	{
		::std::uint_least64_t product_high{};
		auto product_low{::fast_io::intrinsics::umul(left, right, product_high)};
		auto const low_before_carry{product_low};
		product_low = static_cast<::std::uint_least64_t>(product_low + carry);
		product_high = static_cast<::std::uint_least64_t>(
			product_high + static_cast<::std::uint_least64_t>(product_low < low_before_carry));
		carry = product_high;
		return product_low;
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_mul_small(scan_decfloat_bigint<limb_capacity> &value,
																	   ::std::uint_least32_t multiplier) noexcept
	{
		::std::uint_least64_t carry{};
		for (::std::size_t index{}; index != value.size; ++index)
		{
			value.limb[index] = ::fast_io::details::scan_decfloat_bigint_mul_add_carry(
				value.limb[index], static_cast<::std::uint_least64_t>(multiplier), carry);
		}
		if (carry)
		{
			if (value.size == limb_capacity)
			{
				return false;
			}
			value.limb[value.size] = static_cast<::std::uint_least64_t>(carry);
			++value.size;
		}
		return true;
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_mul_u64(scan_decfloat_bigint<limb_capacity> &value,
																	 ::std::uint_least64_t multiplier) noexcept
	{
		if (!multiplier || !value.size)
		{
			::fast_io::details::scan_decfloat_bigint_clear(value);
			return true;
		}
		::std::uint_least64_t carry{};
		for (::std::size_t index{}; index != value.size; ++index)
		{
			value.limb[index] = ::fast_io::details::scan_decfloat_bigint_mul_add_carry(
				value.limb[index], multiplier, carry);
		}
		if (carry)
		{
			if (value.size == limb_capacity)
			{
				return false;
			}
			value.limb[value.size] = static_cast<::std::uint_least64_t>(carry);
			++value.size;
		}
		return true;
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_add_small(scan_decfloat_bigint<limb_capacity> &value,
																	   ::std::uint_least32_t addend) noexcept
	{
		if (!value.size)
		{
			::fast_io::details::scan_decfloat_bigint_set_u64(value, addend);
			return true;
		}
		auto const first_before_add{value.limb[0]};
		value.limb[0] = static_cast<::std::uint_least64_t>(first_before_add + addend);
		bool carry{value.limb[0] < first_before_add};
		for (::std::size_t index{1u}; carry && index != value.size; ++index)
		{
			++value.limb[index];
			carry = value.limb[index] == 0u;
		}
		if (carry)
		{
			if (value.size == limb_capacity)
			{
				return false;
			}
			value.limb[value.size] = static_cast<::std::uint_least64_t>(carry);
			++value.size;
		}
		return true;
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_add_u64(scan_decfloat_bigint<limb_capacity> &value,
																	 ::std::uint_least64_t addend) noexcept
	{
		if (!addend)
		{
			return true;
		}
		if (!value.size)
		{
			::fast_io::details::scan_decfloat_bigint_set_u64(value, addend);
			return true;
		}
		auto const first_before_add{value.limb[0]};
		value.limb[0] = static_cast<::std::uint_least64_t>(first_before_add + addend);
		bool carry{value.limb[0] < first_before_add};
		for (::std::size_t index{1u}; carry && index != value.size; ++index)
		{
			++value.limb[index];
			carry = value.limb[index] == 0u;
		}
		if (carry)
		{
			if (value.size == limb_capacity)
			{
				return false;
			}
			value.limb[value.size] = static_cast<::std::uint_least64_t>(carry);
			++value.size;
		}
		return true;
	}

	template <::std::size_t limb_capacity, typename T>
	inline constexpr bool scan_decfloat_bigint_from_digits(scan_decfloat_bigint<limb_capacity> &value,
														   scan_decfloat_significand_state<T> const &state) noexcept
	{
		::fast_io::details::scan_decfloat_bigint_clear(value);
		constexpr ::std::uint_least64_t chunk_digits{19u};
		constexpr ::std::uint_least64_t chunk_power{10000000000000000000ull};
		::std::uint_least64_t chunk{};
		::std::uint_least64_t count{};
		for (::std::size_t index{}; index != state.exact_stored_digits; ++index)
		{
			chunk = chunk * 10u + static_cast<::std::uint_least64_t>(state.exact_digits[index]);
			++count;
			if (count == chunk_digits)
			{
				if (!::fast_io::details::scan_decfloat_bigint_mul_u64(value, chunk_power) ||
					!::fast_io::details::scan_decfloat_bigint_add_u64(value, chunk))
				{
					return false;
				}
				chunk = 0;
				count = 0;
			}
		}
		if (count)
		{
			if (!::fast_io::details::scan_decfloat_bigint_mul_u64(
					value, ::fast_io::details::print_rsv_fp_pow10_0_to_19_table[count]) ||
				!::fast_io::details::scan_decfloat_bigint_add_u64(value, chunk))
			{
				return false;
			}
		}
		return true;
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr ::std::size_t
	scan_decfloat_bigint_bit_width(scan_decfloat_bigint<limb_capacity> const &value) noexcept
	{
		if (!value.size)
		{
			return 0u;
		}
		return (value.size - 1u) * 64u +
			   static_cast<::std::size_t>(::std::bit_width(value.limb[value.size - 1u]));
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_get_bit(scan_decfloat_bigint<limb_capacity> const &value,
																	 ::std::size_t bit) noexcept
	{
		auto const limb_index{bit / 64u};
		if (limb_index >= value.size)
		{
			return false;
		}
		return ((value.limb[limb_index] >> (bit % 64u)) & 1u) != 0u;
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr bool
	scan_decfloat_bigint_any_bits_below(
		scan_decfloat_bigint<limb_capacity> const &value,
		::std::size_t bit_limit) noexcept
	{
		auto const whole_limbs{bit_limit / 64u};
		auto const checked_limbs{
			whole_limbs < value.size ? whole_limbs : value.size};
		for (::std::size_t index{}; index != checked_limbs; ++index)
		{
			if (value.limb[index] != 0u)
			{
				return true;
			}
		}
		auto const remaining_bits{bit_limit % 64u};
		if (remaining_bits && whole_limbs < value.size)
		{
			auto const mask{
				(::std::uint_least64_t{1u} << remaining_bits) - 1u};
			return (value.limb[whole_limbs] & mask) != 0u;
		}
		return false;
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr int scan_decfloat_bigint_compare(scan_decfloat_bigint<limb_capacity> const &left,
																	scan_decfloat_bigint<limb_capacity> const &right) noexcept
	{
		if (left.size != right.size)
		{
			return left.size < right.size ? -1 : 1;
		}
		for (auto index{left.size}; index != 0u;)
		{
			--index;
			if (left.limb[index] != right.limb[index])
			{
				return left.limb[index] < right.limb[index] ? -1 : 1;
			}
		}
		return 0;
	}

	template <::std::size_t limb_capacity>
	inline constexpr void scan_decfloat_bigint_sub_assign(scan_decfloat_bigint<limb_capacity> &left,
														  scan_decfloat_bigint<limb_capacity> const &right) noexcept
	{
		::std::uint_least64_t borrow{};
		for (::std::size_t index{}; index != left.size; ++index)
		{
			auto const subtrahend{index < right.size ? right.limb[index] : ::std::uint_least64_t{}};
			auto const old{left.limb[index]};
			auto const with_borrow{static_cast<::std::uint_least64_t>(subtrahend + borrow)};
			left.limb[index] = static_cast<::std::uint_least64_t>(old - with_borrow);
			borrow = old < with_borrow || (borrow && with_borrow == 0u);
		}
		::fast_io::details::scan_decfloat_bigint_normalize(left);
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_shl1_add_bit(scan_decfloat_bigint<limb_capacity> &value,
																		  bool bit) noexcept
	{
		::std::uint_least64_t carry{static_cast<::std::uint_least64_t>(bit)};
		for (::std::size_t index{}; index != value.size; ++index)
		{
			auto const next_carry{value.limb[index] >> 63u};
			value.limb[index] = static_cast<::std::uint_least64_t>((value.limb[index] << 1u) | carry);
			carry = next_carry;
		}
		if (carry)
		{
			if (value.size == limb_capacity)
			{
				return false;
			}
			value.limb[value.size] = carry;
			++value.size;
		}
		else if (bit && !value.size)
		{
			value.limb[0] = 1u;
			value.size = 1u;
		}
		return true;
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_shift_left(scan_decfloat_bigint<limb_capacity> &out,
																		scan_decfloat_bigint<limb_capacity> const &in,
																		::std::size_t shift) noexcept
	{
		::fast_io::details::scan_decfloat_bigint_clear(out);
		if (!in.size)
		{
			return true;
		}
		auto const limb_shift{shift / 64u};
		auto const bit_shift{shift % 64u};
		if (in.size + limb_shift + (bit_shift != 0u) >
			limb_capacity)
		{
			return false;
		}
		for (::std::size_t index{}; index != limb_shift; ++index)
		{
			out.limb[index] = 0u;
		}
		::std::uint_least64_t carry{};
		for (::std::size_t index{}; index != in.size; ++index)
		{
			auto const word{in.limb[index]};
			out.limb[index + limb_shift] = static_cast<::std::uint_least64_t>((word << bit_shift) | carry);
			carry = bit_shift == 0u ? 0u : static_cast<::std::uint_least64_t>(word >> (64u - bit_shift));
		}
		out.size = in.size + limb_shift;
		if (carry)
		{
			out.limb[out.size] = carry;
			++out.size;
		}
		::fast_io::details::scan_decfloat_bigint_normalize(out);
		return true;
	}

	template <::std::size_t limb_capacity>
	inline constexpr void
	scan_decfloat_bigint_shift_right_one_inplace(scan_decfloat_bigint<limb_capacity> &value) noexcept
	{
		::std::uint_least64_t carry{};
		for (auto index{value.size}; index != 0u;)
		{
			--index;
			auto const word{value.limb[index]};
			value.limb[index] = static_cast<::std::uint_least64_t>(
				(word >> 1u) | (carry << 63u));
			carry = word & 1u;
		}
		::fast_io::details::scan_decfloat_bigint_normalize(value);
	}

	template <::std::size_t limb_capacity>
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_shift_left_inplace(scan_decfloat_bigint<limb_capacity> &value,
																				::std::size_t shift) noexcept
	{
		if (!value.size || !shift)
		{
			return true;
		}
		auto const limb_shift{shift / 64u};
		auto const bit_shift{shift % 64u};
		if (value.size + limb_shift + (bit_shift != 0u) >
			limb_capacity)
		{
			return false;
		}
		if (limb_shift)
		{
			for (auto index{value.size}; index != 0u;)
			{
				--index;
				value.limb[index + limb_shift] = value.limb[index];
			}
			for (::std::size_t index{}; index != limb_shift; ++index)
			{
				value.limb[index] = 0u;
			}
			value.size += limb_shift;
		}
		if (bit_shift)
		{
			::std::uint_least64_t carry{};
			for (::std::size_t index{limb_shift}; index != value.size; ++index)
			{
				auto const word{value.limb[index]};
				value.limb[index] = static_cast<::std::uint_least64_t>((word << bit_shift) | carry);
				carry = static_cast<::std::uint_least64_t>(word >> (64u - bit_shift));
			}
			if (carry)
			{
				value.limb[value.size] = carry;
				++value.size;
			}
		}
		::fast_io::details::scan_decfloat_bigint_normalize(value);
		return true;
	}

	inline constexpr auto scan_decfloat_generate_pow5_0_to_27_table() noexcept
	{
		::fast_io::freestanding::array<::std::uint_least64_t, 28u> table{};
		table.index_unchecked(0u) = 1u;
		for (::std::size_t index{1u}; index != 28u; ++index)
		{
			table.index_unchecked(index) =
				static_cast<::std::uint_least64_t>(table.index_unchecked(index - 1u) * 5u);
		}
		return table;
	}

	// 5^27 = 7,450,580,596,923,828,125 < 2^63, so every multiplication in
	// this constant-initialization recurrence is representable in uint_least64_t.
	// Generation preserves the exact chunk constants without a handwritten table.
	inline constexpr auto scan_decfloat_pow5_0_to_27_table{
		::fast_io::details::scan_decfloat_generate_pow5_0_to_27_table()};

	// As in Dragonbox, d^-1*m mod 2^64 is the exact quotient precisely
	// when it does not exceed floor((2^64-1)/d). This same limit proves
	// that multiplying a coefficient by d cannot overflow.
	inline constexpr dragonbox_mod5_table_type<::std::uint_least64_t, uint64x2, 28u>
		scan_decfloat_mod5_table{};

	template <typename T>
	[[nodiscard]] inline constexpr bool
	scan_decfloat_try_exact_dyadic(T &value, bool negative,
								  ::std::uint_least64_t significand,
								  ::std::int_least64_t decimal_exponent) noexcept
	{
		using no_cvref_t = ::std::remove_cvref_t<T>;
		if constexpr (!::fast_io::details::scan_decfloat_compute_supported<no_cvref_t>)
		{
			/*
			The bit construction below targets the at-most-64-bit IEEE field
			model used by the cached converter.  Wide native formats retain their
			existing exact-bigint path; declining this optional shortcut changes
			neither their accepted grammar nor their rounding result.
			*/
			return false;
		}
		else
		{
			using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
			constexpr auto precision_bits{
				static_cast<unsigned>(trait::mbits + 1u)};
			constexpr auto exponent_bias{
				static_cast<::std::int_least32_t>(
					(static_cast<::std::uint_least32_t>(1u)
					 << (trait::ebits - 1u)) -
					1u)};
			constexpr auto minimum_normal_exponent{
				static_cast<::std::int_least32_t>(1 - exponent_bias)};
			constexpr auto maximum_normal_exponent{exponent_bias};

			/*
			M*10^e = (M*5^e)*2^e for e>=0 and
			M*10^-k = (M/5^k)*2^-k exactly when 5^k divides M.  Since
			5^27 fits in uint64_t and 5^28 does not, an unsigned 64-bit M
			can satisfy the negative divisibility case only for k<=27.
			The positive multiplication guard proves that its coefficient is
			also represented exactly.  No floating instruction or ambient
			rounding mode participates in either reduction.
			*/
			if (decimal_exponent < -27 || 27 < decimal_exponent)
			{
				return false;
			}
			::std::uint_least64_t coefficient{significand};
			auto binary_exponent{static_cast<::std::int_least32_t>(
				decimal_exponent)};
			if constexpr (scan_decfloat_binary64_like<T>)
			{
				auto const scale{static_cast<::std::size_t>(
					decimal_exponent < 0 ? -decimal_exponent : decimal_exponent)};
				auto const mod5{scan_decfloat_mod5_table.values[scale]};
				if (decimal_exponent < 0)
				{
					coefficient = static_cast<::std::uint_least64_t>(
						static_cast<::std::uintmax_t>(coefficient) * static_cast<::std::uintmax_t>(mod5.hi));
				}
				if (mod5.lo < coefficient)
				{
					return false;
				}
				if (decimal_exponent > 0)
				{
					coefficient *= scan_decfloat_pow5_0_to_27_table[scale];
				}
			}
			else
			{
				if (decimal_exponent < 0)
				{
					auto const power5{
						::fast_io::details::scan_decfloat_pow5_0_to_27_table[
							static_cast<::std::size_t>(-decimal_exponent)]};
					if (coefficient % power5 != 0u)
					{
						/*
						An uncancelled factor 5 remains in the denominator, so the
						rational is not dyadic and cannot be materialized by an
						exact binary shift.  The cached/exact rounding pipeline must
						decide between adjacent target values.
						*/
						return false;
					}
					coefficient /= power5;
				}
				else
				{
					auto const power5{
						::fast_io::details::scan_decfloat_pow5_0_to_27_table[
							static_cast<::std::size_t>(decimal_exponent)]};
					constexpr auto uint64_max{
						(::std::numeric_limits<::std::uint_least64_t>::max)()};
					if (coefficient > uint64_max / power5)
					{
						/*
						The exact coefficient does not fit the shortcut's carrier.
						Falling through preserves it in the existing 64x128 cached
						product rather than accepting a wrapped integer.
						*/
						return false;
					}
					coefficient *= power5;
				}
			}

			if (coefficient == 0u)
			{
				/*
				The caller normally handles zero before conversion.  Keeping the
				helper total avoids applying count-zero operations outside their
				nonzero domain without creating a second signed-zero policy.
				*/
				return false;
			}
			auto const trailing_zeroes{
				static_cast<unsigned>(::std::countr_zero(coefficient))};
			coefficient >>= trailing_zeroes;
			binary_exponent += static_cast<::std::int_least32_t>(
				trailing_zeroes);
			auto const coefficient_bits{static_cast<unsigned>(
				::std::numeric_limits<::std::uint_least64_t>::digits -
				::std::countl_zero(coefficient))};
			if (precision_bits < coefficient_bits)
			{
				/*
				After removing every factor two, an odd coefficient wider than
				the target precision cannot be represented exactly at any binary
				exponent.  All rounding policies therefore continue through the
				normal cached converter.
				*/
				return false;
			}

			auto const leading_exponent{
				static_cast<::std::int_least32_t>(
					binary_exponent +
					static_cast<::std::int_least32_t>(coefficient_bits) - 1)};
			::fast_io::details::scan_decfloat_adjusted_mantissa adjusted;
			if (minimum_normal_exponent <= leading_exponent &&
				leading_exponent <= maximum_normal_exponent)
			{
				/*
				Shifting the odd coefficient to precision_bits places its leading
				one in the implicit-bit position and appends only zero bits.
				Removing that implicit bit and adding the IEEE bias therefore
				encodes coefficient*2^binary_exponent exactly.
				*/
				auto const normalized{
					coefficient << (precision_bits - coefficient_bits)};
				auto const implicit_bit{
					::std::uint_least64_t{1u} << trait::mbits};
				adjusted.mantissa = normalized & (implicit_bit - 1u);
				adjusted.power2 = leading_exponent + exponent_bias;
			}
			else if (leading_exponent < minimum_normal_exponent)
			{
				constexpr auto subnormal_unit_exponent{
					static_cast<::std::int_least32_t>(
						minimum_normal_exponent -
						static_cast<::std::int_least32_t>(trait::mbits))};
				auto const subnormal_shift{
					static_cast<::std::int_least32_t>(
						binary_exponent - subnormal_unit_exponent)};
				if (subnormal_shift < 0 ||
					static_cast<unsigned>(subnormal_shift) >=
						::std::numeric_limits<::std::uint_least64_t>::digits ||
					trait::mbits <
						coefficient_bits +
							static_cast<unsigned>(subnormal_shift))
				{
					/*
					A negative shift leaves a fractional subnormal quantum; an
					oversized shift leaves the finite subnormal field.  In both
					cases the exact dyadic is not a target value, so rounding
					must remain in the general path.
					*/
					return false;
				}
				adjusted.mantissa =
					coefficient << static_cast<unsigned>(subnormal_shift);
				adjusted.power2 = 0;
			}
			else
			{
				/*
				The exact dyadic lies above the largest finite binade.  Its
				infinity-versus-max-finite result depends on the requested
				rounding policy, which the general converter already implements.
				*/
				return false;
			}
			::fast_io::details::scan_decfloat_to_float(
				negative, adjusted, value);
			return true;
		}
	}

	inline constexpr ::std::uint_least64_t
		scan_decfloat_pow5_anchor_chunk_count{32u};
	inline constexpr ::std::uint_least64_t
		scan_decfloat_pow5_anchor_exponent{
			27u * scan_decfloat_pow5_anchor_chunk_count};

	template <::std::size_t limb_capacity>
	inline constexpr ::std::uint_least64_t
		scan_decfloat_pow5_anchor_maximum_exponent{
			limb_capacity >= 640u ? 5095u : 1075u};

	// Give each anchor its own constant evaluation. A frontend emulating 64x64
	// multiplication can exceed its default step budget if all six binary128
	// anchors are generated in a single evaluation.
	template <::std::size_t limb_capacity, ::std::size_t index>
	inline constexpr scan_decfloat_bigint<limb_capacity> scan_decfloat_pow5_anchor = []() constexpr {
		scan_decfloat_bigint<limb_capacity> value{};
		if constexpr (index == 0u)
		{
			scan_decfloat_bigint_set_u64(value, 1u);
		}
		else
		{
			scan_decfloat_bigint_copy(value, scan_decfloat_pow5_anchor<limb_capacity, index - 1u>);
			for (::std::size_t chunk{}; chunk != scan_decfloat_pow5_anchor_chunk_count; ++chunk)
			{
				(void)scan_decfloat_bigint_mul_u64(value, scan_decfloat_pow5_0_to_27_table[27u]);
			}
		}
		return value;
	}();

	template <::std::size_t limb_capacity>
	struct scan_decfloat_pow5_anchor_table
	{
		inline static constexpr ::std::size_t extent{
			static_cast<::std::size_t>(
				scan_decfloat_pow5_anchor_maximum_exponent<limb_capacity> /
				scan_decfloat_pow5_anchor_exponent) +
			1u};
		scan_decfloat_bigint<limb_capacity> values[extent];

		template <::std::size_t... indices>
		constexpr scan_decfloat_pow5_anchor_table(::std::index_sequence<indices...>) noexcept
			: values{scan_decfloat_pow5_anchor<limb_capacity, indices>...}
		{}

		constexpr scan_decfloat_pow5_anchor_table() noexcept
			: scan_decfloat_pow5_anchor_table(::std::make_index_sequence<extent>{})
		{}
	};

	template <::std::size_t limb_capacity>
	inline constexpr scan_decfloat_pow5_anchor_table<limb_capacity>
		scan_decfloat_pow5_anchor_table_instance{};

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_mul_pow5(scan_decfloat_bigint<limb_capacity> &value,
																	  ::std::uint_least64_t exponent) noexcept
	{
		/*
		A short decimal coefficient occupies one limb, but binary80/binary128
		can request nearly five thousand powers of five.  Starting from that
		coefficient and multiplying by 5^27 makes every early pass revisit an
		ever-growing accumulator.  The constexpr anchors store every 864th
		power; copying the nearest lower anchor, multiplying it once by the
		coefficient, and finishing at most 31 chunks removes most of that
		quadratic growth.  Six wide anchors occupy 30 KiB, and constexpr storage
		avoids a first-call initialization spike.  Multi-limb decimal prefixes
		retain the general exact recurrence.
		*/
		if (!::std::is_constant_evaluated() && value.size <= 1u &&
			exponent >= ::fast_io::details::scan_decfloat_pow5_anchor_exponent)
		{
			using table_type =
				::fast_io::details::scan_decfloat_pow5_anchor_table<limb_capacity>;
			auto const anchor_index_wide{
				exponent /
				::fast_io::details::scan_decfloat_pow5_anchor_exponent};
			if (anchor_index_wide <
				static_cast<::std::uint_least64_t>(table_type::extent))
			{
				// Compare in the exponent's full domain before narrowing. This ordering is required on 32-bit targets:
				// truncating an adversarial quotient first could make an out-of-range exponent name a valid anchor.
				auto const anchor_index{
					static_cast<::std::size_t>(anchor_index_wide)};
				auto const coefficient{
					value.size ? value.limb[0u] : ::std::uint_least64_t{}};
				::fast_io::details::scan_decfloat_bigint_copy(
					value,
					::fast_io::details::
						scan_decfloat_pow5_anchor_table_instance<limb_capacity>
							.values[anchor_index]);
				if (!::fast_io::details::scan_decfloat_bigint_mul_u64(
						value, coefficient))
				{
					return false;
				}
				exponent %=
					::fast_io::details::scan_decfloat_pow5_anchor_exponent;
			}
		}
		constexpr ::std::uint_least64_t chunk{27u};
		for (; exponent >= chunk; exponent -= chunk)
		{
			if (!::fast_io::details::scan_decfloat_bigint_mul_u64(
					value, ::fast_io::details::scan_decfloat_pow5_0_to_27_table[chunk]))
			{
				return false;
			}
		}
		if (exponent != 0)
		{
			// The loop exit invariant is `0 < exponent < chunk`, and chunk is 27. The explicit conversion therefore
			// preserves the complete index on every supported size_t width and documents the table-bound proof.
			auto const tail_index{static_cast<::std::size_t>(exponent)};
			if (!::fast_io::details::scan_decfloat_bigint_mul_u64(
					value, ::fast_io::details::scan_decfloat_pow5_0_to_27_table[tail_index]))
			{
				return false;
			}
		}
		return true;
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_pow5(scan_decfloat_bigint<limb_capacity> &value,
																  ::std::uint_least64_t exponent) noexcept
	{
		::fast_io::details::scan_decfloat_bigint_set_u64(value, 1u);
		return ::fast_io::details::scan_decfloat_bigint_mul_pow5(value, exponent);
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr ::std::int_least64_t
	scan_decfloat_bigint_floor_log2_ratio(scan_decfloat_bigint<limb_capacity> const &numerator,
										  scan_decfloat_bigint<limb_capacity> const &denominator) noexcept
	{
		auto const numerator_bits{::fast_io::details::scan_decfloat_bigint_bit_width(numerator)};
		auto const denominator_bits{::fast_io::details::scan_decfloat_bigint_bit_width(denominator)};
		auto exponent{static_cast<::std::int_least64_t>(numerator_bits) -
					  static_cast<::std::int_least64_t>(denominator_bits)};
		::fast_io::details::scan_decfloat_bigint<limb_capacity> shifted;
		if (exponent >= 0)
		{
			(void)::fast_io::details::scan_decfloat_bigint_shift_left(
				shifted, denominator, static_cast<::std::size_t>(exponent));
			if (::fast_io::details::scan_decfloat_bigint_compare(numerator, shifted) < 0)
			{
				--exponent;
			}
		}
		else
		{
			(void)::fast_io::details::scan_decfloat_bigint_shift_left(
				shifted, numerator, static_cast<::std::size_t>(-exponent));
			if (::fast_io::details::scan_decfloat_bigint_compare(shifted, denominator) < 0)
			{
				--exponent;
			}
		}
		return exponent;
	}

	/*
	Exact division returns at most p target significand bits.  Binary128 and
	IBM double-double need the native 128-bit carrier.  For binary80 a 64-bit
	carrier holds the complete quotient.  If rounding a normal result produces
	2^64, the resulting zero indicates a carry into the next binade.
	*/
#if defined(__SIZEOF_INT128__)
	using scan_decfloat_quotient_type = __uint128_t;
#else
	using scan_decfloat_quotient_type = ::std::uint_least64_t;
#endif
	inline constexpr ::std::size_t scan_decfloat_quotient_bits{
		::std::numeric_limits<scan_decfloat_quotient_type>::digits};

	struct scan_decfloat_bigint_div_result
	{
		scan_decfloat_quotient_type quotient{};
		int twice_remainder_compare{};
		bool remainder_nonzero{};
		bool quotient_overflow{};
	};

#if defined(__SIZEOF_INT128__)
	/**
	 * @brief Divides a scaled nonnegative numerator by a nonzero denominator one radix-2^64 digit at a time.
	 * @details Both operands have one spare limb. Shifting them together until the divisor's high bit is set
	 * preserves the quotient and the ordering of twice the remainder against the divisor. Before each digit
	 * the high n words of the active n+1 word window represent a remainder smaller than the divisor.
	 * Dividing its leading pair by the high divisor word yields an upper estimate, capped at B-1 for B=2^64.
	 * Normalization bounds the estimate error by two. Testing the next divisor word removes every excess
	 * except a possible one. A borrow after the full subtraction detects that last case and a single
	 * add-back yields the exact remainder. Each iteration appends one exact digit to the quotient.
	 * @param dividend Scratch numerator with initialized low limbs and one spare word.
	 * @param divisor Scratch positive divisor with initialized low limbs and one spare word.
	 * @return Low quotient bits, overflow status, midpoint comparison and an exact nonzero-remainder flag.
	 */
	template <::std::size_t capacity>
	[[nodiscard]] inline constexpr scan_decfloat_bigint_div_result
	scan_decfloat_divide_words(scan_decfloat_bigint<capacity> &dividend, scan_decfloat_bigint<capacity> &divisor) noexcept
	{
		using word = ::std::uint_least64_t;
		using full = scan_decfloat_quotient_type;
		auto const n{divisor.size};
		auto const length{dividend.size};
		scan_decfloat_bigint_div_result result{};
		if (length >= n)
		{
			if (n == 1u)
			{
				// A one-word divisor needs only the remainder from the preceding digit.
				word rest{};
				for (auto i{length}; i--;)
				{
					auto const part{::fast_io::intrinsics::udivbigbysmalltosmalldefault(rest, dividend.limb[i], divisor.limb[0])};
					rest = part.remainder;
					if (i < 2u)
					{
						result.quotient |= static_cast<full>(part.quotient) << (i * 64u);
					}
					else if (part.quotient)
					{
						result.quotient_overflow = true;
					}
				}
				auto const other{divisor.limb[0] - rest};
				result.twice_remainder_compare = (rest > other) - (rest < other);
				result.remainder_nonzero = rest != 0u;
				return result;
			}
			auto const shift{static_cast<unsigned>(::std::countl_zero(divisor.limb[n - 1u]))};
			dividend.limb[length] = 0u;
			if (shift)
			{
				// The divisor's width stays n. The extra dividend word absorbs its top carry.
				(void)scan_decfloat_bigint_shift_left_inplace(divisor, shift);
				(void)scan_decfloat_bigint_shift_left_inplace(dividend, shift);
			}
			auto const v1{divisor.limb[n - 1u]}, v0{divisor.limb[n - 2u]};
			for (auto windows{length - n + 1u}; windows--;)
			{
				auto *u{dividend.limb + windows};
				auto const top{u[n]}, second{u[n - 1u]};
				word digit, rest;
				bool rest_carry{};
				if (top == v1)
				{
					// The trial digit would reach B. Saturating it adds v1 to the trial remainder.
					digit = (::std::numeric_limits<word>::max)();
					rest = second + v1;
					rest_carry = rest < second;
				}
				else
				{
					auto const part{::fast_io::intrinsics::udivbigbysmalltosmalldefault(top, second, v1)};
					digit = part.quotient;
					rest = part.remainder;
				}
				while (!rest_carry && static_cast<full>(digit) * v0 > ((static_cast<full>(rest) << 64u) | u[n - 2u]))
				{
					--digit;
					rest += v1;
					rest_carry = rest < v1;
				}
				word borrow{};
				for (::std::size_t i{}; i != n; ++i)
				{
					auto const product{static_cast<full>(digit) * divisor.limb[i] + borrow};
					auto const low{static_cast<word>(product)}, old{u[i]};
					u[i] = old - low;
					borrow = static_cast<word>(product >> 64u) + (old < low);
				}
				if (top < borrow)
				{
					// The low n words represent a negative residual modulo B^n.
					--digit;
					bool carry{};
					for (::std::size_t i{}; i != n; ++i)
					{
						u[i] = ::fast_io::intrinsics::addc(u[i], divisor.limb[i], carry, carry);
					}
				}
				if (windows < 2u)
				{
					result.quotient |= static_cast<full>(digit) << (windows * 64u);
				}
				else if (digit)
				{
					result.quotient_overflow = true;
				}
			}
			dividend.size = n;
			scan_decfloat_bigint_normalize(dividend);
		}
		result.remainder_nonzero = dividend.size != 0u;
		// The spare limb holds a carry from doubling even for a high-bit remainder.
		(void)scan_decfloat_bigint_shl1_add_bit(dividend, false);
		result.twice_remainder_compare = scan_decfloat_bigint_compare(dividend, divisor);
		return result;
	}
#endif

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr scan_decfloat_bigint_div_result
	scan_decfloat_bigint_div_shifted_to_quotient(
		scan_decfloat_bigint<limb_capacity> const &numerator,
		scan_decfloat_bigint<limb_capacity> const &denominator,
		::std::int_least64_t binary_shift) noexcept
	{
		/*
		The quotient requested by assign_big has only p or p+1 significant
		bits, but the old restoring division visited every bit of the scaled
		dividend.  That is about 16,000 iterations for a finite binary128 value
		near either decimal exponent limit.

		Materialize the power-of-two scale on one side, align the divisor once,
		then extract only the actual quotient bits.  Each subtraction leaves the
		exact remainder, so the final midpoint comparison and every directed
		rounding policy remain unchanged.  The loop is bounded by the quotient
		width (at most 114 useful bits for binary128), independently of the
		decimal exponent.
		*/
		if (denominator.size == 1u && denominator.limb[0] == 1u)
		{
			/*
			Positive decimal exponents leave the exact denominator equal to
			one.  After scale selection their effective divisor is normally a
			large power of two.  Extracting the quotient and classifying the
			discarded low bits is exact and touches each relevant limb once;
			constructing and repeatedly shifting a 5,000-digit aligned divisor
			would be pure overhead.
			*/
			auto const numerator_bits{
				::fast_io::details::scan_decfloat_bigint_bit_width(numerator)};
			scan_decfloat_quotient_type quotient{};
			bool quotient_overflow{};
			if (binary_shift >= 0)
			{
				// A fitting nonzero result uses at most one carrier's worth of source words.
				auto const left_shift{static_cast<::std::uint_least64_t>(binary_shift)};
				if (numerator_bits > ::fast_io::details::scan_decfloat_quotient_bits ||
					(numerator_bits && left_shift > ::fast_io::details::scan_decfloat_quotient_bits - numerator_bits))
				{
					quotient_overflow = true;
				}
				else if (numerator_bits)
				{
					quotient = numerator.limb[0];
#if defined(__SIZEOF_INT128__)
					if (numerator.size > 1u)
					{
						quotient |= static_cast<scan_decfloat_quotient_type>(numerator.limb[1]) << 64u;
					}
#endif
					quotient <<= left_shift;
				}
				return {.quotient = quotient,
						.twice_remainder_compare = -1,
						.remainder_nonzero = false,
						.quotient_overflow = quotient_overflow};
			}

			// Unsigned subtraction also represents the magnitude of the most negative shift.
			auto const magnitude{::std::uint_least64_t{} - static_cast<::std::uint_least64_t>(binary_shift)};
			if (magnitude > numerator_bits)
			{
				// N < 2^(magnitude-1), hence the quotient is zero and the residual is below half.
				return {.twice_remainder_compare = -1, .remainder_nonzero = numerator_bits != 0u};
			}
			auto const right_shift{static_cast<::std::size_t>(magnitude)};
			auto const quotient_width{numerator_bits - right_shift};
			if (quotient_width > scan_decfloat_quotient_bits)
			{
				quotient_overflow = true;
			}
			else if (quotient_width)
			{
				// The result is the carrier-width bit window starting at right_shift in N.
				auto const first{right_shift / 64u};
				auto const offset{static_cast<unsigned>(right_shift % 64u)};
				quotient = numerator.limb[first];
#if defined(__SIZEOF_INT128__)
				if (first + 1u < numerator.size)
				{
					quotient |= static_cast<scan_decfloat_quotient_type>(numerator.limb[first + 1u]) << 64u;
				}
#endif
				quotient >>= offset;
				// An unaligned window may take its top bits from one additional source limb.
				auto const next{first + scan_decfloat_quotient_bits / 64u};
				if (offset && next < numerator.size)
				{
					quotient |= static_cast<scan_decfloat_quotient_type>(numerator.limb[next]) <<
						(scan_decfloat_quotient_bits - offset);
				}
			}
			bool const remainder_nonzero{
				::fast_io::details::scan_decfloat_bigint_any_bits_below(
					numerator, right_shift)};
			int twice_remainder_compare{-1};
			if (right_shift &&
				::fast_io::details::scan_decfloat_bigint_get_bit(
					numerator, right_shift - 1u))
			{
				twice_remainder_compare =
					::fast_io::details::scan_decfloat_bigint_any_bits_below(
						numerator, right_shift - 1u)
						? 1
						: 0;
			}
			return {.quotient = quotient,
					.twice_remainder_compare = twice_remainder_compare,
					.remainder_nonzero = remainder_nonzero,
					.quotient_overflow = quotient_overflow};
		}

		::fast_io::details::scan_decfloat_bigint<limb_capacity> remainder;
		::fast_io::details::scan_decfloat_bigint<limb_capacity> divisor;
		if (binary_shift > 0)
		{
			if (!::fast_io::details::scan_decfloat_bigint_shift_left(
					remainder, numerator, static_cast<::std::size_t>(binary_shift)))
			{
				return {.quotient_overflow = true};
			}
		}
		else
		{
			::fast_io::details::scan_decfloat_bigint_copy(remainder, numerator);
		}
		if (binary_shift < 0)
		{
			if (!::fast_io::details::scan_decfloat_bigint_shift_left(
					divisor, denominator, static_cast<::std::size_t>(-binary_shift)))
			{
				return {.quotient_overflow = true};
			}
		}
		else
		{
			::fast_io::details::scan_decfloat_bigint_copy(divisor, denominator);
		}

#if defined(__SIZEOF_INT128__)
		// Two initialized limbs fit the quotient carrier exactly. The scaled operands
		// already include binary_shift, so Euclidean division gives the same quotient
		// and remainder as the aligned subtraction below. Comparing R with D - R
		// orders 2R against D without overflowing, including an exact midpoint.
		if (remainder.size <= 2u && divisor.size && divisor.size <= 2u)
		{
			auto const dividend_low{remainder.size ? remainder.limb[0] : 0u};
			auto const dividend_high{remainder.size == 2u ? remainder.limb[1] : 0u};
			auto const divisor_high{divisor.size == 2u ? divisor.limb[1] : 0u};
			auto const parts{::fast_io::intrinsics::udivmod(dividend_low, dividend_high, divisor.limb[0], divisor_high)};
			auto const join = [](::std::uint_least64_t low, ::std::uint_least64_t high) constexpr noexcept {
				return (static_cast<scan_decfloat_quotient_type>(high) << 64u) | low;
			};
			auto const residue{join(parts.remainderlow, parts.remainderhigh)};
			auto const complement{join(divisor.limb[0], divisor_high) - residue};
			return {.quotient = join(parts.quotientlow, parts.quotienthigh),
					.twice_remainder_compare = (residue > complement) - (residue < complement),
					.remainder_nonzero = residue != 0u};
		}

		// Normalized word division needs one scratch limb above each occupied range.
		if (remainder.size < limb_capacity && divisor.size && divisor.size < limb_capacity)
		{
			return scan_decfloat_divide_words(remainder, divisor);
		}
#endif

		scan_decfloat_quotient_type quotient{};
		bool quotient_overflow{};
		auto const remainder_bits{
			::fast_io::details::scan_decfloat_bigint_bit_width(remainder)};
		auto const divisor_bits{
			::fast_io::details::scan_decfloat_bigint_bit_width(divisor)};
		::fast_io::details::scan_decfloat_bigint<limb_capacity> aligned_divisor;
		if (remainder_bits >= divisor_bits && divisor_bits != 0u)
		{
			auto quotient_bit{remainder_bits - divisor_bits};
			if (!::fast_io::details::scan_decfloat_bigint_shift_left(
					aligned_divisor, divisor, quotient_bit))
			{
				return {.quotient_overflow = true};
			}
			bool quotient_nonzero_possible{true};
			if (::fast_io::details::scan_decfloat_bigint_compare(
					aligned_divisor, remainder) > 0)
			{
				if (quotient_bit == 0u)
				{
					quotient_nonzero_possible = false;
				}
				else
				{
					--quotient_bit;
					::fast_io::details::scan_decfloat_bigint_shift_right_one_inplace(
						aligned_divisor);
				}
			}
			for (; quotient_nonzero_possible;)
			{
				if (::fast_io::details::scan_decfloat_bigint_compare(
						remainder, aligned_divisor) >= 0)
				{
					::fast_io::details::scan_decfloat_bigint_sub_assign(
						remainder, aligned_divisor);
					if (quotient_bit < ::fast_io::details::scan_decfloat_quotient_bits)
					{
						quotient |= static_cast<scan_decfloat_quotient_type>(1u)
									<< quotient_bit;
					}
					else
					{
						quotient_overflow = true;
					}
				}
				if (quotient_bit == 0u)
				{
					break;
				}
				--quotient_bit;
				::fast_io::details::scan_decfloat_bigint_shift_right_one_inplace(
					aligned_divisor);
			}
		}
		::fast_io::details::scan_decfloat_bigint_copy(aligned_divisor, remainder);
		(void)::fast_io::details::scan_decfloat_bigint_shl1_add_bit(aligned_divisor, false);
		return {.quotient = quotient,
				.twice_remainder_compare =
					::fast_io::details::scan_decfloat_bigint_compare(aligned_divisor, divisor),
				.remainder_nonzero = remainder.size != 0u,
				.quotient_overflow = quotient_overflow};
	}

	template <::fast_io::manipulators::floating_rounding rounding>
	[[nodiscard]] inline constexpr bool scan_decfloat_big_round_up(
		bool negative, scan_decfloat_quotient_type quotient,
		int twice_remainder_compare,
		bool remainder_nonzero,
		bool tail_nonzero) noexcept
	{
		if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
		{
			/*
			For the retained lower endpoint let the scaled magnitude be q+r/d,
			0<=r<d.  Comparing 2r with d orders that endpoint against q+1/2.
			The type-dependent prefix contains the complete terminating decimal
			expansion of every binary midpoint.  Hence, if 2r<d, no omitted suffix
			can reach the midpoint: any first differing midpoint digit was already
			retained.  If 2r=d, a nonzero suffix is strictly above equality; if it
			is zero, the input is the exact tie.  Finally 2r>d is already above.
			Consequently every nearest policy returns q below the midpoint and q+1
			above it.  Only exact equality may consult the policy's tie rule.
			Treating every inexact nearest-to-odd input as "make q odd" would instead
			implement round-to-odd jamming and can select the farther neighbour.
			*/
			if (twice_remainder_compare < 0)
			{
				return false;
			}
			if (twice_remainder_compare > 0 || tail_nonzero)
			{
				return true;
			}
			if (!remainder_nonzero)
			{
				return false;
			}
			return ::fast_io::details::floating_rounding_nearest_tie_round_up<rounding>(
				negative, static_cast<::std::uint_least64_t>(quotient) << 1u);
		}
		else
		{
			return (remainder_nonzero || tail_nonzero) &&
				   ::fast_io::details::floating_rounding_directed_round_up<rounding>(negative);
		}
	}

	template <typename T>
	[[nodiscard]] inline constexpr ::std::uint_least64_t
	scan_decfloat_adjusted_significand(scan_decfloat_adjusted_mantissa adjusted) noexcept
	{
		using no_cvref_t = ::std::remove_cvref_t<T>;
		using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
		if (adjusted.power2 == 0)
		{
			return adjusted.mantissa;
		}
		return adjusted.mantissa | (::std::uint_least64_t{1u} << trait::mbits);
	}

	struct scan_decfloat_extended_mantissa
	{
		::std::uint_least64_t mantissa{};
		::std::int_least64_t power2{};
	};

	template <typename T>
	[[nodiscard]] inline constexpr scan_decfloat_extended_mantissa
	scan_decfloat_adjusted_to_extended(scan_decfloat_adjusted_mantissa adjusted) noexcept
	{
		using no_cvref_t = ::std::remove_cvref_t<T>;
		using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
		constexpr auto bias{static_cast<::std::int_least64_t>(trait::mbits) -
							static_cast<::std::int_least64_t>(
								::fast_io::details::scan_decfloat_minimum_exponent<no_cvref_t>())};
		::fast_io::details::scan_decfloat_extended_mantissa extended;
		if (adjusted.power2 == 0)
		{
			extended.mantissa = adjusted.mantissa;
			extended.power2 = 1 - bias;
		}
		else
		{
			extended.mantissa =
				::fast_io::details::scan_decfloat_adjusted_significand<no_cvref_t>(adjusted);
			extended.power2 = static_cast<::std::int_least64_t>(adjusted.power2) - bias;
		}
		return extended;
	}

	template <typename T>
	[[nodiscard]] inline constexpr scan_decfloat_extended_mantissa
	scan_decfloat_adjusted_to_extended_halfway(scan_decfloat_adjusted_mantissa adjusted) noexcept
	{
		auto extended{
			::fast_io::details::scan_decfloat_adjusted_to_extended<T>(adjusted)};
		extended.mantissa = static_cast<::std::uint_least64_t>((extended.mantissa << 1u) | 1u);
		--extended.power2;
		return extended;
	}

	template <typename T>
	[[nodiscard]] inline constexpr bool
	scan_decfloat_compare_exact_to_extended(
		scan_decfloat_significand_state<T> const &state,
		::std::int_least64_t decimal_exponent,
		scan_decfloat_extended_mantissa boundary,
		int &order) noexcept
	{
		using no_cvref_t = ::std::remove_cvref_t<T>;
		if constexpr (!::fast_io::details::scan_decfloat_compute_supported<no_cvref_t>)
		{
			return false;
		}
		else
		{
			::fast_io::details::scan_decfloat_bigint_for<no_cvref_t> real_digits;
			if (!::fast_io::details::scan_decfloat_bigint_from_digits(real_digits, state))
			{
				return false;
			}
			::fast_io::details::scan_decfloat_bigint_for<no_cvref_t> theor_digits;
			::fast_io::details::scan_decfloat_bigint_set_u64(
				theor_digits, boundary.mantissa);
			if (decimal_exponent < 0)
			{
				if (!::fast_io::details::scan_decfloat_bigint_mul_pow5(
						theor_digits,
						static_cast<::std::uint_least64_t>(-decimal_exponent)))
				{
					return false;
				}
			}
			else if (decimal_exponent > 0)
			{
				if (!::fast_io::details::scan_decfloat_bigint_mul_pow5(
						real_digits,
						static_cast<::std::uint_least64_t>(decimal_exponent)))
				{
					return false;
				}
			}
			auto const pow2_exponent{boundary.power2 - decimal_exponent};
			if (pow2_exponent > 0)
			{
				if (!::fast_io::details::scan_decfloat_bigint_shift_left_inplace(
						theor_digits, static_cast<::std::size_t>(pow2_exponent)))
				{
					return false;
				}
			}
			else if (pow2_exponent < 0)
			{
				if (!::fast_io::details::scan_decfloat_bigint_shift_left_inplace(
						real_digits, static_cast<::std::size_t>(-pow2_exponent)))
				{
					return false;
				}
			}
			order = ::fast_io::details::scan_decfloat_bigint_compare(
				real_digits, theor_digits);
			if (order == 0 && state.exact_truncated_nonzero)
			{
				/*
				The stored decimal is the lower endpoint of the omitted suffix.
				A nonzero omitted digit makes the complete input strictly larger.
				The exact-digit capacity theorem guarantees that a terminating
				binary boundary cannot first differ beyond this prefix.
				*/
				order = 1;
			}
			return true;
		}
	}

	template <typename T, ::fast_io::manipulators::floating_rounding rounding>
	[[nodiscard]] inline constexpr bool
	scan_decfloat_try_interval_compare(T &value, bool negative,
									   scan_decfloat_significand_state<T> const &state,
									   ::std::int_least64_t decimal_exponent,
									   scan_decfloat_adjusted_mantissa lower,
									   scan_decfloat_adjusted_mantissa upper,
									   ::fast_io::parse_code &code) noexcept
	{
		using no_cvref_t = ::std::remove_cvref_t<T>;
		if constexpr (!::fast_io::details::scan_decfloat_compute_supported<no_cvref_t>)
		{
			return false;
		}
		else
		{
			int order{};
			::fast_io::details::scan_decfloat_adjusted_mantissa selected;
			if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
			{
				auto const halfway{
					::fast_io::details::scan_decfloat_adjusted_to_extended_halfway<no_cvref_t>(
						lower)};
				if (!::fast_io::details::scan_decfloat_compare_exact_to_extended(
						state, decimal_exponent, halfway, order))
				{
					return false;
				}
				auto const lower_significand{
					::fast_io::details::scan_decfloat_adjusted_significand<no_cvref_t>(
						lower)};
				bool const select_upper{
					order > 0 ||
					(order == 0 &&
					 ::fast_io::details::floating_rounding_nearest_tie_round_up<rounding>(
						 negative,
						 static_cast<::std::uint_least64_t>(
							 (lower_significand << 1u) | 1u)))};
				selected = select_upper ? upper : lower;
			}
			else
			{
				bool const round_up{
					::fast_io::details::floating_rounding_directed_round_up<rounding>(
						negative)};
				auto const boundary{
					::fast_io::details::scan_decfloat_adjusted_to_extended<no_cvref_t>(
						round_up ? lower : upper)};
				if (!::fast_io::details::scan_decfloat_compare_exact_to_extended(
						state, decimal_exponent, boundary, order))
				{
					return false;
				}
				/*
				For magnitude-up rounding, lower is the first representable
				value not below the decimal interval's lower endpoint; it remains
				the answer through exact equality.  For magnitude-down rounding,
				upper is the last representable value not above the interval's
				upper endpoint and wins from exact equality onward.
				*/
				selected = round_up ? (order > 0 ? upper : lower)
									: (order >= 0 ? upper : lower);
			}
			::fast_io::details::scan_decfloat_to_float(negative, selected, value);
			code = ((state.significand != 0 && selected.mantissa == 0 && selected.power2 == 0) ||
					selected.power2 == ::fast_io::details::scan_decfloat_infinite_power<no_cvref_t>())
					   ? ::fast_io::parse_code::overflow
					   : ::fast_io::parse_code::ok;
			return true;
		}
	}

	template <typename T>
	inline constexpr void scan_decfloat_assign_min_subnormal(T &value, bool negative) noexcept
	{
		using no_cvref_t = ::std::remove_cvref_t<T>;
		using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
		using mantissa_type = typename trait::mantissa_type;
		constexpr auto mbits{trait::mbits};
		constexpr auto ebits{trait::ebits};
		if constexpr (::fast_io::details::fp_floating_point_is_float80<no_cvref_t>)
		{
			::fast_io::details::fp_assign_float80_bits(value, 1u, 0u, negative);
		}
		else if constexpr (
			::fast_io::details::fp_floating_point_is_ibm_double_double<no_cvref_t>)
		{
#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
			(void)::fast_io::details::fp_assign_ibm_double_double_significand(
				value, 1u, -1074, negative);
#endif
		}
		else
		{
			mantissa_type bits{1u};
			if (negative)
			{
				bits |= static_cast<mantissa_type>(static_cast<mantissa_type>(1u) << (mbits + ebits));
			}
			::fast_io::details::fp_assign_bits(value, bits);
		}
	}

	template <typename T>
	inline constexpr void scan_decfloat_assign_max_finite(T &value, bool negative) noexcept
	{
		using no_cvref_t = ::std::remove_cvref_t<T>;
		using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
		using mantissa_type = typename trait::mantissa_type;
		constexpr auto mbits{trait::mbits};
		constexpr auto ebits{trait::ebits};
		if constexpr (::fast_io::details::fp_floating_point_is_float80<no_cvref_t>)
		{
			::fast_io::details::fp_assign_float80_bits(value, static_cast<::std::uint_least64_t>(~::std::uint_least64_t{}),
													   (static_cast<::std::uint_least32_t>(1u) << ebits) - 2u,
													   negative);
		}
		else if constexpr (
			::fast_io::details::fp_floating_point_is_ibm_double_double<no_cvref_t>)
		{
			value = negative
				? -(::std::numeric_limits<no_cvref_t>::max)()
				: (::std::numeric_limits<no_cvref_t>::max)();
		}
		else
		{
			constexpr mantissa_type exponent{(static_cast<mantissa_type>(1u) << ebits) - 2u};
			constexpr mantissa_type fraction{(static_cast<mantissa_type>(1u) << mbits) - 1u};
			mantissa_type bits{static_cast<mantissa_type>((exponent << mbits) | fraction)};
			if (negative)
			{
				bits |= static_cast<mantissa_type>(static_cast<mantissa_type>(1u) << (mbits + ebits));
			}
			::fast_io::details::fp_assign_bits(value, bits);
		}
	}

	template <::fast_io::manipulators::floating_rounding rounding, typename T>
	inline constexpr ::fast_io::parse_code scan_decfloat_assign_overflow_value(T &value, bool negative) noexcept
	{
		bool assign_max_finite{};
		if constexpr (rounding == ::fast_io::manipulators::floating_rounding::toward_zero)
		{
			assign_max_finite = true;
		}
		else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::toward_plus_infinity)
		{
			assign_max_finite = negative;
		}
		else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::toward_minus_infinity)
		{
			assign_max_finite = !negative;
		}
		if (assign_max_finite)
		{
			::fast_io::details::scan_decfloat_assign_max_finite(value, negative);
		}
		else
		{
			::fast_io::details::fp_assign_infinity(value, negative);
		}
		return ::fast_io::parse_code::overflow;
	}

#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
	// Round the high binary64 component to nearest even. Then form and round
	// the signed residual with the full exact decimal numerator and denominator.
	template <::fast_io::manipulators::floating_rounding rounding, typename T, ::std::size_t capacity>
	[[nodiscard]] inline constexpr bool scan_decfloat_try_ibm_components(
		T &value, bool negative, scan_decfloat_bigint<capacity> const &numerator,
		scan_decfloat_bigint<capacity> const &denominator,
		::std::int_least64_t adjustment, ::std::int_least64_t top_exponent, bool sticky) noexcept
	{
		using mode = ::fast_io::manipulators::floating_rounding;
		// Here the low component always rounds on the binary64 subnormal grid.
		if (top_exponent < -968 || 1023 < top_exponent)
		{
			return false;
		}
		auto const high_scale{top_exponent - 52};
		auto const high_division{scan_decfloat_bigint_div_shifted_to_quotient(
			numerator, denominator, adjustment - high_scale)};
		auto const high{static_cast<::std::uint_least64_t>(high_division.quotient) +
			static_cast<::std::uint_least64_t>(scan_decfloat_big_round_up<mode::nearest_to_even>(
				false, high_division.quotient, high_division.twice_remainder_compare,
				high_division.remainder_nonzero, sticky))};
		if (high_division.quotient_overflow || (top_exponent == 1023 && high == (UINT64_C(1) << 53u)))
		{
			return false;
		}
		// Express the input and the rounded high part on the same scale.
		scan_decfloat_bigint<capacity> scaled_numerator;
		scan_decfloat_bigint<capacity> scaled_denominator;
		auto const shift{adjustment - high_scale};
		if (!scan_decfloat_bigint_shift_left(scaled_numerator, numerator,
				static_cast<::std::size_t>(0 < shift ? shift : 0)) ||
			!scan_decfloat_bigint_shift_left(scaled_denominator, denominator,
				static_cast<::std::size_t>(shift < 0 ? -shift : 0)))
		{
			return false;
		}
		scan_decfloat_bigint<capacity> high_numerator;
		scan_decfloat_bigint_copy(high_numerator, scaled_denominator);
		if (!scan_decfloat_bigint_mul_u64(high_numerator, high))
		{
			return false;
		}
		auto const order{scan_decfloat_bigint_compare(scaled_numerator, high_numerator)};
		auto const high_part{::fast_io::details::ibm_double_double_make_binary64(
			high, static_cast<::std::int_least32_t>(high_scale), negative)};
		scan_decfloat_bigint_div_result low{};
		::std::int_least64_t low_scale{-1074};
		if (0 < order)
		{
			scan_decfloat_bigint_sub_assign(scaled_numerator, high_numerator);
		}
		else if (order < 0)
		{
			scan_decfloat_bigint_sub_assign(high_numerator, scaled_numerator);
		}
		if (order)
		{
			auto const &residual{0 < order ? scaled_numerator : high_numerator};
			auto const low_top{scan_decfloat_bigint_floor_log2_ratio(residual, scaled_denominator) + high_scale};
			low_scale = low_top < -1022 ? -1074 : low_top - 52;
			low = scan_decfloat_bigint_div_shifted_to_quotient(
				residual, scaled_denominator, high_scale - low_scale);
			if (low.quotient_overflow)
			{
				return false;
			}
		}
		else
		{
			// The omitted decimal tail is smaller than half the smallest
			// binary64 quantum because 1400 positions retain every midpoint.
			low.twice_remainder_compare = -1;
		}
		auto const opposite{order < 0};
		if (opposite && sticky)
		{
			// The omitted suffix reduces the magnitude of a negative residual.
			if (!low.remainder_nonzero)
			{
				// Below an exact power of two the grid may be one bit finer.
				if (low.quotient == (UINT64_C(1) << 52u) && -1074 < low_scale)
				{
					low.quotient <<= 1u;
					--low_scale;
				}
				--low.quotient;
				low.remainder_nonzero = true;
				low.twice_remainder_compare = 1;
			}
			else if (!low.twice_remainder_compare)
			{
				low.twice_remainder_compare = -1;
			}
		}
		auto const low_negative{negative != opposite};
		auto const tail_increases_magnitude{sticky && !opposite};
		auto round_residual = [&]<mode selected>() constexpr noexcept {
			return scan_decfloat_big_round_up<selected>(low_negative, low.quotient,
				low.twice_remainder_compare, low.remainder_nonzero, tail_increases_magnitude);
		};
		bool increment{};
		if constexpr (rounding == mode::toward_zero || rounding == mode::away_from_zero ||
					  rounding == mode::nearest_toward_zero || rounding == mode::nearest_away_from_zero)
		{
			// Toward and away from zero refer to the whole value. An
			// opposite-sign residual reverses those magnitude directions.
			constexpr mode opposite_rounding{
				rounding == mode::toward_zero ? mode::away_from_zero :
				rounding == mode::away_from_zero ? mode::toward_zero :
				rounding == mode::nearest_toward_zero ? mode::nearest_away_from_zero : mode::nearest_toward_zero};
			increment = opposite ? round_residual.template operator()<opposite_rounding>() :
								   round_residual.template operator()<rounding>();
		}
		else
		{
			increment = round_residual.template operator()<rounding>();
		}
		auto const low_significand{static_cast<::std::uint_least64_t>(low.quotient) +
			static_cast<::std::uint_least64_t>(increment)};
		auto const low_part{::fast_io::details::ibm_double_double_make_binary64(
			low_significand, static_cast<::std::int_least32_t>(low_scale), low_significand && low_negative)};
		if (top_exponent == 1023 && 0 < order && high == (UINT64_C(1) << 53u) - 1u)
		{
			auto const maximum{::fast_io::bit_cast<::fast_io::details::ibm_double_double_storage>(
				(::std::numeric_limits<T>::max)())};
			constexpr auto magnitude_mask{UINT64_C(0x7fffffffffffffff)};
			if (::fast_io::bit_cast<::std::uint_least64_t>(maximum.low) <
				(::fast_io::bit_cast<::std::uint_least64_t>(low_part) & magnitude_mask))
			{
				return false;
			}
		}
		::fast_io::details::fp_assign_ibm_double_double_components(value, high_part, low_part);
		return true;
	}

	template <::fast_io::manipulators::floating_rounding rounding, typename T>
	inline constexpr ::fast_io::parse_code scan_hexfloat_assign_ibm_result(
		T &value, bool negative, scan_hexfloat_significand_state<scan_hexfloat_ibm_storage> const &state,
		::std::int_least64_t binary_exponent) noexcept
	{
		using mode = ::fast_io::manipulators::floating_rounding;
		if constexpr (rounding == mode::current_environment)
		{
			switch (::fast_io::details::current_floating_rounding())
			{
			case mode::toward_plus_infinity:
				return scan_hexfloat_assign_ibm_result<mode::toward_plus_infinity>(
					value, negative, state, binary_exponent);
			case mode::toward_minus_infinity:
				return scan_hexfloat_assign_ibm_result<mode::toward_minus_infinity>(
					value, negative, state, binary_exponent);
			case mode::toward_zero:
				return scan_hexfloat_assign_ibm_result<mode::toward_zero>(
					value, negative, state, binary_exponent);
			default:
				return scan_hexfloat_assign_ibm_result<mode::nearest_to_even>(
					value, negative, state, binary_exponent);
			}
		}
		else
		{
			// One spare limb covers alignment of the rounded high part.
			constexpr auto capacity{scan_hexfloat_ibm_storage::word_count + 1u};
			scan_decfloat_bigint<capacity> numerator;
			auto const &words{state.stored.words};
			auto const word_count{(state.stored_hex_digits + 15u) / 16u};
			auto const padding{(word_count * 16u - state.stored_hex_digits) * 4u};
			numerator.size = word_count;
			for (::std::size_t i{}; i != word_count; ++i)
			{
				auto const word{word_count - 1u - i};
				numerator.limb[i] = words[word] >> padding;
				if (padding && word)
				{
					numerator.limb[i] |= words[word - 1u] << (64u - padding);
				}
			}
			scan_decfloat_bigint_normalize(numerator);
			auto scale_for = [&, binary_exponent](::std::size_t digits) constexpr noexcept {
				constexpr auto maximum{(::std::numeric_limits<::std::int_least64_t>::max)()};
				auto const omitted{state.significant_hex_digits - static_cast<::std::int_least64_t>(digits)};
				return ::fast_io::details::scan_decfloat_saturating_add(
					binary_exponent, omitted < maximum / 4 ? omitted * 4 : maximum);
			};
			auto const scale{scale_for(state.stored_hex_digits)};
			auto const top{::fast_io::details::scan_decfloat_saturating_add(
				scale, static_cast<::std::int_least64_t>(scan_decfloat_bigint_bit_width(numerator)) - 1)};
			if (-968 <= top && top <= 1023)
			{
				scan_decfloat_bigint<capacity> denominator;
				scan_decfloat_bigint_set_u64(denominator, 1u);
				if (scan_decfloat_try_ibm_components<rounding>(
						value, negative, numerator, denominator, scale, top, state.truncated_nonzero))
				{
					return ::fast_io::parse_code::ok;
				}
			}

			// On the subnormal grid and at the range endpoints the compact
			// converter needs only the original prefix and a sticky bit.
			constexpr auto compact_capacity{
				::fast_io::details::scan_hexfloat_storage_type<
					::fast_io::details::iec559_traits<T>::mbits + 1u>::stored_hex_digits_limit};
			auto const compact_digits{state.stored_hex_digits < compact_capacity
										  ? state.stored_hex_digits : compact_capacity};
			auto const prefix{(static_cast<__uint128_t>(words[0]) << 64u) | words[1]};
			auto const discard{128u - compact_digits * 4u};
			bool sticky{state.truncated_nonzero ||
						(prefix & ::fast_io::details::scan_hexfloat_low_mask<__uint128_t>(discard)) != 0u};
			for (::std::size_t word{2u}; !sticky && word < word_count; ++word)
			{
				sticky = words[word] != 0u;
			}
			return ::fast_io::details::scan_hexfloat_assign_ieee_result<T, __uint128_t, rounding>(
				value, negative, prefix >> discard, compact_digits,
				static_cast<::std::int_least64_t>(compact_digits), sticky, scale_for(compact_digits));
		}
	}
#endif

	/*
	Assigning an arbitrary decimal uses the exact limb numerator/denominator of
	its retained lower endpoint and the midpoint-capacity theorem proved at the
	state definition to classify any omitted suffix.  The resulting endpoint
	quotient/remainder plus sticky bit is therefore sufficient for the exact
	rounding decision.  The 64-bit quotient also supports binary80 on targets
	such as IA-32, with normal carry handled after rounding.
	*/
	template <typename T, ::fast_io::manipulators::floating_rounding rounding =
							  ::fast_io::manipulators::floating_rounding::nearest_to_even>
	[[nodiscard]] inline constexpr ::fast_io::parse_code
	scan_decfloat_assign_big(T &value, bool negative, scan_decfloat_significand_state<T> const &state,
							 ::std::int_least64_t exponent) noexcept
	{
		using no_cvref_t = ::std::remove_cvref_t<T>;
		using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
		using mantissa_type = typename trait::mantissa_type;
		constexpr ::std::size_t mbits{trait::mbits};
		constexpr ::std::size_t ebits{trait::ebits};
		constexpr ::std::size_t precision_bits{mbits + 1u};
		static_assert(precision_bits <= ::fast_io::details::scan_decfloat_quotient_bits);
		constexpr auto bias{
			static_cast<::std::int_least64_t>((static_cast<::std::uint_least32_t>(1u) << ebits) >> 1u) - 1};
		constexpr auto min_exponent{[]() constexpr noexcept
		{
			if constexpr (::fast_io::details::
				fp_floating_point_is_ibm_double_double<no_cvref_t>)
			{
				return static_cast<::std::int_least64_t>(
					::std::numeric_limits<no_cvref_t>::min_exponent - 1);
			}
			else
			{
				return static_cast<::std::int_least64_t>(1 - bias);
			}
		}()};
		constexpr auto max_exponent{[]() constexpr noexcept
		{
			if constexpr (::fast_io::details::
				fp_floating_point_is_ibm_double_double<no_cvref_t>)
			{
				return static_cast<::std::int_least64_t>(
					::std::numeric_limits<no_cvref_t>::max_exponent - 1);
			}
			else
			{
				/*
				For an IEC field with E exponent bits the bias is
				B=2^(E-1)-1 and the largest finite unbiased exponent is B.
				This is the exact inverse of the encoded exponent (2^E-2);
				it depends only on the representation already proved by
				scan_decfloat_layout_supported.

				Do not query numeric_limits here.  Compiler extension types may
				have a complete iec559_traits specialization while the selected
				C++ standard library does not specialize numeric_limits for that
				frontend.  In particular Clang with libstdc++ reports
				numeric_limits<__float128>::max_exponent==0, which would turn
				every value at exponent zero (for example 1.25) into a false
				overflow.  Substitution of B is identical for every standard
				specialization and keeps the arithmetic independent of frontend
				library declarations.
				*/
				return bias;
			}
		}()};
		if constexpr (rounding == ::fast_io::manipulators::floating_rounding::current_environment)
		{
			switch (::fast_io::details::current_floating_rounding())
			{
			case ::fast_io::manipulators::floating_rounding::toward_plus_infinity:
				return ::fast_io::details::scan_decfloat_assign_big<
					T, ::fast_io::manipulators::floating_rounding::toward_plus_infinity>(
					value, negative, state, exponent);
			case ::fast_io::manipulators::floating_rounding::toward_minus_infinity:
				return ::fast_io::details::scan_decfloat_assign_big<
					T, ::fast_io::manipulators::floating_rounding::toward_minus_infinity>(
					value, negative, state, exponent);
			case ::fast_io::manipulators::floating_rounding::toward_zero:
				return ::fast_io::details::scan_decfloat_assign_big<
					T, ::fast_io::manipulators::floating_rounding::toward_zero>(
					value, negative, state, exponent);
			default:
				return ::fast_io::details::scan_decfloat_assign_big<
					T, ::fast_io::manipulators::floating_rounding::nearest_to_even>(
					value, negative, state, exponent);
			}
		}
		else
		{
			if (!state.exact_stored_digits)
			{
				value = negative ? -static_cast<no_cvref_t>(0.0) : static_cast<no_cvref_t>(0.0);
				return ::fast_io::parse_code::ok;
			}
			auto decimal_exponent{::fast_io::details::scan_decfloat_saturating_add(
				exponent, -static_cast<::std::int_least64_t>(state.fractional_digits))};
			decimal_exponent = ::fast_io::details::scan_decfloat_saturating_add(
				decimal_exponent,
				static_cast<::std::int_least64_t>(state.significant_digits - state.exact_stored_digits));
			if constexpr (::fast_io::details::fp_floating_point_is_float80<no_cvref_t>)
			{
				if (scan_decfloat_try_x87_long<rounding>(value, negative, state, decimal_exponent))
				{
					return ::fast_io::parse_code::ok;
				}
			}
			auto const decimal_top_exponent{::fast_io::details::scan_decfloat_saturating_add(
				decimal_exponent, static_cast<::std::int_least64_t>(state.exact_stored_digits - 1u))};
			constexpr auto high_guard{static_cast<::std::int_least64_t>(trait::e10max + 2u)};
			constexpr auto low_guard{static_cast<::std::int_least64_t>(trait::e10max + trait::m10digits + 16u)};
			if (decimal_top_exponent > high_guard)
			{
				return ::fast_io::details::scan_decfloat_assign_overflow_value<rounding>(value, negative);
			}
			if (decimal_top_exponent < -low_guard)
			{
				if constexpr (!::fast_io::details::floating_rounding_is_nearest<rounding>)
				{
					if (state.has_nonzero_digit &&
						::fast_io::details::floating_rounding_directed_round_up<rounding>(negative))
					{
						::fast_io::details::scan_decfloat_assign_min_subnormal(value, negative);
						return ::fast_io::parse_code::overflow;
					}
				}
				value = negative ? -static_cast<no_cvref_t>(0.0) : static_cast<no_cvref_t>(0.0);
				return ::fast_io::parse_code::overflow;
			}

			::fast_io::details::scan_decfloat_bigint_for<no_cvref_t> numerator;
			if (!::fast_io::details::scan_decfloat_bigint_from_digits(numerator, state))
			{
				return ::fast_io::details::scan_decfloat_assign_overflow_value<rounding>(value, negative);
			}
			::fast_io::details::scan_decfloat_bigint_for<no_cvref_t> denominator;
			::fast_io::details::scan_decfloat_bigint_set_u64(denominator, 1u);
			::std::int_least64_t binary_exponent_adjust{};
			if (decimal_exponent >= 0)
			{
				if (!::fast_io::details::scan_decfloat_bigint_mul_pow5(
						numerator, static_cast<::std::uint_least64_t>(decimal_exponent)))
				{
					return ::fast_io::details::scan_decfloat_assign_overflow_value<rounding>(value, negative);
				}
				binary_exponent_adjust = decimal_exponent;
			}
			else
			{
				auto const pow5_exponent{static_cast<::std::uint_least64_t>(-decimal_exponent)};
				if (!::fast_io::details::scan_decfloat_bigint_pow5(denominator, pow5_exponent))
				{
					value = negative ? -static_cast<no_cvref_t>(0.0) : static_cast<no_cvref_t>(0.0);
					return ::fast_io::parse_code::overflow;
				}
				binary_exponent_adjust = decimal_exponent;
			}

			::std::int_least64_t binary_exponent{};
			if (decimal_exponent >= 0)
			{
				binary_exponent = static_cast<::std::int_least64_t>(
									  ::fast_io::details::scan_decfloat_bigint_bit_width(numerator)) -
								  1 + binary_exponent_adjust;
			}
			else
			{
				binary_exponent =
					::fast_io::details::scan_decfloat_bigint_floor_log2_ratio(numerator, denominator) +
					binary_exponent_adjust;
			}

#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
			if constexpr (::fast_io::details::fp_floating_point_is_ibm_double_double<no_cvref_t>)
			{
				if (scan_decfloat_try_ibm_components<rounding>(value, negative,
						numerator, denominator, binary_exponent_adjust, binary_exponent,
						state.exact_truncated_nonzero))
				{
					return ::fast_io::parse_code::ok;
				}
			}
#endif
			bool subnormal{};
			::std::int_least64_t target_exponent{binary_exponent};
			::std::int_least64_t scale_exponent{};
			if (binary_exponent >= min_exponent)
			{
				scale_exponent = binary_exponent - static_cast<::std::int_least64_t>(precision_bits - 1u);
			}
			else
			{
				subnormal = true;
				target_exponent = min_exponent;
				scale_exponent = min_exponent - static_cast<::std::int_least64_t>(precision_bits - 1u);
			}

			auto const division{
				::fast_io::details::scan_decfloat_bigint_div_shifted_to_quotient(
					numerator, denominator, binary_exponent_adjust - scale_exponent)};
			if (division.quotient_overflow)
			{
				return ::fast_io::details::scan_decfloat_assign_overflow_value<rounding>(value, negative);
			}
			auto significand{division.quotient};
			if (::fast_io::details::scan_decfloat_big_round_up<rounding>(
					negative, significand, division.twice_remainder_compare,
					division.remainder_nonzero, state.exact_truncated_nonzero))
			{
				++significand;
			}
			auto const hidden_bit{
				static_cast<::fast_io::details::scan_decfloat_quotient_type>(1u)
				<< mbits};
			if (!subnormal)
			{
				if constexpr (precision_bits == ::fast_io::details::scan_decfloat_quotient_bits)
				{
					// A normal quotient is nonzero before rounding. Only a carry can wrap it to zero.
					if (!significand)
					{
						significand = hidden_bit;
						++target_exponent;
					}
				}
				else if (significand >= (hidden_bit << 1u))
				{
					significand >>= 1u;
					++target_exponent;
				}
				if (target_exponent > max_exponent)
				{
					return ::fast_io::details::scan_decfloat_assign_overflow_value<rounding>(value, negative);
				}
				if constexpr (::fast_io::details::fp_floating_point_is_float80<no_cvref_t>)
				{
					::fast_io::details::fp_assign_float80_bits(
						value, static_cast<::std::uint_least64_t>(significand),
						static_cast<::std::uint_least32_t>(target_exponent + bias), negative);
				}
				else if constexpr (
					::fast_io::details::fp_floating_point_is_ibm_double_double<no_cvref_t>)
				{
#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
					/*
					Before carry, scale_exponent equals target_exponent-(p-1).
					If rounding produces 2^p, the branch above replaces it by
					2^(p-1) and increments target_exponent.  Reusing the old scale
					would therefore divide the result by two.  Deriving the dyadic
					scale from the final exponent preserves the invariant

					  value = significand * 2^(target_exponent-(p-1))

					in both the carry and no-carry cases; the component bridge then
					encodes exactly that already-rounded dyadic.
					*/
					if (!::fast_io::details::fp_assign_ibm_double_double_significand(
							value, static_cast<__uint128_t>(significand),
							static_cast<::std::int_least32_t>(target_exponent -
								static_cast<::std::int_least64_t>(mbits)), negative))
					{
						return ::fast_io::details::scan_decfloat_assign_overflow_value<rounding>(
							value, negative);
					}
#endif
				}
				else
				{
					auto fraction{static_cast<mantissa_type>(significand - hidden_bit)};
					auto bits{static_cast<mantissa_type>(
						(static_cast<mantissa_type>(static_cast<::std::uint_least64_t>(target_exponent + bias)) << mbits) |
						fraction)};
					if (negative)
					{
						bits |= static_cast<mantissa_type>(static_cast<mantissa_type>(1u) << (mbits + ebits));
					}
					::fast_io::details::fp_assign_bits(value, bits);
				}
				return ::fast_io::parse_code::ok;
			}
			if (!significand)
			{
				value = negative ? -static_cast<no_cvref_t>(0.0) : static_cast<no_cvref_t>(0.0);
				return ::fast_io::parse_code::overflow;
			}
			if (significand >= hidden_bit)
			{
				if constexpr (::fast_io::details::fp_floating_point_is_float80<no_cvref_t>)
				{
					::fast_io::details::fp_assign_float80_bits(value, static_cast<::std::uint_least64_t>(hidden_bit),
															   1u, negative);
				}
				else if constexpr (
					::fast_io::details::fp_floating_point_is_ibm_double_double<no_cvref_t>)
				{
#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
					(void)::fast_io::details::fp_assign_ibm_double_double_significand(
						value, static_cast<__uint128_t>(hidden_bit),
						static_cast<::std::int_least32_t>(scale_exponent), negative);
#endif
				}
				else
				{
					mantissa_type bits{static_cast<mantissa_type>(mantissa_type{1u} << mbits)};
					if (negative)
					{
						bits |= static_cast<mantissa_type>(static_cast<mantissa_type>(1u) << (mbits + ebits));
					}
					::fast_io::details::fp_assign_bits(value, bits);
				}
			}
			else if constexpr (::fast_io::details::fp_floating_point_is_float80<no_cvref_t>)
			{
				::fast_io::details::fp_assign_float80_bits(
					value, static_cast<::std::uint_least64_t>(significand), 0u, negative);
			}
			else if constexpr (
				::fast_io::details::fp_floating_point_is_ibm_double_double<no_cvref_t>)
			{
#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
				(void)::fast_io::details::fp_assign_ibm_double_double_significand(
					value, static_cast<__uint128_t>(significand),
					static_cast<::std::int_least32_t>(scale_exponent), negative);
#endif
			}
			else
			{
				auto bits{static_cast<mantissa_type>(significand)};
				if (negative)
				{
					bits |= static_cast<mantissa_type>(static_cast<mantissa_type>(1u) << (mbits + ebits));
				}
				::fast_io::details::fp_assign_bits(value, bits);
			}
			return ::fast_io::parse_code::ok;
		}
	}

	template <typename T>
	inline constexpr void scan_decfloat_state_from_u64(scan_decfloat_significand_state<T> &state,
													   ::std::uint_least64_t significand) noexcept
	{
		auto const original{significand};
		char8_t buffer[20];
		::std::size_t size{};
		for (; significand; significand /= 10u)
		{
			buffer[size] = static_cast<char8_t>(significand % 10u);
			++size;
		}
		state.has_digit = true;
		state.has_nonzero_digit = size != 0u;
		state.significant_digits = size;
		state.stored_digits = size;
		state.significand = original;
		for (auto index{size}; index != 0u;)
		{
			--index;
			::fast_io::details::scan_decfloat_append_exact_digit(state, buffer[index]);
		}
	}

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool
scan_decfloat_decimal_round_up(bool negative, ::std::uint_least64_t rounded_down,
							   ::std::uint_least64_t remainder, ::std::uint_least64_t divisor,
							   bool tail_nonzero) noexcept
{
	/*
	For retained decimal integer q, the discarded suffix represents a
	nonnegative fraction. An empty suffix is exact and cannot change q under
	any rounding policy; this branch also prevents a directed mode from moving
	an already representable value.
	*/
	if (!remainder && !tail_nonzero)
	{
		return false;
	}
	if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
	{
		/*
		The common nearest decision follows from comparing the discarded
		fraction with 1/2. A nonzero tail after an exactly-half retained
		remainder makes the full suffix strictly greater than one half. Parity
		(or sign for another tie convention) matters only in the exact-half
		branch; this is the defining distinction between nearest-to-odd and
		round-to-odd jamming.
		*/
		auto const half{divisor >> 1u};
		if (remainder < half)
		{
			return false;
		}
		if (half < remainder || tail_nonzero)
		{
			return true;
		}
		return ::fast_io::details::print_rsv_fp_decimal_tie_round_up<rounding>(negative, rounded_down);
	}
	else
	{
		return ::fast_io::details::floating_rounding_directed_round_up<rounding>(negative);
	}
}

template <typename T>
inline constexpr void scan_decfloat_append_digit(scan_decfloat_significand_state<T> &state, char8_t digit,
												 bool after_decimal) noexcept
{
	constexpr auto digit_limit{::fast_io::details::scan_decfloat_significand_digit_limit<T>};
	state.has_digit = true;
	if (after_decimal)
	{
		++state.fractional_digits;
	}
	if (!state.has_nonzero_digit)
	{
		if (digit == 0)
		{
			return;
		}
		state.has_nonzero_digit = true;
	}
	++state.significant_digits;
	::fast_io::details::scan_decfloat_append_exact_digit(state, digit);
	if (state.stored_digits != digit_limit)
	{
		state.significand = state.significand * 10u + static_cast<::std::uint_least64_t>(digit);
		++state.stored_digits;
	}
	else if (digit != 0)
	{
		state.truncated_nonzero = true;
	}
}

[[nodiscard]] inline constexpr bool
scan_decfloat_ascii8_is_digits(::std::uint_least64_t val) noexcept
{
	/*
	Interpret val as eight little-endian unsigned bytes c_i.  For an ASCII
	digit, c_i-0x30 is in [0,9] and c_i+0x46 is in [0x76,0x7f], so neither
	operation sets that lane's high bit or propagates a borrow/carry.

	If some lane is invalid, choose the least-significant invalid lane; every
	earlier lane is a digit and therefore supplies no incoming borrow/carry.
	For c_i<0x30, subtraction modulo 256 lies in [0xd0,0xff] and sets bit 7.
	For 0x3a<=c_i<=0xb9, addition lies in [0x80,0xff] and sets bit 7.  For
	c_i>=0xba, subtraction lies in [0x8a,0xcf] and sets bit 7.  Thus the masked
	word is zero iff all eight lanes are digits.  This also proves that whole-
	word carry propagation cannot create a false acceptance.
	*/
	return ((((val + 0x4646464646464646u) | (val - 0x3030303030303030u)) &
			 0x8080808080808080u) == 0);
}

[[nodiscard]] inline constexpr bool
scan_decfloat_ascii8_has_nonzero_digit(::std::uint_least64_t val) noexcept
{
	/*
	The caller first proves every byte is a digit.  XOR with eight ASCII zero
	bytes is therefore zero exactly for the string "00000000"; any nonzero
	digit changes at least one lane and hence the full integer.
	*/
	return (val ^ 0x3030303030303030u) != 0;
}

template <::std::size_t vec_size, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type const *
scan_decfloat_skip_after_exact_limit_simd(char_type const *first, char_type const *last,
										  bool &tail_nonzero) noexcept
{
	static_assert(sizeof(char_type) == sizeof(char8_t));
	using unsigned_char_type = ::std::make_unsigned_t<::std::remove_cvref_t<char_type>>;
	using signed_char_type = ::std::make_signed_t<unsigned_char_type>;
	constexpr unsigned N{vec_size / sizeof(char_type)};
	using simd_vector_type = ::fast_io::intrinsics::simd_vector<signed_char_type, N>;
	// std::bit_cast preserves every byte of the character array, so both branches
	// construct the same vector on every ISA.  Clang 22 and 23 were additionally
	// verified to lower the constant form to the same x86-64 instructions as
	// load(); Clang 21 and older keep the load fallback because their constexpr
	// vector lowering remains unmeasured.  Admitting later Clang releases and
	// non-Clang frontends is a conservative code-generation hypothesis based on
	// that semantic identity, not a claim that every future target was measured;
	// their emitted objects must be re-audited before relying on performance.
#if (__cpp_lib_bit_cast >= 201806L) && (!defined(__clang__) || __clang_major__ >= 22)
	constexpr simd_vector_type zeroes{
		::std::bit_cast<simd_vector_type>(::fast_io::details::characters_array_impl<u8'0', char_type, N>)};
	constexpr simd_vector_type nines{
		::std::bit_cast<simd_vector_type>(::fast_io::details::characters_array_impl<u8'9', char_type, N>)};
#else
	simd_vector_type zeroes;
	zeroes.load(::fast_io::details::characters_array_impl<u8'0', char_type, N>.data());
	simd_vector_type nines;
	nines.load(::fast_io::details::characters_array_impl<u8'9', char_type, N>.data());
#endif
	for (; N <= static_cast<::std::size_t>(last - first);)
	{
		/*
		The loop guard proves the full N-code-unit load is contained in
		[first,last); no masked or speculative overread is required.
		*/
		simd_vector_type vec;
		vec.load(first);
		/*
		ASCII '0'..'9' are positive signed-byte values.  Lane-wise signed
		comparisons therefore satisfy

		    valid_i <=> ('0' <= vec_i && vec_i <= '9')

		for every possible input byte: negative lanes fail the lower bound and
		bytes above 0x7f also appear negative and fail it.  Conjunction is thus
		the exact digit predicate, not a locale-dependent approximation.
		*/
		auto const valid{(zeroes <= vec) & (vec <= nines)};
		auto const invalid{~valid};
		if (!::fast_io::intrinsics::is_all_zeros(invalid))
		{
			/*
			Comparison masks use an all-zero lane for valid and an all-one lane
			for invalid after complementation.  Counting zero lanes from the
			front yields precisely the index of the first nondigit.  The scalar
			prefix loop visits exactly the accepted lanes and computes their
			sticky OR before returning that boundary.
			*/
			auto const valid_count{::fast_io::intrinsics::vector_mask_countr_zero(invalid)};
			auto const *const invalid_pos{first + valid_count};
			for (; first != invalid_pos; ++first)
			{
				if (!tail_nonzero && *first != ::fast_io::char_literal_v<u8'0', char_type>)
				{
					tail_nonzero = true;
				}
			}
			return first;
		}
		if (!tail_nonzero && !::fast_io::intrinsics::is_all_zeros(vec != zeroes))
		{
			/*
			All lanes are already known digits.  Therefore vec!=zeroes has a
			nonzero lane iff at least one digit is 1..9, exactly the sticky
			predicate needed after the retained exact prefix.
			*/
			tail_nonzero = true;
		}
		first += N;
	}
	return first;
}

template <::std::size_t vec_size, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type const *
scan_decfloat_skip_after_exact_limit_padding_simd(
	char_type const *first, char_type const *last, bool &tail_nonzero,
	::std::size_t padding) noexcept
{
	/*
	The ordinary SIMD loop owns every complete semantic block.  Only its final
	0<R<N suffix is eligible for the padded leaf, so ordinary scanner ABI and
	loop code generation are completely independent of this protocol.
	*/
	first = ::fast_io::details::scan_decfloat_skip_after_exact_limit_simd<vec_size>(
		first, last, tail_nonzero);
#if ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	 !(defined(__arm64ec__) || defined(_M_ARM64EC))) &&                  \
	!defined(__AVX512F__) &&                                            \
	(defined(__AVX2__) || defined(__SSE2__))
	static_assert(sizeof(char_type) == sizeof(char8_t));
	constexpr ::std::size_t N{vec_size / sizeof(char_type)};
	auto const remaining{static_cast<::std::size_t>(last - first)};
	if (remaining == 0u || N <= remaining || padding < N - remaining)
	{
		return first;
	}

	using unsigned_char_type =
		::std::make_unsigned_t<::std::remove_cvref_t<char_type>>;
	using signed_char_type = ::std::make_signed_t<unsigned_char_type>;
	using simd_vector_type =
		::fast_io::intrinsics::simd_vector<signed_char_type, N>;
#if defined(__GNUC__) && !defined(__clang__)
	constexpr ::std::size_t minimum_profitable_tail{4u};
#else
	constexpr ::std::size_t minimum_profitable_tail{3u};
#endif
	if (remaining < minimum_profitable_tail)
	{
		/*
		For one to three decimal bytes the scalar cleanup is cheaper than
		materializing a complete vector and extracting its mask.  Returning the
		unchanged semantic cursor leaves that cleanup to the caller.
		*/
		return first;
	}
#if (__cpp_lib_bit_cast >= 201806L) && (!defined(__clang__) || __clang_major__ >= 22)
	constexpr simd_vector_type zeroes{
		::std::bit_cast<simd_vector_type>(
			::fast_io::details::characters_array_impl<u8'0', char_type, N>)};
	constexpr simd_vector_type nines{
		::std::bit_cast<simd_vector_type>(
			::fast_io::details::characters_array_impl<u8'9', char_type, N>)};
#else
	simd_vector_type zeroes;
	zeroes.load(
		::fast_io::details::characters_array_impl<u8'0', char_type, N>.data());
	simd_vector_type nines;
	nines.load(
		::fast_io::details::characters_array_impl<u8'9', char_type, N>.data());
#endif
	simd_vector_type vec;
	vec.load(first);
	auto const invalid{~((zeroes <= vec) & (vec <= nines))};
	auto accepted{
		::fast_io::intrinsics::is_all_zeros(invalid)
			? N
			: static_cast<::std::size_t>(
				  ::fast_io::intrinsics::vector_mask_countr_zero(invalid))};
	if (remaining < accepted)
	{
		accepted = remaining;
	}
	auto const *const stop{first + accepted};
	if (!tail_nonzero)
	{
		auto probe{first};
		for (; probe != stop; ++probe)
		{
			if (*probe !=
				::fast_io::char_literal_v<u8'0', char_type>)
			{
				tail_nonzero = true;
				break;
			}
		}
	}
	first = stop;
#else
	(void)padding;
#endif
	return first;
}

template <typename T, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type const *scan_decfloat_skip_after_exact_limit_run(
	char_type const *first, char_type const *last, bool after_decimal,
	scan_decfloat_significand_state<T> &state) noexcept
{
	auto const *const original_first{first};
	auto tail_nonzero{state.exact_truncated_nonzero};
	FAST_IO_IF_NOT_CONSTEVAL
	{
		if constexpr (::fast_io::details::is_ascii<char_type> && sizeof(char_type) == sizeof(char8_t) &&
					  ::std::numeric_limits<::std::uint_least64_t>::digits == 64u)
		{
			constexpr auto simd_size{
				::fast_io::intrinsics::optimal_simd_vector_run_with_cpu_instruction_size_with_mask_countr};
			if constexpr (simd_size != 0)
			{
				first = ::fast_io::details::scan_decfloat_skip_after_exact_limit_simd<simd_size>(
					first, last, tail_nonzero);
			}
			for (; static_cast<::std::size_t>(last - first) >= sizeof(::std::uint_least64_t);)
			{
				::std::uint_least64_t val;
				::fast_io::freestanding::my_memcpy(__builtin_addressof(val), first, sizeof(::std::uint_least64_t));
				if constexpr (::std::endian::little != ::std::endian::native)
				{
					val = ::fast_io::little_endian(val);
				}
				if (!::fast_io::details::scan_decfloat_ascii8_is_digits(val))
				{
					break;
				}
				if (!tail_nonzero && ::fast_io::details::scan_decfloat_ascii8_has_nonzero_digit(val))
				{
					tail_nonzero = true;
				}
				first += sizeof(::std::uint_least64_t);
			}
		}
	}
	char8_t digit{};
	for (; first != last && ::fast_io::details::scan_decfloat_decimal_digit(*first, digit); ++first)
	{
		if (!tail_nonzero && digit != 0)
		{
			tail_nonzero = true;
		}
	}
	auto const skipped{static_cast<::std::uint_least64_t>(first - original_first)};
	if (skipped)
	{
		if (after_decimal)
		{
			state.fractional_digits += skipped;
		}
		state.significant_digits += skipped;
		if (tail_nonzero)
		{
			state.exact_truncated_nonzero = true;
			state.truncated_nonzero = true;
		}
	}
	return first;
}

template <typename T, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type const *
scan_decfloat_skip_after_exact_limit_padding_run(
	char_type const *first, char_type const *last, bool after_decimal,
	scan_decfloat_significand_state<T> &state, ::std::size_t padding) noexcept
{
	auto const *const original_first{first};
	auto tail_nonzero{state.exact_truncated_nonzero};
	FAST_IO_IF_NOT_CONSTEVAL
	{
		if constexpr (
			::fast_io::details::is_ascii<char_type> &&
			sizeof(char_type) == sizeof(char8_t) &&
			::std::numeric_limits<::std::uint_least64_t>::digits == 64u)
		{
			constexpr auto simd_size{
				::fast_io::intrinsics::
					optimal_simd_vector_run_with_cpu_instruction_size_with_mask_countr};
			if constexpr (simd_size != 0)
			{
				first =
					::fast_io::details::
						scan_decfloat_skip_after_exact_limit_padding_simd<
							simd_size>(first, last, tail_nonzero, padding);
			}
			for (; static_cast<::std::size_t>(last - first) >=
				   sizeof(::std::uint_least64_t);)
			{
				::std::uint_least64_t val;
				::fast_io::freestanding::my_memcpy(
					__builtin_addressof(val), first,
					sizeof(::std::uint_least64_t));
				if constexpr (::std::endian::little != ::std::endian::native)
				{
					val = ::fast_io::little_endian(val);
				}
				if (!::fast_io::details::scan_decfloat_ascii8_is_digits(val))
				{
					break;
				}
				if (!tail_nonzero &&
					::fast_io::details::
						scan_decfloat_ascii8_has_nonzero_digit(val))
				{
					tail_nonzero = true;
				}
				first += sizeof(::std::uint_least64_t);
			}
		}
	}
	char8_t digit{};
	for (; first != last &&
		   ::fast_io::details::scan_decfloat_decimal_digit(*first, digit);
		 ++first)
	{
		if (!tail_nonzero && digit != 0)
		{
			tail_nonzero = true;
		}
	}
	auto const skipped{
		static_cast<::std::uint_least64_t>(first - original_first)};
	if (skipped)
	{
		if (after_decimal)
		{
			state.fractional_digits += skipped;
		}
		state.significant_digits += skipped;
		if (tail_nonzero)
		{
			state.exact_truncated_nonzero = true;
			state.truncated_nonzero = true;
		}
	}
	return first;
}

[[nodiscard]] inline constexpr ::std::uint_least32_t
scan_decfloat_ascii8_parse(::std::uint_least64_t val) noexcept
{
	/*
	Let B=2^8 and let the already validated little-endian bytes be
	c_i='0'+d_i, 0<=d_i<=9.  Whole-word subtraction by eight 0x30 bytes has no
	inter-byte borrow: starting at the least-significant lane, c_i>=0x30 and
	every preceding lane generated no borrow.  Hence the resulting word is
	V=sum(d_i B^i) exactly.

	W=10V+(V/B) has byte i equal to p_i=10d_i+d_(i+1) for i<7.
	Since p_i<=99<B, neither multiplication by ten nor the addition propagates
	a carry between bytes.  The even bytes p_0,p_2,p_4,p_6 are therefore the
	four adjacent two-digit groups.  With M=0x000000ff000000ff, write

	  X=W&M              = p_0+p_4 B^4,
	  Y=(W/B^2)&M        = p_2+p_6 B^4.

	The constants below are 100+10^6 B^4 and 1+10^4 B^4.  In unsigned
	64-bit arithmetic, terms containing B^8 vanish modulo B^8, so the high
	base-B^4 limb of

	  X(100+10^6 B^4)+Y(1+10^4 B^4)

	is 10^6 p_0+10^4 p_2+100 p_4+p_6.  Its low limb is
	100p_0+p_2<=9999<B^4 and cannot carry into that result.  Substituting the
	p_i gives sum(d_i 10^(7-i)), the exact eight-digit integer; it is below
	10^8 and consequently survives the final uint_least32_t conversion.
	*/
	constexpr ::std::uint_least64_t mask{0x000000FF000000FFu};
	constexpr ::std::uint_least64_t mul1{0x000F424000000064u};
	constexpr ::std::uint_least64_t mul2{0x0000271000000001u};
	val -= 0x3030303030303030u;
	val = (val * 10u) + (val >> 8u);
	val = (((val & mask) * mul1) + (((val >> 16u) & mask) * mul2)) >> 32u;
	return static_cast<::std::uint_least32_t>(val);
}

template <typename T>
inline constexpr void scan_decfloat_append_eight_digits(scan_decfloat_significand_state<T> &state,
														bool after_decimal,
														::std::uint_least32_t digits) noexcept
{
	constexpr auto digit_limit{::fast_io::details::scan_decfloat_significand_digit_limit<T>};
	state.has_digit = true;
	if (after_decimal)
	{
		state.fractional_digits += 8u;
	}
	if (!state.has_nonzero_digit)
	{
		if (digits == 0)
		{
			return;
		}
		state.has_nonzero_digit = true;
	}
	state.significant_digits += 8u;
	::fast_io::details::scan_decfloat_append_exact_eight_digits(state, digits);
	if (state.stored_digits == digit_limit)
	{
		if (digits != 0)
		{
			state.truncated_nonzero = true;
		}
		return;
	}
	auto const available{digit_limit - state.stored_digits};
	if (8u <= available)
	{
		state.significand = state.significand * 100000000u + static_cast<::std::uint_least64_t>(digits);
		state.stored_digits += 8u;
		return;
	}
	auto const divisor{::fast_io::details::scan_decfloat_pow10_0_to_8_table[8u - available]};
	auto const stored_part{digits / divisor};
	auto const truncated_part{digits - stored_part * divisor};
	state.significand =
		state.significand * ::fast_io::details::scan_decfloat_pow10_0_to_8_table[available] +
		static_cast<::std::uint_least64_t>(stored_part);
	state.stored_digits = digit_limit;
	if (truncated_part != 0)
	{
		state.truncated_nonzero = true;
	}
}

template <typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void scan_decfloat_append_eight_ascii_digits(scan_decfloat_significand_state<T> &state,
															  bool after_decimal,
															  ::std::uint_least64_t val) noexcept
{
	constexpr auto digit_limit{::fast_io::details::scan_decfloat_significand_digit_limit<T>};
	auto const digits{::fast_io::details::scan_decfloat_ascii8_parse(val)};
	state.has_digit = true;
	if (after_decimal)
	{
		state.fractional_digits += 8u;
	}
	if (!state.has_nonzero_digit)
	{
		if (digits == 0)
		{
			return;
		}
		state.has_nonzero_digit = true;
	}
	state.significant_digits += 8u;
	::fast_io::details::scan_decfloat_append_exact_ascii8_digits(state, val);
	if (state.stored_digits == digit_limit)
	{
		if (digits != 0)
		{
			state.truncated_nonzero = true;
		}
		return;
	}
	auto const available{digit_limit - state.stored_digits};
	if (8u <= available)
	{
		state.significand = state.significand * 100000000u + static_cast<::std::uint_least64_t>(digits);
		state.stored_digits += 8u;
		return;
	}
	auto const divisor{::fast_io::details::scan_decfloat_pow10_0_to_8_table[8u - available]};
	auto const stored_part{digits / divisor};
	auto const truncated_part{digits - stored_part * divisor};
	state.significand =
		state.significand * ::fast_io::details::scan_decfloat_pow10_0_to_8_table[available] +
		static_cast<::std::uint_least64_t>(stored_part);
	state.stored_digits = digit_limit;
	if (truncated_part != 0)
	{
		state.truncated_nonzero = true;
	}
}

template <typename T, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type const *
scan_decfloat_digits(char_type const *first, char_type const *last, bool after_decimal,
					 scan_decfloat_significand_state<T> &state) noexcept
{
	constexpr auto digit_limit{::fast_io::details::scan_decfloat_significand_digit_limit<T>};
	char8_t digit{};
	if constexpr (sizeof(char_type) == sizeof(char8_t) && ::fast_io::details::is_ascii<char_type> &&
				  ::std::numeric_limits<::std::uint_least64_t>::digits == 64u)
	{
		FAST_IO_IF_NOT_CONSTEVAL
		{
			for (; first != last && !state.has_nonzero_digit; ++first)
			{
				if (!::fast_io::details::scan_decfloat_decimal_digit(*first, digit))
				{
					return first;
				}
				::fast_io::details::scan_decfloat_append_digit<T>(state, digit, after_decimal);
			}
			if (static_cast<::std::size_t>(last - first) >= 2u * sizeof(::std::uint_least64_t))
			{
				for (; static_cast<::std::size_t>(last - first) >= sizeof(::std::uint_least64_t);)
				{
					if (state.has_nonzero_digit && state.stored_digits == digit_limit &&
						state.exact_stored_digits == ::fast_io::details::scan_decfloat_exact_digit_capacity<T>)
					{
						first = ::fast_io::details::scan_decfloat_skip_after_exact_limit_run(
							first, last, after_decimal, state);
						break;
					}
					::std::uint_least64_t val;
					::fast_io::freestanding::my_memcpy(__builtin_addressof(val), first, sizeof(::std::uint_least64_t));
					if constexpr (::std::endian::little != ::std::endian::native)
					{
						val = ::fast_io::little_endian(val);
					}
					if (!::fast_io::details::scan_decfloat_ascii8_is_digits(val))
					{
						break;
					}
					if (state.has_nonzero_digit &&
						state.stored_digits == digit_limit)
					{
						if (after_decimal)
						{
							state.fractional_digits += 8u;
						}
						state.significant_digits += 8u;
						auto const has_nonzero{
							::fast_io::details::scan_decfloat_ascii8_has_nonzero_digit(val)};
						if (state.exact_stored_digits !=
							::fast_io::details::scan_decfloat_exact_digit_capacity<T>)
						{
							::fast_io::details::scan_decfloat_append_exact_ascii8_digits(state, val);
						}
						else if (!state.exact_truncated_nonzero && has_nonzero)
						{
							state.exact_truncated_nonzero = true;
						}
						if (!state.truncated_nonzero && has_nonzero)
						{
							state.truncated_nonzero = true;
						}
					}
					else
					{
						::fast_io::details::scan_decfloat_append_eight_ascii_digits<T>(
							state, after_decimal, val);
					}
					first += sizeof(::std::uint_least64_t);
				}
			}
		}
	}
	for (; first != last && ::fast_io::details::scan_decfloat_decimal_digit(*first, digit); ++first)
	{
		::fast_io::details::scan_decfloat_append_digit<T>(state, digit, after_decimal);
	}
	return first;
}

template <typename T, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type const *
scan_decfloat_digits_padding(
	char_type const *first, char_type const *last, bool after_decimal,
	scan_decfloat_significand_state<T> &state, ::std::size_t padding) noexcept
{
	constexpr auto digit_limit{
		::fast_io::details::scan_decfloat_significand_digit_limit<T>};
	char8_t digit{};
	if constexpr (
		sizeof(char_type) == sizeof(char8_t) &&
		::fast_io::details::is_ascii<char_type> &&
		::std::numeric_limits<::std::uint_least64_t>::digits == 64u)
	{
		FAST_IO_IF_NOT_CONSTEVAL
		{
			for (; first != last && !state.has_nonzero_digit; ++first)
			{
				if (!::fast_io::details::scan_decfloat_decimal_digit(
						*first, digit))
				{
					return first;
				}
				::fast_io::details::scan_decfloat_append_digit<T>(
					state, digit, after_decimal);
			}
			if (static_cast<::std::size_t>(last - first) >=
				2u * sizeof(::std::uint_least64_t))
			{
				for (; static_cast<::std::size_t>(last - first) >=
					   sizeof(::std::uint_least64_t);)
				{
					if (state.has_nonzero_digit &&
						state.stored_digits == digit_limit &&
						state.exact_stored_digits ==
							::fast_io::details::
								scan_decfloat_exact_digit_capacity<T>)
					{
						first =
							::fast_io::details::
								scan_decfloat_skip_after_exact_limit_padding_run(
									first, last, after_decimal, state, padding);
						break;
					}
					::std::uint_least64_t val;
					::fast_io::freestanding::my_memcpy(
						__builtin_addressof(val), first,
						sizeof(::std::uint_least64_t));
					if constexpr (
						::std::endian::little != ::std::endian::native)
					{
						val = ::fast_io::little_endian(val);
					}
					if (!::fast_io::details::
							scan_decfloat_ascii8_is_digits(val))
					{
						break;
					}
					if (state.has_nonzero_digit &&
						state.stored_digits == digit_limit)
					{
						if (after_decimal)
						{
							state.fractional_digits += 8u;
						}
						state.significant_digits += 8u;
						auto const has_nonzero{
							::fast_io::details::
								scan_decfloat_ascii8_has_nonzero_digit(val)};
						if (state.exact_stored_digits !=
							::fast_io::details::
								scan_decfloat_exact_digit_capacity<T>)
						{
							::fast_io::details::
								scan_decfloat_append_exact_ascii8_digits(
									state, val);
						}
						else if (!state.exact_truncated_nonzero &&
								 has_nonzero)
						{
							state.exact_truncated_nonzero = true;
						}
						if (!state.truncated_nonzero && has_nonzero)
						{
							state.truncated_nonzero = true;
						}
					}
					else
					{
						::fast_io::details::
							scan_decfloat_append_eight_ascii_digits<T>(
								state, after_decimal, val);
					}
					first += sizeof(::std::uint_least64_t);
				}
			}
		}
	}
	for (; first != last &&
		   ::fast_io::details::scan_decfloat_decimal_digit(*first, digit);
		 ++first)
	{
		::fast_io::details::scan_decfloat_append_digit<T>(
			state, digit, after_decimal);
	}
	return first;
}

template <::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_exponent(char_type const *first, char_type const *last, ::std::int_least64_t &exponent) noexcept
{
	if constexpr (sizeof(char_type) == sizeof(char8_t) && ::fast_io::details::is_ascii<char_type>)
	{
		FAST_IO_IF_NOT_CONSTEVAL
		{
			auto const *const original_first{first};
			if (first == last)
			{
				return {first, ::fast_io::parse_code::invalid};
			}
			using unsigned_char_type = ::fast_io::details::my_make_unsigned_t<char_type>;
			constexpr auto plus{static_cast<unsigned_char_type>(u8'+')};
			constexpr auto minus{static_cast<unsigned_char_type>(u8'-')};
			constexpr auto zero{static_cast<unsigned_char_type>(u8'0')};
			bool negative{};
			auto uch{static_cast<unsigned_char_type>(*first)};
			if (uch == minus)
			{
				negative = true;
				++first;
			}
			else if (uch == plus)
			{
				++first;
			}
			if (first == last)
			{
				return {first, ::fast_io::parse_code::invalid};
			}
			uch = static_cast<unsigned_char_type>(*first);
			auto digit{static_cast<unsigned_char_type>(uch - zero)};
			if (10u <= digit)
			{
				return {first, ::fast_io::parse_code::invalid};
			}
			::std::uint_least64_t value{static_cast<::std::uint_least64_t>(digit)};
			++first;
			::std::size_t digit_count{1u};
			for (; first != last; ++first)
			{
				uch = static_cast<unsigned_char_type>(*first);
				digit = static_cast<unsigned_char_type>(uch - zero);
				if (10u <= digit)
				{
					break;
				}
				if (digit_count == 18u)
				{
					return ::fast_io::details::scan_hexfloat_exponent(original_first, last, exponent);
				}
				value = value * 10u + static_cast<::std::uint_least64_t>(digit);
				++digit_count;
			}
			exponent = negative ? -static_cast<::std::int_least64_t>(value) :
								  static_cast<::std::int_least64_t>(value);
			return {first, ::fast_io::parse_code::ok};
		}
	}
	return ::fast_io::details::scan_hexfloat_exponent(first, last, exponent);
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr bool scan_decfloat_exponent_prefix_may_extend(char_type const *first,
																			 char_type const *last) noexcept
{
	constexpr auto plus{::fast_io::char_literal_v<u8'+', char_type>};
	constexpr auto minus{::fast_io::char_literal_v<u8'-', char_type>};
	++first;
	if (first == last)
	{
		return true;
	}
	if ((*first == plus || *first == minus) && first + 1 == last)
	{
		return true;
	}
	return false;
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr bool scan_decfloat_special_start_char(char_type ch) noexcept
{
	return ::fast_io::details::scan_hexfloat_caseless_equal<u8'i', u8'I'>(ch) ||
		   ::fast_io::details::scan_hexfloat_caseless_equal<u8'n', u8'N'>(ch);
}

template <typename T>
[[nodiscard]] inline constexpr ::fast_io::parse_code
scan_decfloat_assign_adjusted(T &value, bool negative, ::std::uint_least64_t significand,
							  scan_decfloat_adjusted_mantissa adjusted) noexcept
{
	::fast_io::details::scan_decfloat_to_float(negative, adjusted, value);
	if ((significand != 0 && adjusted.mantissa == 0 && adjusted.power2 == 0) ||
		adjusted.power2 == ::fast_io::details::scan_decfloat_infinite_power<T>())
	{
		return ::fast_io::parse_code::overflow;
	}
	return ::fast_io::parse_code::ok;
}

template <typename T, ::fast_io::manipulators::floating_rounding rounding =
						  ::fast_io::manipulators::floating_rounding::nearest_to_even>
[[nodiscard]] inline constexpr ::fast_io::parse_code scan_decfloat_assign(T &value, bool negative,
																		  scan_decfloat_significand_state<T> const &state,
																		  ::std::int_least64_t exponent) noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	auto adjusted_exponent{::fast_io::details::scan_decfloat_adjusted_exponent(state, exponent)};
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even)
	{
		if (::std::is_constant_evaluated() &&
			!state.truncated_nonzero &&
			::fast_io::details::scan_decfloat_try_clinger(
				state.significand, adjusted_exponent, negative, value))
		{
			/*
			Constant evaluation has a translation-fixed nearest-even mode, and
			an untruncated state denotes the exact integer significand.  The
			Clinger bounds then prove the native multiply/divide is correctly
			rounded.  At runtime the first conjunct is false and this entire
			branch folds away, preserving fenv independence.
			*/
			return ::fast_io::parse_code::ok;
		}
	}
	if constexpr (::fast_io::details::fp_floating_point_is_float80<no_cvref_t>)
	{
		if (!state.truncated_nonzero && ::fast_io::details::scan_decfloat_try_x87_cached<rounding>(
				value, negative, state.significand, adjusted_exponent))
		{
			return ::fast_io::parse_code::ok;
		}
	}
	if constexpr (::fast_io::details::scan_decfloat_compute_supported<no_cvref_t>)
	{
		::fast_io::details::scan_decfloat_adjusted_mantissa adjusted;
		if (!state.truncated_nonzero)
		{
			if (::fast_io::details::scan_decfloat_compute_adjusted<no_cvref_t, rounding>(
					adjusted_exponent, state.significand, negative, adjusted))
			{
				return ::fast_io::details::scan_decfloat_assign_adjusted(value, negative, state.significand, adjusted);
			}
		}
		else if (state.significand != 0 &&
				 state.significand != (::std::numeric_limits<::std::uint_least64_t>::max)())
		{
			::fast_io::details::scan_decfloat_adjusted_mantissa upper_adjusted;
			if (::fast_io::details::scan_decfloat_compute_adjusted<no_cvref_t, rounding>(
					adjusted_exponent, state.significand, negative, adjusted) &&
				::fast_io::details::scan_decfloat_compute_adjusted<no_cvref_t, rounding>(
					adjusted_exponent, state.significand + 1u, negative, upper_adjusted))
			{
				if (::fast_io::details::scan_decfloat_adjusted_mantissa_equal(adjusted, upper_adjusted))
				{
					return ::fast_io::details::scan_decfloat_assign_adjusted(value, negative, state.significand, adjusted);
				}
				auto decimal_exponent{::fast_io::details::scan_decfloat_saturating_add(
					exponent, -static_cast<::std::int_least64_t>(state.fractional_digits))};
				decimal_exponent = ::fast_io::details::scan_decfloat_saturating_add(
					decimal_exponent,
					static_cast<::std::int_least64_t>(
						state.significant_digits - state.exact_stored_digits));
				::fast_io::parse_code code{};
				if (::fast_io::details::scan_decfloat_try_interval_compare<
						no_cvref_t, rounding>(
						value, negative, state, decimal_exponent,
						adjusted, upper_adjusted, code))
				{
					return code;
				}
			}
		}
	}
	return ::fast_io::details::scan_decfloat_assign_big<T, rounding>(value, negative, state, exponent);
}

/** @brief Materializes the exact coefficient only after the bounded conversion has declined it.
 * @details Wide formats retain up to 12,000 decimal digits. Allocate that context in the fallback
 * frame so a complete short field can return without probing stack pages for an unused context.
 */
template <typename T, ::fast_io::manipulators::floating_rounding rounding>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
[[nodiscard]] inline constexpr ::fast_io::parse_code
scan_decfloat_assign_wide_short_exact(T &value, bool negative, ::std::uint_least64_t significand,
									  ::std::int_least64_t adjusted_exponent) noexcept
{
	scan_decfloat_significand_state<T> state;
	scan_decfloat_state_from_u64(state, significand);
	return scan_decfloat_assign_big<T, rounding>(value, negative, state, adjusted_exponent);
}

template <typename T, ::fast_io::manipulators::floating_rounding rounding =
						  ::fast_io::manipulators::floating_rounding::nearest_to_even>
[[nodiscard]] inline constexpr ::fast_io::parse_code
scan_decfloat_assign_significand(T &value, bool negative, ::std::uint_least64_t significand,
								 ::std::int_least64_t adjusted_exponent) noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if (!significand)
	{
		value = negative ? -static_cast<T>(0.0) : static_cast<T>(0.0);
		return ::fast_io::parse_code::ok;
	}
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even)
	{
		if (::std::is_constant_evaluated() &&
			::fast_io::details::scan_decfloat_try_clinger(
				significand, adjusted_exponent, negative, value))
		{
			/*
			This state contains no omitted digits by construction.  Therefore
			the constant-evaluation Clinger theorem applies directly; runtime
			again bypasses native floating arithmetic before reading the fenv.
			*/
			return ::fast_io::parse_code::ok;
		}
	}
	if constexpr (::fast_io::details::fp_floating_point_is_float80<no_cvref_t>)
	{
		if (::fast_io::details::scan_decfloat_try_x87_cached<rounding>(
				value, negative, significand, adjusted_exponent))
		{
			return ::fast_io::parse_code::ok;
		}
		return ::fast_io::details::scan_decfloat_assign_wide_short_exact<T, rounding>(
			value, negative, significand, adjusted_exponent);
	}
	if constexpr (::fast_io::details::scan_decfloat_compute_supported<no_cvref_t>)
	{
		::fast_io::details::scan_decfloat_adjusted_mantissa adjusted;
		if (::fast_io::details::scan_decfloat_compute_adjusted<no_cvref_t, rounding>(
				adjusted_exponent, significand, negative, adjusted))
		{
			return ::fast_io::details::scan_decfloat_assign_adjusted(value, negative, significand, adjusted);
		}
	}
	::fast_io::details::scan_decfloat_significand_state<T> state;
	::fast_io::details::scan_decfloat_state_from_u64(state, significand);
	return ::fast_io::details::scan_decfloat_assign_big<T, rounding>(value, negative, state, adjusted_exponent);
}

/// @brief Rounds the decimal coefficient to the requested decimal grid, then converts to a binary value.
/// @details Ordinary scans use the same rounding for both steps. A narrower interchange destination
/// can request directed binary bounds on the same quantized decimal to resolve an exact halfway case.
template <typename T, ::fast_io::manipulators::floating_precision precision_mode,
		  ::fast_io::manipulators::floating_rounding rounding =
			  ::fast_io::manipulators::floating_rounding::nearest_to_even,
		  ::fast_io::manipulators::floating_rounding binary_rounding = rounding>
[[nodiscard]] inline constexpr ::fast_io::parse_code
scan_decfloat_assign_precision(T &value, bool negative, scan_decfloat_significand_state<T> const &state,
							   ::std::int_least64_t exponent, ::std::size_t precision) noexcept
{
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::current_environment)
	{
		switch (::fast_io::details::current_floating_rounding())
		{
		case ::fast_io::manipulators::floating_rounding::toward_plus_infinity:
			return ::fast_io::details::scan_decfloat_assign_precision<
				T, precision_mode, ::fast_io::manipulators::floating_rounding::toward_plus_infinity,
				(binary_rounding == ::fast_io::manipulators::floating_rounding::current_environment ? ::fast_io::manipulators::floating_rounding::toward_plus_infinity : binary_rounding)>(
				value, negative, state, exponent, precision);
		case ::fast_io::manipulators::floating_rounding::toward_minus_infinity:
			return ::fast_io::details::scan_decfloat_assign_precision<
				T, precision_mode, ::fast_io::manipulators::floating_rounding::toward_minus_infinity,
				(binary_rounding == ::fast_io::manipulators::floating_rounding::current_environment ? ::fast_io::manipulators::floating_rounding::toward_minus_infinity : binary_rounding)>(
				value, negative, state, exponent, precision);
		case ::fast_io::manipulators::floating_rounding::toward_zero:
			return ::fast_io::details::scan_decfloat_assign_precision<
				T, precision_mode, ::fast_io::manipulators::floating_rounding::toward_zero,
				(binary_rounding == ::fast_io::manipulators::floating_rounding::current_environment ? ::fast_io::manipulators::floating_rounding::toward_zero : binary_rounding)>(
				value, negative, state, exponent, precision);
		default:
			return ::fast_io::details::scan_decfloat_assign_precision<
				T, precision_mode, ::fast_io::manipulators::floating_rounding::nearest_to_even,
				(binary_rounding == ::fast_io::manipulators::floating_rounding::current_environment ? ::fast_io::manipulators::floating_rounding::nearest_to_even : binary_rounding)>(
				value, negative, state, exponent, precision);
		}
	}
	else
	{
		auto adjusted_exponent{::fast_io::details::scan_decfloat_adjusted_exponent(state, exponent)};
		auto significand{state.significand};
		if constexpr (::fast_io::details::floating_precision_is_significant<precision_mode>)
		{
			if (!precision)
			{
				precision = 1u;
			}
			if (state.significant_digits <= precision || state.stored_digits <= precision)
			{
				return ::fast_io::details::scan_decfloat_assign<T, binary_rounding>(value, negative, state, exponent);
			}
			auto const cut{static_cast<::std::uint_least32_t>(state.stored_digits - precision)};
			auto const divisor{::fast_io::details::print_rsv_fp_pow10_0_to_19_table[cut]};
			auto quotient{significand / divisor};
			auto const remainder{significand - quotient * divisor};
			if (::fast_io::details::scan_decfloat_decimal_round_up<rounding>(
					negative, quotient, remainder, divisor, state.truncated_nonzero))
			{
				++quotient;
			}
			significand = quotient;
			adjusted_exponent = ::fast_io::details::scan_decfloat_saturating_add(
				adjusted_exponent, static_cast<::std::int_least64_t>(cut));
			if (precision < 20u &&
				significand == ::fast_io::details::print_rsv_fp_pow10_0_to_19_table[precision])
			{
				significand /= 10u;
				adjusted_exponent = ::fast_io::details::scan_decfloat_saturating_add(adjusted_exponent, 1);
			}
		}
		else
		{
			constexpr auto int64_max{(::std::numeric_limits<::std::int_least64_t>::max)()};
			if (static_cast<::std::uintmax_t>(precision) >
				static_cast<::std::uintmax_t>(int64_max))
			{
				return ::fast_io::details::scan_decfloat_assign<T, binary_rounding>(value, negative, state, exponent);
			}
			auto const target_exponent{-static_cast<::std::int_least64_t>(precision)};
			if (target_exponent <= adjusted_exponent)
			{
				return ::fast_io::details::scan_decfloat_assign<T, binary_rounding>(value, negative, state, exponent);
			}
			auto const cut64{static_cast<::std::uint_least64_t>(target_exponent) -
							 static_cast<::std::uint_least64_t>(adjusted_exponent)};
			if (20u <= cut64)
			{
				if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
				{
					significand = 0u;
				}
				else
				{
					significand = static_cast<::std::uint_least64_t>(
						::fast_io::details::floating_rounding_directed_round_up<rounding>(negative));
				}
			}
			else
			{
				auto const divisor{
					::fast_io::details::print_rsv_fp_pow10_0_to_19_table[static_cast<::std::size_t>(cut64)]};
				auto quotient{significand / divisor};
				auto const remainder{significand - quotient * divisor};
				if (::fast_io::details::scan_decfloat_decimal_round_up<rounding>(
						negative, quotient, remainder, divisor, state.truncated_nonzero))
				{
					++quotient;
				}
				significand = quotient;
			}
			adjusted_exponent = target_exponent;
		}
		return ::fast_io::details::scan_decfloat_assign_significand<T, binary_rounding>(
			value, negative, significand, adjusted_exponent);
	}
}

template <typename T, ::fast_io::manipulators::floating_rounding rounding =
						  ::fast_io::manipulators::floating_rounding::nearest_to_even>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr ::fast_io::parse_code
scan_decfloat_assign_short(T &value, bool negative, ::std::uint_least64_t significand,
						   ::std::uint_least64_t fractional_digits,
						   ::std::int_least64_t exponent) noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	auto adjusted_exponent{::fast_io::details::scan_decfloat_saturating_add(
		exponent, -static_cast<::std::int_least64_t>(fractional_digits))};
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even)
	{
		if (::std::is_constant_evaluated() &&
			::fast_io::details::scan_decfloat_try_clinger(
				significand, adjusted_exponent, negative, value))
		{
			/*
			The short parser's coefficient and adjusted exponent denote the
			complete decimal exactly.  Constant evaluation may use Clinger;
			runtime conversion falls through to the same integer kernel used by
			long inputs and all other explicit policies.
			*/
			return ::fast_io::parse_code::ok;
		}
	}
	if (::fast_io::details::scan_decfloat_try_exact_dyadic(
			value, negative, significand, adjusted_exponent))
	{
		/*
		The helper returns true only after constructing the exact target bit
		pattern with integer arithmetic.  Exact representability makes all ten
		explicit rounding policies, and every current-environment selection,
		observationally identical; returning here cannot bypass a policy-specific
		tie, underflow, or overflow decision.
		*/
		return ::fast_io::parse_code::ok;
	}
	if constexpr (::fast_io::details::fp_floating_point_is_float80<no_cvref_t>)
	{
		if (::fast_io::details::scan_decfloat_try_x87_cached<rounding>(
				value, negative, significand, adjusted_exponent))
		{
			return ::fast_io::parse_code::ok;
		}
		return ::fast_io::details::scan_decfloat_assign_wide_short_exact<T, rounding>(
			value, negative, significand, adjusted_exponent);
	}
	if constexpr (::fast_io::details::scan_decfloat_compute_supported<no_cvref_t>)
	{
		::fast_io::details::scan_decfloat_adjusted_mantissa adjusted;
		if (::fast_io::details::scan_decfloat_compute_adjusted<no_cvref_t, rounding>(
				adjusted_exponent, significand, negative, adjusted))
		{
			return ::fast_io::details::scan_decfloat_assign_adjusted(value, negative, significand, adjusted);
		}
	}
	::fast_io::details::scan_decfloat_significand_state<T> state;
	::fast_io::details::scan_decfloat_state_from_u64(state, significand);
	return ::fast_io::details::scan_decfloat_assign_big<T, rounding>(value, negative, state, adjusted_exponent);
}

template <typename T>
[[nodiscard]] inline constexpr ::fast_io::parse_code
scan_decfloat_assign_current_environment(T &value, bool negative,
										 scan_decfloat_significand_state<T> const &state,
										 ::std::int_least64_t exponent) noexcept
{
	switch (::fast_io::details::current_floating_rounding())
	{
	case ::fast_io::manipulators::floating_rounding::toward_plus_infinity:
		return ::fast_io::details::scan_decfloat_assign<
			T, ::fast_io::manipulators::floating_rounding::toward_plus_infinity>(
			value, negative, state, exponent);
	case ::fast_io::manipulators::floating_rounding::toward_minus_infinity:
		return ::fast_io::details::scan_decfloat_assign<
			T, ::fast_io::manipulators::floating_rounding::toward_minus_infinity>(
			value, negative, state, exponent);
	case ::fast_io::manipulators::floating_rounding::toward_zero:
		return ::fast_io::details::scan_decfloat_assign<
			T, ::fast_io::manipulators::floating_rounding::toward_zero>(
			value, negative, state, exponent);
	default:
		return ::fast_io::details::scan_decfloat_assign<
			T, ::fast_io::manipulators::floating_rounding::nearest_to_even>(
			value, negative, state, exponent);
	}
}

template <typename T>
[[nodiscard]] inline constexpr ::fast_io::parse_code
scan_decfloat_assign_short_current_environment(T &value, bool negative, ::std::uint_least64_t significand,
											   ::std::uint_least64_t fractional_digits,
											   ::std::int_least64_t exponent) noexcept
{
	switch (::fast_io::details::current_floating_rounding())
	{
	case ::fast_io::manipulators::floating_rounding::toward_plus_infinity:
		return ::fast_io::details::scan_decfloat_assign_short<
			T, ::fast_io::manipulators::floating_rounding::toward_plus_infinity>(
			value, negative, significand, fractional_digits, exponent);
	case ::fast_io::manipulators::floating_rounding::toward_minus_infinity:
		return ::fast_io::details::scan_decfloat_assign_short<
			T, ::fast_io::manipulators::floating_rounding::toward_minus_infinity>(
			value, negative, significand, fractional_digits, exponent);
	case ::fast_io::manipulators::floating_rounding::toward_zero:
		return ::fast_io::details::scan_decfloat_assign_short<
			T, ::fast_io::manipulators::floating_rounding::toward_zero>(
			value, negative, significand, fractional_digits, exponent);
	default:
		return ::fast_io::details::scan_decfloat_assign_short<
			T, ::fast_io::manipulators::floating_rounding::nearest_to_even>(
			value, negative, significand, fractional_digits, exponent);
	}
}

/// @brief Identifies which boundary can commit a numeric token after its optional exponent.
enum class scan_decfloat_short_completion : unsigned char
{
	contiguous,
	chunk,
	terminal
};

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T,
		  scan_decfloat_short_completion completion = scan_decfloat_short_completion::contiguous>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr scan_decfloat_fast_result<char_type>
scan_decfloat_contiguous_short_define_impl(char_type const *first,
										   char_type const *end,
										   bool negative, T &value) noexcept
{
	if constexpr (sizeof(char_type) != sizeof(char8_t) || !::fast_io::details::is_ascii<char_type>)
	{
		return {};
	}
	else
	{
		FAST_IO_IF_CONSTEVAL
		{
			return {};
		}
		else
		{
			constexpr auto digit_limit{::fast_io::details::scan_decfloat_significand_digit_limit<T>};
			::std::uint_least64_t significand{};
			::std::uint_least64_t digit_count{};
			::std::uint_least64_t fractional_digits{};
			bool has_digit{};
			char8_t digit{};
			constexpr auto zero{
				::fast_io::char_literal_v<u8'0', char_type>};
			constexpr ::std::uint_least64_t ascii_zeroes{
				0x3030303030303030ULL};
#if (defined(__GNUC__) || defined(__clang__)) && defined(__x86_64__) && \
	!defined(__arm64ec__) && !defined(_M_ARM64EC)
			[[maybe_unused]] auto scan_short_digit_blocks = [&]() noexcept {
				// The caller selects a remaining span shorter than two eight-byte words.
				// Each block requires four readable characters and room for four more coefficient digits.
				for (; digit_count + 4u <= digit_limit && end - first >= 4; first += 4)
				{
					::std::uint_least64_t block;
					if (!scan_int_contiguous_x86_parse_four_digits<10u>(first, block))
					{
						break;
					}
					significand = significand * 10000u + block;
					digit_count += 4u;
					has_digit = true;
				}
			};
#endif
			/*
			If z leading integer zeroes precede the first nonzero digit, replacing
			the digit sequence 0^z D by D leaves both M*10^q and q unchanged:
			the integer radix position is unchanged and the removed coefficient
			terms are exactly zero.  Consequently they consume no significand
			precision.  The eight-byte equality is stronger than a digit test and
			therefore cannot skip punctuation or a nonzero digit.  Unaligned loads
			use memcpy, so the optimization adds no alignment precondition.
			*/
			for (; static_cast<::std::size_t>(end - first) >=
				   sizeof(::std::uint_least64_t);)
			{
				::std::uint_least64_t val;
				::fast_io::freestanding::my_memcpy(
					__builtin_addressof(val), first,
					sizeof(::std::uint_least64_t));
				if constexpr (
					::std::endian::little != ::std::endian::native)
				{
					val = ::fast_io::little_endian(val);
				}
				if (val != ascii_zeroes)
				{
					break;
				}
				first += sizeof(::std::uint_least64_t);
				has_digit = true;
			}
			for (; first != end && *first == zero; ++first)
			{
				has_digit = true;
			}
#if (defined(__GNUC__) || defined(__clang__)) && defined(__x86_64__) && \
	!defined(__arm64ec__) && !defined(_M_ARM64EC)
			constexpr bool resident_binary32{completion != scan_decfloat_short_completion::contiguous && scan_decfloat_binary32_like<T>};
			if (resident_binary32 && end - first < 2 * static_cast<::std::ptrdiff_t>(sizeof(::std::uint_least64_t)))
			{
				scan_short_digit_blocks();
			}
			else
#endif
			{
				for (; digit_count + 8u <= digit_limit &&
					   static_cast<::std::size_t>(end - first) >= sizeof(::std::uint_least64_t);)
				{
					::std::uint_least64_t val;
					::fast_io::freestanding::my_memcpy(__builtin_addressof(val), first, sizeof(::std::uint_least64_t));
					if constexpr (::std::endian::little != ::std::endian::native)
					{
						val = ::fast_io::little_endian(val);
					}
					if (!::fast_io::details::scan_decfloat_ascii8_is_digits(val))
					{
						break;
					}
					significand = significand * 100000000u +
								  static_cast<::std::uint_least64_t>(
									  ::fast_io::details::scan_decfloat_ascii8_parse(val));
					digit_count += 8u;
					first += sizeof(::std::uint_least64_t);
					has_digit = true;
				}
			}
			/*
			Peel a compiler-selected short scalar prefix without changing the
			decimal recurrence.
			One successful call consumes exactly one abstract digit d and applies

				(S,n,p) -> (10*S+d,n+1,p+1).

			The `&&` sequence stops at the first end/nondigit, exactly where the
			former loop stopped; only after every peeled call succeeds can the
			residual loop run.  Testing the digit before `digit_limit` is
			significant: a nondigit after the last storable digit completes the
			short token, whereas one more digit must select the exact long
			fallback.  Thus the peeled and loop forms have identical pointer,
			coefficient, count and fallback results.

			Four calls give the best measured layout on Apple/LLVM and GCC
			11--15.  GCC 16 lowers three calls to a smaller public parser and a
			better real Raptor-Lake result; LLVM-MCA also reduces its modeled
			uops versus four calls on Alder Lake, Zen 3/4 and Broadwell.  The
			compile-time choice changes only how many applications of the proved
			recurrence are written straight-line.  It does not claim to remove
			every dynamic branch, and the residual loop is identical.
			*/
			bool digit_limit_reached{};
			auto scan_one_digit = [&]() constexpr noexcept {
				if (first == end ||
					!::fast_io::details::scan_decfloat_decimal_digit(*first, digit))
				{
					return false;
				}
				if (digit_count == digit_limit)
				{
					digit_limit_reached = true;
					return false;
				}
				significand =
					significand * 10u + static_cast<::std::uint_least64_t>(digit);
				++digit_count;
				++first;
				has_digit = true;
				return true;
			};
#if defined(__GNUC__) && !defined(__clang__) && 16 <= __GNUC__
			if (scan_one_digit() && scan_one_digit() &&
				scan_one_digit())
#else
			if (scan_one_digit() && scan_one_digit() &&
				scan_one_digit() && scan_one_digit())
#endif
			{
				while (scan_one_digit())
				{
				}
			}
			if (digit_limit_reached)
			{
				// A decimal digit beyond capacity proves that the exact accumulator is necessary.
				return {first, ::fast_io::parse_code::partial, false};
			}

			constexpr auto dot{::fast_io::char_literal_v<
				(flags.comma ? u8',' : u8'.'), char_type>};
			if (first != end && *first == dot)
			{
				++first;
				auto const *fraction_begin{first};
				/*
				Fractional zeroes may be erased from the stored coefficient only
				while no earlier nonzero digit exists.  Unlike integer leading
				zeroes, each erased fractional zero still moves the decimal radix;
				fractional_digits is therefore measured from fraction_begin after
				scanning and includes every skipped code unit.  Thus

					0.00D * 10^E = D * 10^(E-3)

				is preserved exactly.  Once significand is nonzero, an intervening
				zero changes the place value of every later digit and must enter
				the multiply-by-ten recurrence.
				*/
				if (!digit_count)
				{
					for (; static_cast<::std::size_t>(end - first) >=
						   sizeof(::std::uint_least64_t);)
					{
						::std::uint_least64_t val;
						::fast_io::freestanding::my_memcpy(
							__builtin_addressof(val), first,
							sizeof(::std::uint_least64_t));
						if constexpr (
							::std::endian::little !=
							::std::endian::native)
						{
							val = ::fast_io::little_endian(val);
						}
						if (val != ascii_zeroes)
						{
							break;
						}
						first += sizeof(::std::uint_least64_t);
						has_digit = true;
					}
					for (; first != end && *first == zero; ++first)
					{
						has_digit = true;
					}
				}
#if (defined(__GNUC__) || defined(__clang__)) && defined(__x86_64__) && \
	!defined(__arm64ec__) && !defined(_M_ARM64EC)
				if (resident_binary32 && end - first < 2 * static_cast<::std::ptrdiff_t>(sizeof(::std::uint_least64_t)))
				{
					scan_short_digit_blocks();
				}
				else
#endif
				{
					for (; digit_count + 8u <= digit_limit &&
						   static_cast<::std::size_t>(end - first) >= sizeof(::std::uint_least64_t);)
					{
						::std::uint_least64_t val;
						::fast_io::freestanding::my_memcpy(__builtin_addressof(val), first, sizeof(::std::uint_least64_t));
						if constexpr (::std::endian::little != ::std::endian::native)
						{
							val = ::fast_io::little_endian(val);
						}
						if (!::fast_io::details::scan_decfloat_ascii8_is_digits(val))
						{
							break;
						}
						significand = significand * 100000000u +
									  static_cast<::std::uint_least64_t>(
										  ::fast_io::details::scan_decfloat_ascii8_parse(val));
						digit_count += 8u;
						first += sizeof(::std::uint_least64_t);
						has_digit = true;
					}
				}
				/*
				This is the same compiler-selected recurrence prefix proved for
				the integer part.  `fractional_digits` is derived from
				first-fraction_begin afterward, so advancing `first` inside the
				peeled step also preserves every radix-place contribution,
				including zeroes.
				*/
				bool fractional_digit_limit_reached{};
				auto scan_one_fractional_digit = [&]() constexpr noexcept {
					if (first == end ||
						!::fast_io::details::scan_decfloat_decimal_digit(*first, digit))
					{
						return false;
					}
					if (digit_count == digit_limit)
					{
						fractional_digit_limit_reached = true;
						return false;
					}
					significand =
						significand * 10u +
						static_cast<::std::uint_least64_t>(digit);
					++digit_count;
					++first;
					has_digit = true;
					return true;
				};
#if defined(__GNUC__) && !defined(__clang__) && 16 <= __GNUC__
				if (scan_one_fractional_digit() &&
					scan_one_fractional_digit() &&
					scan_one_fractional_digit())
#else
				if (scan_one_fractional_digit() &&
					scan_one_fractional_digit() &&
					scan_one_fractional_digit() &&
					scan_one_fractional_digit())
#endif
				{
					while (scan_one_fractional_digit())
					{
					}
				}
				if (fractional_digit_limit_reached)
				{
					// A decimal digit beyond capacity proves that the exact accumulator is necessary.
					return {first, ::fast_io::parse_code::partial, false};
				}
				fractional_digits = static_cast<::std::uint_least64_t>(first - fraction_begin);
			}

			if (!has_digit)
			{
				/*
				No decimal digit means that this speculative ASCII-only path
				owns no lexical result.  Returning unhandled delegates Inf/NaN,
				an isolated radix point, and every invalid prefix to the complete
				scanner below.  That scanner was already the semantic authority
				for non-ASCII, constant-evaluated, and over-limit inputs.

				This convention also lets the caller enter the short scanner
				without first decoding the leading code unit.  A numeric token
				therefore classifies its first digit exactly once rather than
				once in the caller and once here.  The partition is exhaustive:
				a decimal within the short coefficient bound returns handled,
				an over-limit decimal returns unhandled at the exact digit
				boundary, and a token with no digit returns unhandled here.  In
				all unhandled cases the complete scanner restarts at the
				unchanged position following the already-classified optional
				sign, with the saved `negative` bit; an eventual lexical failure
				still reports `begin`.  Thus no speculative cursor, coefficient,
				value or public error position is observable.
				*/
				return {};
			}

			::std::int_least64_t exponent{};
			constexpr auto lower_e{::fast_io::char_literal_v<u8'e', char_type>};
			constexpr auto upper_e{::fast_io::char_literal_v<u8'E', char_type>};
			if constexpr (flags.floating == ::fast_io::manipulators::floating_format::fixed)
			{
			}
			else if (first != end && (*first == lower_e || *first == upper_e))
			{
				auto const *exponent_begin{first};
				auto exponent_result{::fast_io::details::scan_decfloat_exponent(first + 1, end, exponent)};
				if (exponent_result.code == ::fast_io::parse_code::ok)
				{
					first = exponent_result.iter;
				}
				else if constexpr (completion != scan_decfloat_short_completion::contiguous)
				{
					return {};
				}
				else if (::fast_io::details::scan_decfloat_exponent_prefix_may_extend(exponent_begin, end))
				{
					return {end, ::fast_io::parse_code::partial, true};
				}
				else if constexpr (flags.floating == ::fast_io::manipulators::floating_format::scientific)
				{
					return {exponent_result.iter, exponent_result.code, true};
				}
				else
				{
					first = exponent_begin;
				}
			}
			else if constexpr (flags.floating == ::fast_io::manipulators::floating_format::scientific)
			{
				return {first, ::fast_io::parse_code::invalid, true};
			}

			if constexpr (completion == scan_decfloat_short_completion::chunk)
			{
				if (first == end)
				{
					return {};
				}
			}

			if (significand == 0)
			{
				value = negative ? -static_cast<T>(0.0) : static_cast<T>(0.0);
				return {first, ::fast_io::parse_code::ok, true};
			}

			if constexpr (completion != scan_decfloat_short_completion::contiguous && ::std::same_as<T, float> &&
						  scan_decfloat_binary32_like<T>)
			{
				// Integers in this range convert exactly in every rounding mode.
				if (exponent == static_cast<::std::int_least64_t>(fractional_digits) &&
					significand <= (UINT64_C(1) << 24u))
				{
					auto const magnitude{static_cast<float>(static_cast<::std::int_least32_t>(significand))};
					value = negative ? -magnitude : magnitude;
					return {first, ::fast_io::parse_code::ok, true};
				}
			}

			if constexpr (completion != scan_decfloat_short_completion::contiguous && ::std::same_as<T, double> &&
						  scan_decfloat_binary64_like<T>)
			{
				// Every integer in this range converts exactly in every rounding mode.
				if (exponent == static_cast<::std::int_least64_t>(fractional_digits) &&
					significand <= (UINT64_C(1) << 53u))
				{
					auto const magnitude{static_cast<double>(static_cast<::std::int_least64_t>(significand))};
					value = negative ? -magnitude : magnitude;
					return {first, ::fast_io::parse_code::ok, true};
				}
			}

			if constexpr (flags.rounding == ::fast_io::manipulators::floating_rounding::current_environment)
			{
				return {first,
						::fast_io::details::scan_decfloat_assign_short_current_environment(
							value, negative, significand, fractional_digits, exponent),
						true};
			}
			else
			{
				return {first,
						::fast_io::details::scan_decfloat_assign_short<T, flags.rounding>(
							value, negative, significand, fractional_digits, exponent),
						true};
			}
		}
	}
}

template <::std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T,
		  bool use_precision = false>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_contiguous_padding_define_impl(
	char_type const *begin, char_type const *end, T &value,
	::std::size_t precision, ::std::size_t padding) noexcept
{
	auto first{begin};
	constexpr auto plus{::fast_io::char_literal_v<u8'+', char_type>};
	constexpr auto minus{::fast_io::char_literal_v<u8'-', char_type>};
	bool negative{};
	if (first != end && *first == minus)
	{
		negative = true;
		++first;
	}
	else if constexpr (flags.allow_leading_plus)
	{
		if (first != end && *first == plus)
		{
			++first;
		}
	}

	constexpr auto dot{::fast_io::char_literal_v<
		(flags.comma ? u8',' : u8'.'), char_type>};
	if (first != end &&
		::fast_io::details::scan_decfloat_special_start_char(*first))
	{
		auto const special_result{
			::fast_io::details::
				scan_hexfloat_special_value<flags.nan_parse_sign,
											flags.nan_payload_scan>(
					first, end, negative, value)};
		if (special_result.matched)
		{
			return {special_result.iter, special_result.code};
		}
	}

	::fast_io::details::scan_decfloat_significand_state<T>
		significand_state;
	first = ::fast_io::details::scan_decfloat_digits_padding<T, char_type>(
		first, end, false, significand_state, padding);

	if (first != end && *first == dot)
	{
		++first;
		first =
			::fast_io::details::scan_decfloat_digits_padding<T, char_type>(
				first, end, true, significand_state, padding);
	}

	if (!significand_state.has_digit)
	{
		return {begin, ::fast_io::parse_code::invalid};
	}

	::std::int_least64_t exponent{};
	constexpr auto lower_e{
		::fast_io::char_literal_v<u8'e', char_type>};
	constexpr auto upper_e{
		::fast_io::char_literal_v<u8'E', char_type>};
	if constexpr (
		flags.floating ==
		::fast_io::manipulators::floating_format::fixed)
	{
	}
	else if (first != end && (*first == lower_e || *first == upper_e))
	{
		auto const *exponent_begin{first};
		auto exponent_result{
			::fast_io::details::scan_decfloat_exponent(
				first + 1, end, exponent)};
		if (exponent_result.code == ::fast_io::parse_code::ok)
		{
			first = exponent_result.iter;
		}
		else if (
			::fast_io::details::scan_decfloat_exponent_prefix_may_extend(
				exponent_begin, end))
		{
			return {end, ::fast_io::parse_code::partial};
		}
		else if constexpr (
			flags.floating ==
			::fast_io::manipulators::floating_format::scientific)
		{
			return exponent_result;
		}
		else
		{
			first = exponent_begin;
		}
	}
	else if constexpr (
		flags.floating ==
		::fast_io::manipulators::floating_format::scientific)
	{
		return {first, ::fast_io::parse_code::invalid};
	}

	if (!significand_state.has_nonzero_digit)
	{
		value = negative ? -static_cast<T>(0.0) : static_cast<T>(0.0);
		return {first, ::fast_io::parse_code::ok};
	}
	if constexpr (use_precision)
	{
		return {
			first,
			::fast_io::details::
				scan_decfloat_assign_precision<T, flags.precision,
											 flags.rounding>(
					value, negative, significand_state, exponent, precision)};
	}
	else
	{
		if constexpr (
			flags.rounding ==
			::fast_io::manipulators::floating_rounding::
				current_environment)
		{
			return {
				first,
				::fast_io::details::
					scan_decfloat_assign_current_environment(
						value, negative, significand_state, exponent)};
		}
		else
		{
			return {
				first,
				::fast_io::details::
					scan_decfloat_assign<T, flags.rounding>(
						value, negative, significand_state, exponent)};
		}
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T, bool use_precision = false, bool try_short = true>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_contiguous_define_impl(char_type const *begin, char_type const *end, T &value,
									 ::std::size_t precision = 0) noexcept
{
	auto first{begin};
	constexpr auto plus{::fast_io::char_literal_v<u8'+', char_type>};
	constexpr auto minus{::fast_io::char_literal_v<u8'-', char_type>};
	bool negative{};
	if (first != end && *first == minus)
	{
		negative = true;
		++first;
	}
	else if constexpr (flags.allow_leading_plus)
	{
		if (first != end && *first == plus)
		{
			++first;
		}
	}

	if constexpr (!use_precision && try_short)
	{
		/*
		The short scanner is a transactional classifier: `handled` proves that
		its returned pointer/code/value are final, while unhandled exposes none
		of its local state.  Calling it directly removes the former duplicate
		leading-digit decode.  Its proof above shows that every rejected case is
		reparsed by the unchanged complete grammar from the preserved
		post-sign cursor and sign bit; public lexical failure still returns
		`begin`.
		*/
		auto const short_result{
			::fast_io::details::scan_decfloat_contiguous_short_define_impl<
				char_type, flags>(
				first, end, negative, value)};
		if (short_result.handled)
		{
			return {short_result.iter, short_result.code};
		}
	}

	constexpr auto dot{::fast_io::char_literal_v<
		(flags.comma ? u8',' : u8'.'), char_type>};
	if (first != end &&
		::fast_io::details::scan_decfloat_special_start_char(*first))
	{
		auto const special_result{
			::fast_io::details::scan_hexfloat_special_value<flags.nan_parse_sign, flags.nan_payload_scan>(
				first, end, negative, value)};
		if (special_result.matched)
		{
			return {special_result.iter, special_result.code};
		}
	}

	::fast_io::details::scan_decfloat_significand_state<T> significand_state;
	first = ::fast_io::details::scan_decfloat_digits<T, char_type>(first, end, false, significand_state);

	if (first != end && *first == dot)
	{
		++first;
		first = ::fast_io::details::scan_decfloat_digits<T, char_type>(first, end, true, significand_state);
	}

	if (!significand_state.has_digit)
	{
		return {begin, ::fast_io::parse_code::invalid};
	}

	::std::int_least64_t exponent{};
	constexpr auto lower_e{::fast_io::char_literal_v<u8'e', char_type>};
	constexpr auto upper_e{::fast_io::char_literal_v<u8'E', char_type>};
	if constexpr (flags.floating == ::fast_io::manipulators::floating_format::fixed)
	{
	}
	else if (first != end && (*first == lower_e || *first == upper_e))
	{
		auto const *exponent_begin{first};
		auto exponent_result{::fast_io::details::scan_decfloat_exponent(first + 1, end, exponent)};
		if (exponent_result.code == ::fast_io::parse_code::ok)
		{
			first = exponent_result.iter;
		}
		else if (::fast_io::details::scan_decfloat_exponent_prefix_may_extend(exponent_begin, end))
		{
			return {end, ::fast_io::parse_code::partial};
		}
		else if constexpr (flags.floating == ::fast_io::manipulators::floating_format::scientific)
		{
			return exponent_result;
		}
		else
		{
			first = exponent_begin;
		}
	}
	else if constexpr (flags.floating == ::fast_io::manipulators::floating_format::scientific)
	{
		return {first, ::fast_io::parse_code::invalid};
	}

	if (!significand_state.has_nonzero_digit)
	{
		value = negative ? -static_cast<T>(0.0) : static_cast<T>(0.0);
		return {first, ::fast_io::parse_code::ok};
	}
	if constexpr (use_precision)
	{
		return {first, ::fast_io::details::scan_decfloat_assign_precision<T, flags.precision, flags.rounding>(
						   value, negative, significand_state, exponent, precision)};
	}
	else
	{
		if constexpr (flags.rounding == ::fast_io::manipulators::floating_rounding::current_environment)
		{
			return {first, ::fast_io::details::scan_decfloat_assign_current_environment(
							   value, negative, significand_state, exponent)};
		}
		else
		{
			return {first, ::fast_io::details::scan_decfloat_assign<T, flags.rounding>(
							   value, negative, significand_state, exponent)};
		}
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T, bool use_precision = false>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_contiguous_define(char_type const *begin, char_type const *end, T &value,
								::std::size_t precision = 0) noexcept
{
	if constexpr (!flags.noskipws)
	{
		if (begin == end)
		{
			return {begin, ::fast_io::parse_code::end_of_file};
		}
		bool has_space{};
		if constexpr (sizeof(char_type) == sizeof(char8_t) && ::fast_io::details::is_ascii<char_type>)
		{
			using unsigned_char_type = ::fast_io::details::my_make_unsigned_t<char_type>;
			auto const ch{static_cast<unsigned_char_type>(*begin)};
			has_space = ch <= static_cast<unsigned_char_type>(u8' ') &&
						::fast_io::char_category::is_c_space(*begin);
		}
		else
		{
			has_space = ::fast_io::char_category::is_c_space(*begin);
		}
		if (has_space)
		{
			begin = ::fast_io::details::find_space_common_impl<false, true>(begin, end);
			if (begin == end)
			{
				return {begin, ::fast_io::parse_code::end_of_file};
			}
		}
	}

	return ::fast_io::details::scan_decfloat_contiguous_define_impl<char_type, flags, T, use_precision>(
		begin, end, value, precision);
}

template <::std::integral char_type,
		  scan_decfloat_supported_floating_point T>
inline constexpr bool scan_decfloat_terminal_padding_leaf_available{
#if ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	 !(defined(__arm64ec__) || defined(_M_ARM64EC))) &&                  \
	!defined(__AVX512F__) &&                                            \
	(defined(__AVX2__) || defined(__SSE2__))
	sizeof(char_type) == sizeof(char8_t) &&
		::fast_io::details::is_ascii<char_type> &&
#if defined(__GNUC__) && !defined(__clang__)
		/*
		GCC binary32/binary64 amortize the padded parser body on the Linux
		P-core matrix.  Its binary16/bfloat16, binary80, and binary128 bodies do
		not, so their padded CPO shares the ordinary public implementation.
		*/
		(23u <= ::fast_io::details::
					 iec559_traits<::std::remove_cvref_t<T>>::mbits &&
		 ::fast_io::details::
				 iec559_traits<::std::remove_cvref_t<T>>::mbits <= 52u)
#elif defined(__clang__)
		/*
		Clang profits for binary16/bfloat16, binary32, and binary128.  Its
		binary64/binary80 padded CPO likewise shares the ordinary public body.
		*/
		(::fast_io::details::
			 iec559_traits<::std::remove_cvref_t<T>>::mbits <= 23u ||
		 ::fast_io::details::
			 iec559_traits<::std::remove_cvref_t<T>>::mbits == 112u)
#else
		true
#endif
#else
	false
#endif
};

template <::std::integral char_type,
		  scan_decfloat_supported_floating_point T>
inline constexpr bool
	scan_decfloat_terminal_padding_dispatch_available{
		scan_decfloat_terminal_padding_leaf_available<char_type, T> &&
#if defined(__GNUC__) && !defined(__clang__)
	/*
	No measured GCC decimal type amortizes terminal dispatch against the
	unchanged public context baseline.  Callers may still invoke the padded CPO
	directly, while padded views retain ordinary public dispatch.
	*/
		false
#elif defined(__clang__)
		/*
		Clang binary16/bfloat16 amortize terminal dispatch at the public view
		boundary.  Binary32 and wider types keep the ordinary context path.
		*/
		::fast_io::details::
			iec559_traits<::std::remove_cvref_t<T>>::mbits <= 10u
#else
		false
#endif
};

template <::std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T,
		  bool use_precision = false>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_contiguous_padding_define(
	char_type const *begin, char_type const *end, T &value,
	::std::size_t precision, ::std::size_t padding) noexcept
{
	if constexpr (
		!::fast_io::details::
			scan_decfloat_terminal_padding_leaf_available<char_type, T>)
	{
		(void)padding;
		return ::fast_io::details::
			scan_decfloat_contiguous_define<char_type, flags, T,
										   use_precision>(
				begin, end, value, precision);
	}
	else
	{
		/*
		The padded parser can help only after the exact retained-prefix capacity
		has been crossed.  Keeping shorter inputs on the ordinary entry avoids a
		second large parser body in the common decimal path.
		*/
		constexpr ::std::size_t minimum_semantic_size{
			::fast_io::details::scan_decfloat_exact_digit_capacity<T> +
			1u};
		if (padding == 0u ||
			static_cast<::std::size_t>(end - begin) <
				minimum_semantic_size)
		{
			return ::fast_io::details::
				scan_decfloat_contiguous_define<char_type, flags, T,
											   use_precision>(
					begin, end, value, precision);
		}
		if constexpr (!flags.noskipws)
		{
			if (begin == end)
			{
				return {begin, ::fast_io::parse_code::end_of_file};
			}
			bool has_space{};
			if constexpr (
				sizeof(char_type) == sizeof(char8_t) &&
				::fast_io::details::is_ascii<char_type>)
			{
				using unsigned_char_type =
					::fast_io::details::my_make_unsigned_t<char_type>;
				auto const ch{
					static_cast<unsigned_char_type>(*begin)};
				has_space =
					ch <= static_cast<unsigned_char_type>(u8' ') &&
					::fast_io::char_category::is_c_space(*begin);
			}
			else
			{
				has_space =
					::fast_io::char_category::is_c_space(*begin);
			}
			if (has_space)
			{
				begin =
					::fast_io::details::find_space_common_impl<
						false, true>(begin, end);
				if (begin == end)
				{
					return {
						begin,
						::fast_io::parse_code::end_of_file};
				}
			}
		}
		return ::fast_io::details::
			scan_decfloat_contiguous_padding_define_impl<
				char_type, flags, T, use_precision>(
				begin, end, value, precision, padding);
	}
}

enum class scan_decfloat_context_phase : ::std::uint_least8_t
{
	start,
	after_sign,
	integer,
	fraction,
	exponent_start,
	exponent_after_sign,
	exponent_digits,
	special
};

template <::std::integral char_type, typename T = double>
struct scan_decfloat_context
{
	::fast_io::details::scan_decfloat_significand_state<T> significand_state;
	::fast_io::details::scan_floating_context<char_type> special_buffer;
	::std::uint_least64_t exponent{};
	::fast_io::details::scan_decfloat_context_phase phase{};
	char_type sign_char{};
	bool negative{};
	bool has_sign{};
	bool has_decimal_point{};
	bool has_exponent_marker{};
	bool exponent_negative{};
	bool exponent_has_digit{};
	bool exponent_overflow{};
	bool special_sign_prefixed{};

	/**
	 * @brief Starts an x87 refill without clearing the 12,000-byte decimal digit array.
	 * @details Each append writes a digit before advancing exact_stored_digits. The converter
	 * reads only that prefix, including when a later refill supplies the digit after a midpoint.
	 * Default member initializers reset the counters and flags. Value-initialize the special
	 * token buffer and retain fully initialized storage during constant evaluation.
	 */
	constexpr scan_decfloat_context() noexcept requires fp_floating_point_is_float80<T>
		: special_buffer{}
	{
		if (::std::is_constant_evaluated())
		{
			for (auto &digit : significand_state.exact_digits)
			{
				digit = 0;
			}
		}
	}
	constexpr scan_decfloat_context() noexcept requires (!fp_floating_point_is_float80<T>) = default;
};

template <::std::integral char_type, typename T>
inline constexpr void scan_decfloat_context_append_exponent_digit(
	::fast_io::details::scan_decfloat_context<char_type, T> &state, char8_t digit) noexcept
{
	state.exponent_has_digit = true;
	constexpr auto exponent_limit{
		static_cast<::std::uint_least64_t>((::std::numeric_limits<::std::int_least64_t>::max)())};
	auto const value{static_cast<::std::uint_least64_t>(digit)};
	if (state.exponent > (exponent_limit - value) / 10u)
	{
		state.exponent = exponent_limit;
		state.exponent_overflow = true;
	}
	else if (!state.exponent_overflow)
	{
		state.exponent = state.exponent * 10u + value;
	}
}

template <::std::integral char_type, typename T>
[[nodiscard]] inline constexpr ::std::int_least64_t
scan_decfloat_context_exponent(::fast_io::details::scan_decfloat_context<char_type, T> const &state) noexcept
{
	return state.exponent_negative ? -static_cast<::std::int_least64_t>(state.exponent)
								   : static_cast<::std::int_least64_t>(state.exponent);
}

template <typename T, ::fast_io::manipulators::scalar_flags flags, bool use_precision,
		  ::std::integral char_type>
[[nodiscard]] inline constexpr ::fast_io::parse_code
scan_decfloat_context_assign(::fast_io::details::scan_decfloat_context<char_type, T> const &state, T &value,
							 ::std::size_t precision = 0) noexcept
{
	if (!state.significand_state.has_digit)
	{
		return ::fast_io::parse_code::invalid;
	}
	if constexpr (flags.floating == ::fast_io::manipulators::floating_format::scientific)
	{
		if (!state.has_exponent_marker || !state.exponent_has_digit)
		{
			return ::fast_io::parse_code::invalid;
		}
	}
	if (!state.significand_state.has_nonzero_digit)
	{
		value = state.negative ? -static_cast<T>(0.0) : static_cast<T>(0.0);
		return ::fast_io::parse_code::ok;
	}
	auto const exponent{state.exponent_has_digit
							? ::fast_io::details::scan_decfloat_context_exponent(state)
							: ::std::int_least64_t{}};
	if constexpr (use_precision)
	{
		return ::fast_io::details::scan_decfloat_assign_precision<T, flags.precision, flags.rounding>(
			value, state.negative, state.significand_state, exponent, precision);
	}
	else
	{
		if constexpr (flags.rounding == ::fast_io::manipulators::floating_rounding::current_environment)
		{
			return ::fast_io::details::scan_decfloat_assign_current_environment(
				value, state.negative, state.significand_state, exponent);
		}
		else
		{
			return ::fast_io::details::scan_decfloat_assign<T, flags.rounding>(
				value, state.negative, state.significand_state, exponent);
		}
	}
}

template <::std::integral char_type, typename T>
[[nodiscard]] inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_context_append_special_prefix(::fast_io::details::scan_decfloat_context<char_type, T> &state,
											char_type const *chunk_begin) noexcept
{
	if (state.has_sign && !state.special_sign_prefixed)
	{
		if (!state.special_buffer.reserve(state.special_buffer.size + 1u))
		{
			return {chunk_begin, ::fast_io::parse_code::overflow};
		}
		state.special_buffer.data()[state.special_buffer.size] = state.sign_char;
		++state.special_buffer.size;
		state.special_sign_prefixed = true;
	}
	return {chunk_begin, ::fast_io::parse_code::partial};
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T, bool use_precision>
[[nodiscard]] inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_context_special_define(::fast_io::details::scan_decfloat_context<char_type, T> &state,
									 char_type const *begin, char_type const *end, T &value,
									 ::std::size_t precision = 0) noexcept
{
	auto prefix_result{::fast_io::details::scan_decfloat_context_append_special_prefix(state, begin)};
	if (prefix_result.code != ::fast_io::parse_code::partial)
	{
		return prefix_result;
	}
	auto const old_size{state.special_buffer.size};
	auto const append_result{::fast_io::details::scan_floating_context_append(state.special_buffer, begin, end)};
	if (append_result.code != ::fast_io::parse_code::partial)
	{
		return {append_result.iter, append_result.code};
	}
	T parsed_value{};
	auto const *buffer_begin{state.special_buffer.data()};
	auto const *buffer_end{buffer_begin + state.special_buffer.size};
	::fast_io::parse_result<char_type const *> parse_result;
	if constexpr (use_precision)
	{
		parse_result = ::fast_io::details::scan_decfloat_contiguous_define<char_type, flags, T, true>(
			buffer_begin, buffer_end, parsed_value, precision);
	}
	else
	{
		parse_result = ::fast_io::details::scan_decfloat_contiguous_define<char_type, flags>(
			buffer_begin, buffer_end, parsed_value);
	}
	if (parse_result.code == ::fast_io::parse_code::ok)
	{
		if (parse_result.iter == buffer_end)
		{
			if (::fast_io::details::scan_hexfloat_special_end_may_extend<flags.nan_payload_scan>(
					buffer_begin, buffer_end))
			{
				if (append_result.truncated)
				{
					return {append_result.iter, ::fast_io::parse_code::overflow};
				}
				return {end, ::fast_io::parse_code::partial};
			}
			::fast_io::details::scan_floating_commit_special(value, parsed_value);
			return {::fast_io::details::scan_floating_context_map_iter(
						state.special_buffer, old_size, begin, append_result.iter, parse_result.iter),
					::fast_io::parse_code::ok};
		}
		if (::fast_io::details::scan_hexfloat_special_parse_may_extend<flags.nan_payload_scan>(
				parse_result.iter, buffer_end))
		{
			return {end, ::fast_io::parse_code::partial};
		}
		::fast_io::details::scan_floating_commit_special(value, parsed_value);
		return {::fast_io::details::scan_floating_context_map_iter(
					state.special_buffer, old_size, begin, end, parse_result.iter),
				::fast_io::parse_code::ok};
	}
	if (parse_result.iter == buffer_end)
	{
		if (append_result.truncated)
		{
			return {append_result.iter, ::fast_io::parse_code::overflow};
		}
		return {end, ::fast_io::parse_code::partial};
	}
	if (parse_result.code == ::fast_io::parse_code::end_of_file ||
		parse_result.code == ::fast_io::parse_code::partial)
	{
		if (append_result.truncated)
		{
			return {append_result.iter, ::fast_io::parse_code::overflow};
		}
		return {end, ::fast_io::parse_code::partial};
	}
	if (parse_result.code == ::fast_io::parse_code::invalid)
	{
		if (::fast_io::details::
				scan_hexfloat_special_invalid_prefix_may_extend<flags.allow_leading_plus>(
					buffer_begin, buffer_end))
		{
			if (append_result.truncated)
			{
				return {append_result.iter, ::fast_io::parse_code::overflow};
			}
			return {end, ::fast_io::parse_code::partial};
		}
	}
	return {::fast_io::details::scan_floating_context_map_iter(
				state.special_buffer, old_size, begin, end, parse_result.iter),
			parse_result.code};
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T, bool use_precision>
[[nodiscard]] inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_context_numeric_define(::fast_io::details::scan_decfloat_context<char_type, T> &state,
									 char_type const *first, char_type const *end, T &value,
									 ::std::size_t precision) noexcept;

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T, bool use_precision>
[[nodiscard]] inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_context_finish_or_exponent(::fast_io::details::scan_decfloat_context<char_type, T> &state,
										 char_type const *first, char_type const *end, T &value,
										 ::std::size_t precision = 0) noexcept
{
	constexpr auto lower_e{::fast_io::char_literal_v<u8'e', char_type>};
	constexpr auto upper_e{::fast_io::char_literal_v<u8'E', char_type>};
	if constexpr (flags.floating != ::fast_io::manipulators::floating_format::fixed)
	{
		if (first != end && (*first == lower_e || *first == upper_e))
		{
			state.has_exponent_marker = true;
			state.phase = ::fast_io::details::scan_decfloat_context_phase::exponent_start;
			++first;
			if (first == end)
			{
				return {first, ::fast_io::parse_code::partial};
			}
			return ::fast_io::details::scan_decfloat_context_numeric_define<char_type, flags, T, use_precision>(
				state, first, end, value, precision);
		}
	}
	auto code{::fast_io::details::scan_decfloat_context_assign<T, flags, use_precision>(state, value, precision)};
	return {first, code};
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T, bool use_precision>
[[nodiscard]] inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_context_numeric_define(::fast_io::details::scan_decfloat_context<char_type, T> &state,
									 char_type const *first, char_type const *end, T &value,
									 ::std::size_t precision) noexcept
{
	constexpr auto plus{::fast_io::char_literal_v<u8'+', char_type>};
	constexpr auto minus{::fast_io::char_literal_v<u8'-', char_type>};
	constexpr auto dot{::fast_io::char_literal_v<
		(flags.comma ? u8',' : u8'.'), char_type>};
	for (;;)
	{
		switch (state.phase)
		{
		case ::fast_io::details::scan_decfloat_context_phase::start:
			if (!state.significand_state.has_digit && !state.has_sign)
			{
				first = ::fast_io::details::scan_floating_context_skip_space<char_type, flags.noskipws>(first, end);
				if (first == end)
				{
					return {end, ::fast_io::parse_code::partial};
				}
			}
			if (*first == minus)
			{
				state.negative = true;
				state.has_sign = true;
				state.sign_char = *first;
				++first;
				state.phase = ::fast_io::details::scan_decfloat_context_phase::after_sign;
				if (first == end)
				{
					return {end, ::fast_io::parse_code::partial};
				}
				break;
			}
			if constexpr (flags.allow_leading_plus)
			{
				if (*first == plus)
				{
					state.has_sign = true;
					state.sign_char = *first;
					++first;
					state.phase = ::fast_io::details::scan_decfloat_context_phase::after_sign;
					if (first == end)
					{
						return {end, ::fast_io::parse_code::partial};
					}
					break;
				}
			}
			state.phase = ::fast_io::details::scan_decfloat_context_phase::integer;
			break;
		case ::fast_io::details::scan_decfloat_context_phase::after_sign:
			if (first == end)
			{
				return {end, ::fast_io::parse_code::partial};
			}
			state.phase = ::fast_io::details::scan_decfloat_context_phase::integer;
			break;
		case ::fast_io::details::scan_decfloat_context_phase::integer:
		{
			char8_t digit{};
			if (first != end && *first == dot)
			{
				state.has_decimal_point = true;
				state.phase = ::fast_io::details::scan_decfloat_context_phase::fraction;
				++first;
				if (first == end)
				{
					return {end, ::fast_io::parse_code::partial};
				}
				break;
			}
			auto const *before_digits{first};
			first = ::fast_io::details::scan_decfloat_digits<T, char_type>(
				first, end, false, state.significand_state);
			if (first == end)
			{
				return {end, ::fast_io::parse_code::partial};
			}
			if (first == before_digits && !state.significand_state.has_digit &&
				!::fast_io::details::scan_decfloat_decimal_digit(*first, digit))
			{
				if (::fast_io::details::scan_decfloat_special_start_char(*first))
				{
					state.phase = ::fast_io::details::scan_decfloat_context_phase::special;
					return ::fast_io::details::scan_decfloat_context_special_define<char_type, flags, T, use_precision>(
						state, first, end, value, precision);
				}
				return {first, ::fast_io::parse_code::invalid};
			}
			if (first != end && *first == dot)
			{
				state.has_decimal_point = true;
				state.phase = ::fast_io::details::scan_decfloat_context_phase::fraction;
				++first;
				if (first == end)
				{
					return {end, ::fast_io::parse_code::partial};
				}
				break;
			}
			return ::fast_io::details::scan_decfloat_context_finish_or_exponent<char_type, flags, T, use_precision>(
				state, first, end, value, precision);
		}
		case ::fast_io::details::scan_decfloat_context_phase::fraction:
			first = ::fast_io::details::scan_decfloat_digits<T, char_type>(
				first, end, true, state.significand_state);
			if (first == end)
			{
				return {end, ::fast_io::parse_code::partial};
			}
			if (!state.significand_state.has_digit)
			{
				return {first, ::fast_io::parse_code::invalid};
			}
			return ::fast_io::details::scan_decfloat_context_finish_or_exponent<char_type, flags, T, use_precision>(
				state, first, end, value, precision);
		case ::fast_io::details::scan_decfloat_context_phase::exponent_start:
			if (first == end)
			{
				return {end, ::fast_io::parse_code::partial};
			}
			if (*first == minus)
			{
				state.exponent_negative = true;
				++first;
				state.phase = ::fast_io::details::scan_decfloat_context_phase::exponent_after_sign;
				if (first == end)
				{
					return {end, ::fast_io::parse_code::partial};
				}
				break;
			}
			if (*first == plus)
			{
				++first;
				state.phase = ::fast_io::details::scan_decfloat_context_phase::exponent_after_sign;
				if (first == end)
				{
					return {end, ::fast_io::parse_code::partial};
				}
				break;
			}
			state.phase = ::fast_io::details::scan_decfloat_context_phase::exponent_digits;
			break;
		case ::fast_io::details::scan_decfloat_context_phase::exponent_after_sign:
			if (first == end)
			{
				return {end, ::fast_io::parse_code::partial};
			}
			state.phase = ::fast_io::details::scan_decfloat_context_phase::exponent_digits;
			break;
		case ::fast_io::details::scan_decfloat_context_phase::exponent_digits:
		{
			char8_t digit{};
			for (; first != end && ::fast_io::details::scan_decfloat_decimal_digit(*first, digit); ++first)
			{
				::fast_io::details::scan_decfloat_context_append_exponent_digit(state, digit);
			}
			if (first == end)
			{
				return {end, ::fast_io::parse_code::partial};
			}
			if (!state.exponent_has_digit)
			{
				if constexpr (flags.floating == ::fast_io::manipulators::floating_format::scientific)
				{
					return {first, ::fast_io::parse_code::invalid};
				}
			}
			auto code{::fast_io::details::scan_decfloat_context_assign<T, flags, use_precision>(
				state, value, precision)};
			return {first, code};
		}
		case ::fast_io::details::scan_decfloat_context_phase::special:
			return ::fast_io::details::scan_decfloat_context_special_define<char_type, flags, T, use_precision>(
				state, first, end, value, precision);
		}
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T, bool use_precision>
[[nodiscard]] inline constexpr ::fast_io::parse_code
scan_decfloat_context_eof(::fast_io::details::scan_decfloat_context<char_type, T> &state, T &value,
						  ::std::size_t precision = 0) noexcept
{
	if (state.phase == ::fast_io::details::scan_decfloat_context_phase::special)
	{
		if (!state.special_buffer.size)
		{
			return ::fast_io::parse_code::end_of_file;
		}
		T parsed_value{};
		auto const *buffer_begin{state.special_buffer.data()};
		auto const *buffer_end{buffer_begin + state.special_buffer.size};
		::fast_io::parse_result<char_type const *> parse_result;
		if constexpr (use_precision)
		{
			parse_result = ::fast_io::details::scan_decfloat_contiguous_define<char_type, flags, T, true>(
				buffer_begin, buffer_end, parsed_value, precision);
		}
		else
		{
			parse_result = ::fast_io::details::scan_decfloat_contiguous_define<char_type, flags>(
				buffer_begin, buffer_end, parsed_value);
		}
		if (parse_result.code == ::fast_io::parse_code::ok)
		{
			::fast_io::details::scan_floating_commit_special(value, parsed_value);
		}
		return parse_result.code;
	}
	if (state.phase == ::fast_io::details::scan_decfloat_context_phase::start && !state.significand_state.has_digit &&
		!state.has_sign)
	{
		return ::fast_io::parse_code::end_of_file;
	}
	return ::fast_io::details::scan_decfloat_context_assign<T, flags, use_precision>(state, value, precision);
}

} // namespace details

template <details::scan_decfloat_supported_floating_point T>
inline constexpr ::fast_io::manipulators::scalar_manip_t<::fast_io::manipulators::floating_point_default_scalar_flags,
														 T &>
scan_alias_define(io_alias_t, T &value) noexcept
{
	return {value};
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::fast_io::parse_result<char_type const *>
scan_contiguous_define(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>,
					   char_type const *begin, char_type const *end,
					   ::fast_io::manipulators::scalar_manip_t<flags, T &> value) noexcept
{
	return ::fast_io::details::scan_decfloat_contiguous_define<char_type, flags>(begin, end, value.reference);
}

/*
Let B and E be the original semantic endpoints. Whitespace, sign,
significand, radix-point, and exponent handling advance only by guarded
pointer iteration over [B,E]; malformed optional exponents rewind only to a
saved cursor from that same span. The speculative short and special-value
classifiers publish an iterator only when handled or matched, so their null
miss sentinels are unobservable. Numeric conversion returns only a parse_code
and therefore cannot replace the fixed lexical cursor. Hence every public
result has the form B+k, 0 <= k <= E-B, with B's array provenance,
independently of success, partial input, EOF, invalid syntax, Inf/NaN,
overflow, underflow, or rounding policy.

This proof belongs only to the ordinary decimal scalar CPO. Precision,
padding, context-state, and hexadecimal scanners have independent protocols
and cannot inherit it from a shared floating representation.
*/
template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::std::true_type scan_contiguous_result_in_range(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_t<flags, T &>>) noexcept
{
	return {};
}

#if defined(__i386__) && defined(__ELF__)
namespace details
{
/**
 * @brief Accumulates up to nine single-byte ASCII digits on little-endian i386.
 * @details Four-byte loads stay within the span. A word containing a nondigit
 * falls back to bounded byte steps from the same cursor. The accumulator fits
 * in 32 bits and the first nondigit remains unconsumed.
 */
template <unsigned max_digits, ::std::integral Char>
	requires(sizeof(Char) == 1 && ::fast_io::details::is_ascii<Char>)
[[nodiscard]] inline constexpr unsigned scan_x87_chunk_digits(
	Char const *&first, Char const *end) noexcept
{
	static_assert(max_digits <= 9u && sizeof(unsigned) == 4 &&
				  ::std::endian::native == ::std::endian::little);
	unsigned value{};
	unsigned limit{max_digits};
	if constexpr (max_digits >= 4u)
	{
		for (; limit >= 4u && end - first >= 4; limit -= 4u, first += 4)
		{
			unsigned packed;
			::fast_io::freestanding::my_memcpy(__builtin_addressof(packed), first, sizeof(packed));
			auto const digits{packed - 0x30303030u};
			if (((digits | (packed + 0x46464646u)) & 0x80808080u) != 0u)
			{
				break;
			}
			auto const pairs{(digits * 10u + (digits >> 8u)) & 0x00ff00ffu};
			value = 10000u * value + (pairs & 255u) * 100u + (pairs >> 16u);
		}
	}
	for (; limit && first != end; --limit, ++first)
	{
		auto const digit{static_cast<unsigned>(static_cast<unsigned char>(*first)) - u8'0'};
		if (digit > 9u)
		{
			break;
		}
		value = value * 10u + digit;
	}
	return value;
}

/** @brief Continues a bounded coefficient from the first digit beyond its native prefix. */
template <::std::integral Char>
	requires(sizeof(Char) == 1 && ::fast_io::details::is_ascii<Char>)
[[nodiscard]] inline constexpr bool scan_x87_extend_chunk_coefficient(Char const *&first, Char const *end,
	::std::uint_least64_t &coefficient, unsigned &digits) noexcept
{
	constexpr unsigned capacity{static_cast<unsigned>(scan_decfloat_uint64_significand_digit_limit)};
	if (coefficient == 0u)
	{
		digits = 0u;
	}
	while (coefficient != 0u && capacity - digits >= 4u && end - first >= 4)
	{
		auto const *const previous{first};
		auto const suffix{scan_x87_chunk_digits<4>(first, end)};
		auto const added{static_cast<unsigned>(first - previous)};
		if (added == 0u)
		{
			return true;
		}
		coefficient = coefficient * static_cast<unsigned>(scan_decfloat_pow10_0_to_8_table[added]) + suffix;
		digits += added;
		if (added != 4u)
		{
			return true;
		}
	}
	while (first != end)
	{
		auto const digit{static_cast<unsigned>(static_cast<unsigned char>(*first)) - u8'0'};
		if (digit > 9u)
		{
			break;
		}
		if (digits == capacity && coefficient != 0u)
		{
			return false;
		}
		coefficient = coefficient * 10u + digit;
		digits = coefficient == 0u ? 0u : digits + 1u;
		++first;
	}
	return true;
}

/**
 * @brief Resolves a complete i386 x87 coefficient after the cached converter declines it.
 */
template <typename T, ::fast_io::manipulators::floating_rounding rounding>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
[[nodiscard]] inline constexpr ::fast_io::parse_code
scan_x87_assign_chunk_exact(T &value, bool negative, ::std::uint_least64_t coefficient,
							::std::int_least64_t exponent) noexcept
{
	// Recover the exact dyadic that can land on a cached-product boundary.
	// A nonzero 64-bit coefficient can contain at most 27 factors of five.
	if (-27 <= exponent && exponent < 0)
	{
		auto const divisor{scan_decfloat_pow5_0_to_27_table[static_cast<unsigned>(-exponent)]};
		auto const quotient{coefficient / divisor};
		if (quotient * divisor == coefficient)
		{
			auto const leading{::std::countl_zero(quotient)};
			auto const encoded{static_cast<::std::uint_least32_t>(exponent + 16383 + 63 - leading)};
			fp_assign_float80_bits(value, quotient << leading, encoded, negative);
			return ::fast_io::parse_code::ok;
		}
	}
	return scan_decfloat_assign_wide_short_exact<T, rounding>(value, negative, coefficient, exponent);
}

/**
 * @brief Commits a resident ordinary decimal field to x87 on little-endian i386.
 * @details The caller handles whitespace and the leading sign. Single-byte
 * ASCII input and a known terminal code unit give the same grammar as the full
 * scanner. The radix character follows flags.comma. The 32-bit prefixes
 * accumulate in groups of up to nine digits. A longer coefficient continues
 * from the saved cursor in 64 bits. A longer exponent also resumes at its
 * saved cursor, with saturation beyond the representable x87 range.
 * A miss returns nullptr and leaves the destination untouched.
 */
template <::std::integral Char, ::fast_io::manipulators::scalar_flags flags, typename T>
	requires(sizeof(Char) == 1 && ::fast_io::details::is_ascii<Char>)
[[nodiscard]] inline constexpr Char const *scan_x87_short_chunk(
	Char const *first, Char const *end, bool negative, T &value) noexcept
{
	constexpr auto radix{char_literal_v<(flags.comma ? u8',' : u8'.'), Char>};
	constexpr auto e_lower{char_literal_v<u8'e', Char>};
	constexpr auto e_upper{char_literal_v<u8'E', Char>};
	constexpr auto is_digit{[](Char ch) {
		return static_cast<unsigned>(static_cast<unsigned char>(ch)) - u8'0' <= 9u;
	}};
	auto const *const integer_begin{first};
	::std::uint_least64_t coefficient{scan_x87_chunk_digits<9>(first, end)};
	auto has_digit{first != integer_begin};
	auto coefficient_digits{static_cast<unsigned>(first - integer_begin)};
	if (first == end)
	{
		return nullptr;
	}
	if (is_digit(*first))
	{
		auto const *first_nonzero{integer_begin};
		while (first_nonzero != first && *first_nonzero == char_literal_v<u8'0', Char>)
		{
			++first_nonzero;
		}
		coefficient_digits = static_cast<unsigned>(first - first_nonzero);
		if (!scan_x87_extend_chunk_coefficient(first, end, coefficient, coefficient_digits) || first == end)
		{
			return nullptr;
		}
	}
	::std::uint_least32_t fraction_digits{};
	if (*first == radix)
	{
		auto const *const fraction_begin{++first};
		auto const fraction{scan_x87_chunk_digits<8>(first, end)};
		auto const initial_fraction_digits{static_cast<unsigned>(first - fraction_begin)};
		if (first == end || (coefficient != 0u && coefficient_digits + initial_fraction_digits >
			scan_decfloat_uint64_significand_digit_limit))
		{
			return nullptr;
		}
		if (coefficient == 0u && fraction != 0u)
		{
			auto const *first_nonzero{fraction_begin};
			while (*first_nonzero == char_literal_v<u8'0', Char>)
			{
				++first_nonzero;
			}
			coefficient_digits = static_cast<unsigned>(first - first_nonzero);
		}
		else if (coefficient != 0u)
		{
			coefficient_digits += initial_fraction_digits;
		}
		coefficient = coefficient * static_cast<unsigned>(scan_decfloat_pow10_0_to_8_table[initial_fraction_digits]) + fraction;
		if (is_digit(*first) &&
			(!scan_x87_extend_chunk_coefficient(first, end, coefficient, coefficient_digits) || first == end))
		{
			return nullptr;
		}
		fraction_digits = static_cast<::std::uint_least32_t>(first - fraction_begin);
		has_digit |= fraction_digits != 0u;
	}
	if (!has_digit)
	{
		return nullptr;
	}
	::std::int_least64_t exponent{-static_cast<::std::int_least64_t>(fraction_digits)};
	if (*first == e_lower || *first == e_upper)
	{
		if (++first == end)
		{
			return nullptr;
		}
		constexpr auto minus{char_literal_v<u8'-', Char>};
		constexpr auto plus{char_literal_v<u8'+', Char>};
		auto const negative_exponent{*first == minus};
		if (negative_exponent || *first == plus)
		{
			++first;
		}
		auto const *const exponent_begin{first};
		auto magnitude{scan_x87_chunk_digits<3>(first, end)};
		if (first == exponent_begin || first == end)
		{
			return nullptr;
		}
		// A 32-bit span limits the number of fractional digits to
		// PTRDIFF_MAX. A saturated exponent still stays outside the
		// supported x87 range after removing that many decimal places.
		constexpr ::std::uint_least64_t magnitude_limit{1ULL << 32u};
		::std::uint_least64_t extended_magnitude{magnitude};
		if (is_digit(*first))
		{
			do
			{
				auto const digit{static_cast<unsigned>(static_cast<unsigned char>(*first)) - u8'0'};
				if (digit > 9u)
				{
					break;
				}
				if (extended_magnitude < magnitude_limit)
				{
					extended_magnitude = extended_magnitude * 10u + digit;
				}
			} while (++first != end);
			if (first == end)
			{
				return nullptr;
			}
		}
		auto const signed_magnitude{static_cast<::std::int_least64_t>(extended_magnitude)};
		exponent += negative_exponent ? -signed_magnitude : signed_magnitude;
	}
	if (*first == e_lower || *first == e_upper)
	{
		return nullptr;
	}
	if (!coefficient || (!exponent && coefficient <= static_cast<::std::uint_least64_t>(
		(::std::numeric_limits<::std::int_least64_t>::max)())))
	{
		auto const integer_value{static_cast<T>(static_cast<::std::int_least64_t>(coefficient))};
		value = negative ? -integer_value : integer_value;
		return first;
	}
	if (0 < exponent && exponent <= 13)
	{
		auto const power5{static_cast<::std::uint_least32_t>(scan_decfloat_pow5_0_to_27_table[static_cast<unsigned>(exponent)])};
		::std::uint_least32_t high{};
		auto const product{::fast_io::intrinsics::umul(power5, coefficient, high)};
		if (high == 0u)
		{
			auto const leading{::std::countl_zero(product)};
			auto const power{static_cast<::std::uint_least32_t>(exponent + 16383 + 63 - leading)};
			fp_assign_float80_bits(value, product << leading, power, negative);
			return first;
		}
	}
	if (scan_decfloat_try_x87_cached<flags.rounding, T, true>(value, negative, coefficient, exponent))
	{
		return first;
	}
	T converted;
	if (scan_x87_assign_chunk_exact<T, flags.rounding>(converted, negative, coefficient, exponent) ==
		::fast_io::parse_code::ok)
	{
		value = converted;
		return first;
	}
	return nullptr;
}
}
#endif

/** @brief Offers an ordinary decimal spelling to the contiguous short scanner as soon as a chunk contains input. */
template <::std::integral Char, ::fast_io::manipulators::scalar_flags flags, details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat &&
			 sizeof(Char) == sizeof(char8_t) && details::is_ascii<Char>)
inline constexpr ::std::size_t scan_context_current_chunk_minimum_size(
	io_reserve_type_t<Char, manipulators::scalar_manip_t<flags, T &>>) noexcept
{
	return 1u;
}

namespace details
{
/**
 * @brief Completes a short Infinity or NaN spelling directly in borrowed terminal storage.
 * @details The same special-value parser produces the sign and payload bits for decimal and
 * hexadecimal input. A matching word requires its real delimiter or EOF. Restrict lookahead
 * to the context's inline capacity so a long malformed suffix cannot be traversed twice.
 * An unfinished payload may have been consumed through EOF by the state machine. Delegate
 * an unconsumed opening parenthesis to preserve that cursor and the public error semantics.
 */
template <manipulators::scalar_flags flags, ::std::integral Char, typename T>
[[nodiscard]] inline constexpr Char const *scan_decfloat_terminal_special(
	Char const *first, Char const *end, bool negative, T &target) noexcept
{
	if (first == end || !scan_decfloat_special_start_char(*first))
	{
		return nullptr;
	}
	constexpr auto capacity{scan_floating_context<Char>::inline_capacity};
	auto const last{static_cast<::std::size_t>(end - first) > capacity ? first + capacity : end};
	T value{};
	auto const result{scan_hexfloat_special_value<flags.nan_parse_sign, flags.nan_payload_scan>(first, last, negative, value)};
	if (!result.matched || result.code != parse_code::ok || (result.iter == last && last != end))
	{
		return nullptr;
	}
	if constexpr (flags.nan_payload_scan != manipulators::floating_nan_payload_scan::none)
	{
		if (result.iter != last && *result.iter == char_literal_v<u8'(', Char>)
		{
			return nullptr;
		}
	}
	scan_floating_commit_special(target, value);
	return result.iter;
}

/**
 * @brief Restarts a proven over-capacity terminal coefficient directly in the exact contiguous scanner.
 * @details The short lexer has already observed a digit beyond its capacity. The original
 * sign and all digits are available in this borrowed span. Skipping a second short probe
 * leaves the exact rounding decision to the existing long accumulator. On a rolled-back
 * exponent or range error the streaming context still owns the error cursor and target.
 */
template <manipulators::scalar_flags flags, ::std::integral Char, typename T>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
[[nodiscard]] inline constexpr Char const *scan_decfloat_terminal_long(
	Char const *number_begin, Char const *end, T &target) noexcept
{
	T value{};
	auto const result{scan_decfloat_contiguous_define_impl<Char, flags, T, false, false>(number_begin, end, value)};
	if (result.code != parse_code::ok)
	{
		return nullptr;
	}
	if constexpr (flags.floating != manipulators::floating_format::fixed)
	{
		if (result.iter != end && scan_hexfloat_caseless_equal<u8'e', u8'E'>(*result.iter))
		{
			return nullptr;
		}
	}
	target = value;
	return result.iter;
}

/**
 * @brief Attempts one ordinary decimal field in a borrowed or refillable span.
 * @details The bounded short scanner parses the configured sign, radix and exponent
 * into a private value. An unconsumed code unit proves the end of a completed field.
 * For fixed notation an exponent marker is itself a delimiter. For optional
 * exponents a rolled-back marker must go to the context scanner, which owns the
 * error cursor. At the end of the span only a terminal provider proves EOF.
 * A terminal span can also resolve a complete special word or retain all digits of
 * a long coefficient. Every miss leaves the original cursor and target for the state machine.
 */
template <bool terminal_span, ::std::integral Char, manipulators::scalar_flags flags, typename T>
FAST_IO_GNU_ALWAYS_INLINE [[nodiscard]] inline constexpr Char const *scan_decfloat_resident_prefix(
	Char const *begin, Char const *end, T &target) noexcept
{
	auto *first{begin};
	if constexpr (!flags.noskipws)
	{
		if (first != end && static_cast<unsigned char>(*first) <= u8' ')
		{
			first = details::find_space_common_impl<false, true>(first, end);
		}
	}
	[[maybe_unused]] auto const number_begin{first};
	bool const negative{first != end && *first == char_literal_v<u8'-', Char>};
	if (negative || (flags.allow_leading_plus && first != end && *first == char_literal_v<u8'+', Char>))
	{
		++first;
	}
#if defined(__i386__) && defined(__ELF__)
	if constexpr (details::fp_floating_point_is_float80<T> &&
				  flags.floating == manipulators::floating_format::decimal &&
				  flags.base == 10u &&
				  flags.rounding != manipulators::floating_rounding::current_environment)
	{
		if (!::std::is_constant_evaluated())
		{
			if (auto const *terminal{details::scan_x87_short_chunk<Char, flags>(first, end, negative, target)})
			{
				return terminal;
			}
			return nullptr;
		}
	}
#endif
	constexpr auto completion{terminal_span ? scan_decfloat_short_completion::terminal : scan_decfloat_short_completion::chunk};
	T value{};
	auto const result{scan_decfloat_contiguous_short_define_impl<Char, flags, T, completion>(first, end, negative, value)};
	if (!result.handled)
	{
		if constexpr (terminal_span)
		{
			if (result.iter != nullptr)
			{
				return scan_decfloat_terminal_long<flags>(number_begin, end, target);
			}
			return scan_decfloat_terminal_special<flags>(first, end, negative, target);
		}
		return nullptr;
	}
	if (result.code != parse_code::ok)
	{
		return nullptr;
	}
	target = value;
	return result.iter;
}

}

/** @brief Completes a refillable decimal field only when its delimiter is resident. */
template <::std::integral Char, ::fast_io::manipulators::scalar_flags flags, details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat &&
			 sizeof(Char) == sizeof(char8_t) && details::is_ascii<Char>)
inline constexpr parse_result<Char const *> scan_context_current_chunk_define(
	io_reserve_type_t<Char, manipulators::scalar_manip_t<flags, T &>>, Char const *begin, Char const *end,
	manipulators::scalar_manip_t<flags, T &> target) noexcept
{
	if (auto const next{details::scan_decfloat_resident_prefix<false, Char, flags>(begin, end, target.reference)})
	{
		return {next, parse_code::ok};
	}
	return {begin, parse_code::partial};
}

/** @brief Certifies the provenance of every committed ordinary decimal chunk cursor.
 * @details The scanner advances from begin with bounded loads and commits only before end.
 * A declined probe returns begin. The i386 x87 leaf uses the same cursor rule. */
template <::std::integral Char, manipulators::scalar_flags flags, details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != manipulators::floating_format::hexfloat && sizeof(Char) == 1 && details::is_ascii<Char>)
inline constexpr ::std::true_type scan_context_current_chunk_result_in_range(
	io_reserve_type_t<Char, manipulators::scalar_manip_t<flags, T &>>) noexcept
{
	return {};
}

/** @brief Accepts true EOF as the delimiter of a complete short binary32 or binary64 decimal. */
template <::std::integral Char, manipulators::scalar_flags flags, details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != manipulators::floating_format::hexfloat && sizeof(Char) == 1 && details::is_ascii<Char> &&
			 (details::iec559_traits<T>::mbits == 23u || details::iec559_traits<T>::mbits == 52u))
inline constexpr Char const *scan_context_try_terminal_define(
	io_reserve_type_t<Char, manipulators::scalar_manip_t<flags, T &>>, Char const *begin, Char const *end,
	manipulators::scalar_manip_t<flags, T &> target) noexcept
{
	return details::scan_decfloat_resident_prefix<true, Char, flags>(begin, end, target.reference);
}

/// @brief Scans a decimal floating value with a provider-proved readable tail.
/// @details The dispatcher supplies the true semantic `[begin,end)` and P readable elements after `end`.  Decimal
///          SIMD code may use P only to complete a 16- or 32-byte unmasked load after the exact significand limit; it
///          caps the valid-lane count at `end` and reduces sticky bits over semantic lanes only.  All other grammar
///          branches remain bounded by `end`.  Therefore the returned iterator never enters padding and changing
///          padding values cannot affect the parsed value, status, or rounding.
template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::fast_io::parse_result<char_type const *>
scan_contiguous_padding_define(
	io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>> tag,
	char_type const *begin, char_type const *end, ::std::size_t padding,
	::fast_io::manipulators::scalar_manip_t<flags, T &> value) noexcept
{
	(void)tag;
	if constexpr (
		!::fast_io::details::
			scan_decfloat_terminal_padding_leaf_available<char_type, T>)
	{
		(void)padding;
		return scan_contiguous_define(tag, begin, end, value);
	}
	else
	{
		return ::fast_io::details::
			scan_decfloat_contiguous_padding_define<char_type, flags>(
				begin, end, value.reference, 0u, padding);
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::fast_io::parse_result<char_type const *>
scan_contiguous_define(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_precision_t<flags, T &>>,
					   char_type const *begin, char_type const *end,
					   ::fast_io::manipulators::scalar_manip_precision_t<flags, T &> value) noexcept
{
	return ::fast_io::details::scan_decfloat_contiguous_define<char_type, flags, T, true>(
		begin, end, value.reference, value.precision);
}

/// @brief Scans a precision-controlled decimal floating value with a provider-proved readable tail.
/// @details This is the precision-manipulator form of the padded decimal protocol.  Precision changes only the final
///          rounding assignment; the same SIMD proof bounds physical reads by `[begin,end+padding)`, semantic progress
///          by `[begin,end]`, and sticky-bit accumulation by characters before `end`.
template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::fast_io::parse_result<char_type const *>
scan_contiguous_padding_define(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_precision_t<flags, T &>> tag,
	char_type const *begin, char_type const *end, ::std::size_t padding,
	::fast_io::manipulators::scalar_manip_precision_t<flags, T &> value) noexcept
{
	(void)tag;
	if constexpr (
		!::fast_io::details::
			scan_decfloat_terminal_padding_leaf_available<char_type, T>)
	{
		(void)padding;
		return scan_contiguous_define(tag, begin, end, value);
	}
	else
	{
		return ::fast_io::details::
			scan_decfloat_contiguous_padding_define<
				char_type, flags, T, true>(
				begin, end, value.reference, value.precision, padding);
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr auto
scan_context_type(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>) noexcept
{
	return io_type_t<::fast_io::details::scan_decfloat_context<char_type, T>>{};
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::fast_io::parse_result<char_type const *>
scan_context_define(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>,
					::fast_io::details::scan_decfloat_context<char_type, T> &state, char_type const *begin,
					char_type const *end,
					::fast_io::manipulators::scalar_manip_t<flags, T &> value) noexcept
{
	return ::fast_io::details::scan_decfloat_context_numeric_define<char_type, flags, T, false>(
		state, begin, end, value.reference, 0);
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::fast_io::parse_code
scan_context_eof_define(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>,
						::fast_io::details::scan_decfloat_context<char_type, T> &state,
						::fast_io::manipulators::scalar_manip_t<flags, T &> value) noexcept
{
	return ::fast_io::details::scan_decfloat_context_eof<char_type, flags, T, false>(state, value.reference);
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::std::size_t
scan_context_eof_rewind_size(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>,
							 ::fast_io::details::scan_decfloat_context<char_type, T> &state,
							 ::fast_io::manipulators::scalar_manip_t<flags, T &>) noexcept
{
	if (state.phase == ::fast_io::details::scan_decfloat_context_phase::special)
	{
		return state.special_buffer.size;
	}
	return 0u;
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr auto
scan_context_type(io_reserve_type_t<char_type,
									::fast_io::manipulators::scalar_manip_precision_t<flags, T &>>) noexcept
{
	return io_type_t<::fast_io::details::scan_decfloat_context<char_type, T>>{};
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::fast_io::parse_result<char_type const *>
scan_context_define(io_reserve_type_t<char_type,
									  ::fast_io::manipulators::scalar_manip_precision_t<flags, T &>>,
					::fast_io::details::scan_decfloat_context<char_type, T> &state, char_type const *begin,
					char_type const *end,
					::fast_io::manipulators::scalar_manip_precision_t<flags, T &> value) noexcept
{
	return ::fast_io::details::scan_decfloat_context_numeric_define<char_type, flags, T, true>(
		state, begin, end, value.reference, value.precision);
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::fast_io::parse_code
scan_context_eof_define(io_reserve_type_t<char_type,
										  ::fast_io::manipulators::scalar_manip_precision_t<flags, T &>>,
						::fast_io::details::scan_decfloat_context<char_type, T> &state,
						::fast_io::manipulators::scalar_manip_precision_t<flags, T &> value) noexcept
{
	return ::fast_io::details::scan_decfloat_context_eof<char_type, flags, T, true>(
		state, value.reference, value.precision);
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::std::size_t
scan_context_eof_rewind_size(io_reserve_type_t<char_type,
											   ::fast_io::manipulators::scalar_manip_precision_t<flags, T &>>,
							 ::fast_io::details::scan_decfloat_context<char_type, T> &state,
							 ::fast_io::manipulators::scalar_manip_precision_t<flags, T &>) noexcept
{
	if (state.phase == ::fast_io::details::scan_decfloat_context_phase::special)
	{
		return state.special_buffer.size;
	}
	return 0u;
}

/*
Range theorem for the ordinary decimal context transition. Let [B,E] be the
closed cursor range supplied to scan_context_define and let S be any
protocol-valid state. In the normal numeric path, whitespace and digit scans
return a cursor in [B,E], every direct increment is guarded by cursor != E,
and recursive phase transitions receive that cursor unchanged. Consequently,
normal success, invalid, overflow, and partial results return either the
current cursor or E.

The special-prefix helper returns B, but its partial result is internal and
processing continues; only allocation failure publishes B, with overflow.
scan_floating_context_append likewise returns only B or E. The decimal
contiguous sub-parser's lexical range theorem places its iterator in the
special buffer's closed range with that buffer's provenance. Consequently,
scan_floating_context_map_iter may subtract it from the buffer base and is the
sole conversion back to the caller's chunk: it clamps an iterator in the old
buffered prefix to B, an offset beyond this chunk to E, and otherwise returns B
plus an offset no larger than E-B. Thus Inf/NaN success, invalid payloads,
truncation, and allocation failure also preserve the theorem.

A partial normal transition reaches E; a partial special transition returns E
(including the empty span, where B == E). Therefore this certificate neither
hides a nonempty no-progress transition nor weakens the dispatcher's separate
partial/no-progress checks. EOF does not return an iterator. Dynamic precision
changes only final numeric assignment and shares the same lexical state
machine, so the proof applies identically to the precision CPO. Formally, for
both overloads below and every parse code, the returned iterator R has B's
array provenance and satisfies 0 <= R-B <= E-B.

This ADL proof is intentionally restricted to these decimal scalar protocols.
It establishes nothing for hexadecimal, custom, transcoding, or string
scanners, even when those scanners happen to use a related representation.
*/
template <::std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating !=
			 ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::std::true_type scan_context_result_in_range(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_t<flags, T &>>) noexcept
{
	return {};
}

template <::std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating !=
			 ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::std::true_type scan_context_result_in_range(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::
			scalar_manip_precision_t<flags, T &>>) noexcept
{
	return {};
}

template <::std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating !=
				 ::fast_io::manipulators::floating_format::hexfloat &&
			 ::fast_io::details::
				 scan_decfloat_terminal_padding_dispatch_available<
					 char_type, T>)
inline constexpr ::std::true_type
scan_context_terminal_padding_equivalent(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_t<flags, T &>>) noexcept
{
	return {};
}

template <::std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating !=
				 ::fast_io::manipulators::floating_format::hexfloat &&
			 ::fast_io::details::
				 scan_decfloat_terminal_padding_dispatch_available<
					 char_type, T>)
inline constexpr ::std::true_type
scan_context_terminal_padding_equivalent(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::
			scalar_manip_precision_t<flags, T &>>) noexcept
{
	return {};
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags, details::my_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat &&
			 !details::scan_decfloat_supported_floating_point<T>)
inline constexpr ::fast_io::parse_result<char_type const *>
scan_contiguous_define(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>,
					   char_type const *begin, char_type const *,
					   ::fast_io::manipulators::scalar_manip_t<flags, T &>) noexcept
{
	static_assert(details::scan_decfloat_supported_floating_point<T>,
				  "fast_io decimal floating scan does not support this floating-point type");
	return {begin, ::fast_io::parse_code::invalid};
}

} // namespace fast_io
