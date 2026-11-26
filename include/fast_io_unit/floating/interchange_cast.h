#pragma once

#include "binary_interchange_convert.h"

namespace fast_io::details
{
template <fp8_format F>
inline constexpr unsigned fp8_fraction_bits{F == fp8_format::e4m3fn ? 3u : 2u};
template <fp8_format F>
inline constexpr unsigned fp8_last_finite{F == fp8_format::e4m3fn ? 126u : 123u};

/// @brief Returns the positive dyadic value at a magnitude code, including the virtual overflow neighbor.
/// @details All significands and powers here are exact binary32 values. The virtual successor of
/// 448 is 480 in E4M3, and that of 57344 is 65536 in E5M2. These delimit the last rounding cell.
template <fp8_format F>
inline constexpr float fp8_magnitude(unsigned code) noexcept
{
	constexpr unsigned m{fp8_fraction_bits<F>};
	constexpr unsigned bias{(1u << (6u - m)) - 1u};
	auto exponent{code >> m};
	auto fraction{code & ((1u << m) - 1u)};
	if (!exponent)
	{
		return static_cast<float>(fraction) *
			   ::std::bit_cast<float>((128u - bias - m) << 23u);
	}
	return ::std::bit_cast<float>(((127u - bias + exponent) << 23u) | (fraction << (23u - m)));
}

/// @brief Expands the interchange value into native binary32 object bits.
template <fp8_format F>
inline constexpr ::std::uint_least32_t fp8_expand_bits(basic_float8<F> value) noexcept
{
	auto const code{static_cast<unsigned>(value.bits) & 127u};
	auto const sign{static_cast<::std::uint_least32_t>(value.bits & 128u) << 24u};
	::std::uint_least32_t magnitude{};
	if (code <= fp8_last_finite<F>)
	{
		magnitude = ::std::bit_cast<::std::uint_least32_t>(fp8_magnitude<F>(code));
	}
	else if constexpr (F == fp8_format::e4m3fn)
	{
		magnitude = 0x7fc00000u;
	}
	else
	{
		magnitude = 0x7f800000u | ((code & 3u) << 21u);
	}
	return fp_translate_nan_encoding<23u, 8u>(sign | magnitude);
}

/// @brief Expands the interchange bits into the native floating value.
template <fp8_format F>
inline constexpr float fp8_expand(basic_float8<F> value) noexcept
{
	return ::std::bit_cast<float>(fp8_expand_bits(value));
}


} // namespace fast_io::details

namespace fast_io::details
{

template <typename T>
concept ofp8_value = ::std::same_as<T, float8_e4m3fn> || ::std::same_as<T, float8_e5m2>;

template <ofp8_value T>
inline constexpr T cast_fp8_code(unsigned code, bool negative) noexcept
{
	return T::from_bits(static_cast<::std::uint_least8_t>((negative ? 128u : 0u) | code));
}

template <ofp8_value T>
inline constexpr unsigned cast_fp8_overflow{T::format == fp8_format::e4m3fn ? 127u : 124u};

/// Compare a positive exact value to the 127 or 124 nearest-even boundaries.
/// An odd lower encoding loses an exact tie, including E5M2's overflow
/// boundary. E4M3FN's largest encoding is even and still wins its last tie.
template <ofp8_value T, typename Compare>
inline constexpr T cast_fp8_interval(bool negative, Compare compare) noexcept
{
	unsigned low{}, high{fp8_last_finite<T::format> + 1u};
	while (low < high)
	{
		auto const index{(low + high) / 2u};
		auto const middle{(fp8_magnitude<T::format>(index) + fp8_magnitude<T::format>(index + 1u)) * 0.5f};
		auto const order{compare(middle)};
		if (0 < order || (order == 0 && (index & 1u)))
		{
			low = index + 1u;
		}
		else
		{
			high = index;
		}
	}
	return cast_fp8_code<T>(fp8_last_finite<T::format> < low ? cast_fp8_overflow<T> : low, negative);
}

template <typename Digits>
inline constexpr int cast_decimal_compare(Digits const &source, unsigned char const *digits, unsigned size, int exponent) noexcept
{
	auto const lhs{source.exponent + static_cast<int>(source.size)};
	auto const rhs{exponent + static_cast<int>(size)};
	if (lhs != rhs)
	{
		return lhs < rhs ? -1 : 1;
	}
	auto const n{source.size < size ? source.size : size};
	for (::std::size_t i{}; i != n; ++i)
	{
		if (source.digits[i] != digits[i])
		{
			return source.digits[i] < digits[i] ? -1 : 1;
		}
	}
	for (auto i{n}; i < source.size; ++i)
	{
		if (source.digits[i])
		{
			return 1;
		}
	}
	for (auto i{n}; i < size; ++i)
	{
		if (digits[i])
		{
			return -1;
		}
	}
	return 0;
}

template <my_unsigned_integral U>
struct cast_integer_decimal
{
	unsigned char digits[::std::numeric_limits<U>::digits10 + 1u]{};
	::std::size_t size{};
	int exponent{};
};

template <my_unsigned_integral U>
inline constexpr auto cast_integer_digits(U magnitude) noexcept
{
	cast_integer_decimal<U> result{};
	do
	{
		result.digits[result.size++] = static_cast<unsigned char>(magnitude % 10u);
		magnitude = static_cast<U>(magnitude / 10u);
	} while (magnitude);
	for (::std::size_t i{}; i < result.size / 2u; ++i)
	{
		auto const other{result.size - 1u - i};
		auto const digit{result.digits[i]};
		result.digits[i] = result.digits[other];
		result.digits[other] = digit;
	}
	return result;
}

template <ofp8_value T, typename Digits>
inline constexpr T cast_decimal_to_fp8(Digits const &source, bool negative) noexcept
{
	if (!source.digits[0])
	{
		return cast_fp8_code<T>(0u, negative);
	}
	// Every OFP8 midpoint times 2^17 is an integer. Removing powers of two
	// first leaves at most 31 times 5^17, so uint64_t holds its exact
	// terminating decimal coefficient on every target.
	return cast_fp8_interval<T>(negative, [&](float halfway) constexpr {
		auto coefficient{static_cast<::std::uint_least64_t>(halfway * 131072.0f)};
		int exponent{-17};
		while (exponent && !(coefficient & 1u))
		{
			coefficient >>= 1u;
			++exponent;
		}
		for (int i{exponent}; i < 0; ++i)
		{
			coefficient *= 5u;
		}
		auto const mid{cast_integer_digits(coefficient)};
		return cast_decimal_compare(source, mid.digits, static_cast<unsigned>(mid.size), exponent);
	});
}

template <my_integral I>
inline constexpr auto cast_integer_magnitude(I value) noexcept
{
	using U = my_make_unsigned_t<::std::conditional_t<::std::same_as<I, bool>, unsigned, I>>;
	U magnitude{static_cast<U>(value)};
	if constexpr (my_signed_integral<I>)
	{
		if (value < 0)
		{
			magnitude = static_cast<U>(U{} - magnitude);
		}
	}
	return magnitude;
}

template <typename To, typename Digits>
inline constexpr To cast_finite_decimal(Digits const &source, bool negative, bool integer_quantum) noexcept
{
	using layout = decimal_interchange_binding<To>;
	decimal_components<layout::bits> out{};
	out.negative = negative;
	if (!source.digits[0])
	{
		out.exponent = source.exponent < layout::minimum_quantum ? layout::minimum_quantum : layout::maximum_quantum < source.exponent ? layout::maximum_quantum
																																	   : source.exponent;
		return decimal_encode<To>(out);
	}
	// Guard and sticky come from the exact input. The subnormal and
	// precision grids are both powers of ten, so their coarsest grid gives
	// the final correctly rounded coefficient in one decision.
	constexpr unsigned capacity{layout::precision + 1u};
	out.size = source.size < capacity ? source.size : capacity;
	out.exponent = source.exponent + static_cast<int>(source.size - out.size);
	bool sticky{};
	for (::std::size_t i{}; i != out.size; ++i)
	{
		out.digits[i] = source.digits[i];
	}
	for (auto i{out.size}; i != source.size; ++i)
	{
		sticky |= source.digits[i] != 0;
	}
	auto const subnormal_keep{static_cast<int>(out.size) + out.exponent - layout::minimum_quantum};
	auto const keep{subnormal_keep < static_cast<int>(layout::precision) ? subnormal_keep : static_cast<int>(layout::precision)};
	// A carry on the minimum-quantum grid keeps that quantum when its
	// extra digit fits, including the carry into the first normal value.
	decimal_round(out, keep, sticky, decimal_rounding::nearest_to_even,
				  subnormal_keep < static_cast<int>(layout::precision));
	int preferred{integer_quantum ? 0 : layout::maximum_quantum};
	while (preferred < out.exponent && out.size < layout::precision)
	{
		out.digits[out.size++] = 0u;
		--out.exponent;
	}
	if (layout::maximum_quantum < out.exponent)
	{
		out = {};
		out.negative = negative;
		out.category = decimal_class::infinity;
	}
	return decimal_encode<To>(out);
}

template <decimal_interchange_value To, unsigned width>
inline constexpr To cast_decimal_special(decimal_components<width> const &source) noexcept
{
	using layout = decimal_interchange_binding<To>;
	decimal_components<layout::bits> out{};
	out.category = source.category == decimal_class::infinity ? decimal_class::infinity : decimal_class::quiet_nan;
	out.negative = source.negative;
	if (source.category != decimal_class::infinity && source.digits[0])
	{
		// BID and DPD place diagnostics in the trailing p-1 decimal digits.
		// GCC libbid aligns the most significant position when changing widths.
		constexpr int delta{static_cast<int>(layout::precision) - static_cast<int>(decimal_layout<width>::precision)};
		int n{static_cast<int>(source.size) + delta};
		if (0 < n)
		{
			out.size = static_cast<::std::size_t>(n);
			for (::std::size_t i{}; i != out.size; ++i)
			{
				out.digits[i] = i < source.size ? source.digits[i] : 0u;
			}
		}
	}
	return decimal_encode<To>(out);
}

/// Binary diagnostic bits align at the top across binary formats, as in LLVM
/// APFloat. Across radices the diagnostic is an unsigned integer; a destination
/// of smaller capacity retains its low representable digits or bits.
template <decimal_interchange_value To, my_unsigned_integral U>
inline constexpr To cast_binary_special_to_decimal(U diagnostic, bool infinity, bool negative) noexcept
{
	using layout = decimal_interchange_binding<To>;
	decimal_components<layout::bits> out{};
	out.negative = negative;
	out.category = infinity ? decimal_class::infinity : decimal_class::quiet_nan;
	if (diagnostic)
	{
		auto const decimal{cast_integer_digits(diagnostic)};
		auto const count{decimal.size < layout::precision ? decimal.size : layout::precision - 1u};
		auto first{decimal.size - count};
		while (first + 1u < decimal.size && !decimal.digits[first])
		{
			++first;
		}
		out.size = decimal.size - first;
		for (::std::size_t i{}; i != out.size; ++i)
		{
			out.digits[i] = decimal.digits[first + i];
		}
	}
	return decimal_encode<To>(out);
}

template <typename To, my_unsigned_integral U>
inline constexpr To cast_binary_special(U diagnostic, bool infinity, bool negative) noexcept
{
	using trait = iec559_traits<To>;
	using word = typename trait::mantissa_type;
	constexpr auto m{trait::mbits};
	constexpr auto exponent{(1u << trait::ebits) - 1u};
	constexpr word quiet{static_cast<word>(word{1} << (m - 1u))};
	word frac{infinity ? word{} : static_cast<word>(quiet | (static_cast<word>(diagnostic) & (quiet - 1u)))};
	To out{};
	if constexpr (fp_floating_point_is_float80<To>)
	{
		fp_assign_float80_bits(out, (UINT64_C(1) << 63u) | static_cast<::std::uint_least64_t>(frac), exponent, negative);
	}
#if defined(__SIZEOF_INT128__)
	else if constexpr (fp_floating_point_is_ibm_double_double<To>)
	{
		fp_assign_ibm_double_double_components(out, cast_binary_special<double>(static_cast<::std::uint_least64_t>(frac >> 53u), infinity, negative), 0.0);
	}
#endif
	else
	{
		auto const bits{static_cast<word>((static_cast<word>(negative) << (m + trait::ebits)) |
										  (static_cast<word>(exponent) << m) | frac)};
		fp_assign_bits(out, bits);
	}
	return out;
}

#if !defined(__SIZEOF_INT128__)
// An x87 result carries 64 significand bits even on 32-bit x86. The general
// scanner's no-int128 quotient cannot store a sixty-fifth carry bit. Compute
// the same exact division and handle that single carry explicitly.
template <typename To, unsigned width>
	requires fp_floating_point_is_float80<To>
inline constexpr To cast_decimal_to_portable_x87(decimal_components<width> const &value) noexcept
{
	// A 34-digit coefficient needs 113 bits. The largest power of five in
	// decimal128 is 5^6176 (14341 bits); aligned subtraction adds <= 65 bits.
	constexpr unsigned capacity{232u};
	scan_decfloat_bigint<capacity> numerator{}, denominator{};
	for (::std::size_t i{}; i != value.size; ++i)
	{
		(void)scan_decfloat_bigint_mul_small(numerator, 10u);
		(void)scan_decfloat_bigint_add_small(numerator, value.digits[i]);
	}
	scan_decfloat_bigint_set_u64(denominator, 1u);
	if (value.exponent < 0)
	{
		(void)scan_decfloat_bigint_mul_pow5(denominator, static_cast<unsigned>(-value.exponent));
	}
	else
	{
		(void)scan_decfloat_bigint_mul_pow5(numerator, static_cast<unsigned>(value.exponent));
	}
	auto top{scan_decfloat_bigint_floor_log2_ratio(numerator, denominator) + value.exponent};
	To out{};
	if (16383 < top)
	{
		fp_assign_infinity(out, value.negative);
		return out;
	}
	if (top < -16446)
	{
		fp_assign_float80_bits(out, 0u, 0u, value.negative);
		return out;
	}
	auto const quantum{(top < -16382 ? -16382 : top) - 63};
	auto const division{scan_decfloat_bigint_div_shifted_to_quotient(numerator, denominator, value.exponent - quantum)};
	auto significand{division.quotient};
	if (division.twice_remainder_compare > 0 || (division.twice_remainder_compare == 0 && (significand & 1u)))
	{
		if (++significand == 0u)
		{
			significand = UINT64_C(1) << 63u;
			++top;
		}
	}
	if (16383 < top)
	{
		fp_assign_infinity(out, value.negative);
	}
	else
	{
		auto const exponent{top < -16382 ? static_cast<unsigned>(significand >> 63u) : static_cast<unsigned>(top + 16383)};
		fp_assign_float80_bits(out, significand, exponent, value.negative);
	}
	return out;
}
#endif

/// A nearest-even numeric conversion returns the signed IEEE endpoint for
/// a range result. The lexical scanner separately reports range to its caller.
template <typename To, typename Digits>
inline constexpr To cast_exact_decimal_to_native_binary(Digits const &value, bool negative) noexcept
{
	scan_decfloat_significand_state<To> state{};
	for (::std::size_t i{}; i != value.size; ++i)
	{
		scan_decfloat_append_digit(state, static_cast<char8_t>(value.digits[i]), false);
	}
	To out{};
	if (scan_decfloat_assign<To, decimal_rounding::nearest_to_even>(out, negative, state, value.exponent) == parse_code::overflow)
	{
		if (0 < static_cast<::std::int_least64_t>(value.size) + value.exponent)
		{
			fp_assign_infinity(out, negative);
		}
		else
		{
			out = negative ? -static_cast<To>(0) : static_cast<To>(0);
		}
	}
	return out;
}

template <typename To, unsigned width>
inline constexpr To cast_decimal_to_binary(decimal_components<width> const &value) noexcept
{
	if (value.category != decimal_class::finite)
	{
		// IBM long double has one binary64 diagnostic. Its low component
		// does not extend the payload as it extends a finite significand.
		using U = ::std::conditional_t<fp_floating_point_is_ibm_double_double<To>,
			::std::uint_least64_t, typename iec559_traits<To>::mantissa_type>;
		U payload{};
		for (::std::size_t i{}; i != value.size; ++i)
		{
			payload = static_cast<U>(static_cast<U>(payload * U{10}) + value.digits[i]);
		}
		auto const infinity{value.category == decimal_class::infinity};
#if defined(__SIZEOF_INT128__)
		if constexpr (fp_floating_point_is_ibm_double_double<To>)
		{
			To out{};
			fp_assign_ibm_double_double_components(out,
				cast_binary_special<double>(payload, infinity, value.negative), 0.0);
			return out;
		}
		else
#endif
		{
			return cast_binary_special<To>(payload, infinity, value.negative);
		}
	}
	if (!value.digits[0])
	{
		return value.negative ? -static_cast<To>(0) : static_cast<To>(0);
	}
#if !defined(__SIZEOF_INT128__)
	if constexpr (fp_floating_point_is_float80<To>)
	{
		return cast_decimal_to_portable_x87<To>(value);
	}
#endif
	return cast_exact_decimal_to_native_binary<To>(value, value.negative);
}

template <my_integral To, unsigned width>
inline constexpr To cast_decimal_to_integer(decimal_components<width> const &value) noexcept
{
	if constexpr (::std::same_as<To, bool>)
	{
		return value.category != decimal_class::finite || value.digits[0] != 0u;
	}
	else
	{
		if (value.category != decimal_class::finite)
		{
			::fast_io::fast_terminate();
		}
		auto const places{value.exponent + static_cast<int>(value.size)};
		if (places <= 0 || !value.digits[0])
		{
			return 0;
		}
		using U = my_make_unsigned_t<To>;
		U limit{static_cast<U>((::std::numeric_limits<To>::max)())};
		if (value.negative)
		{
			if constexpr (my_signed_integral<To>)
			{
				++limit;
			}
			else
			{
				::fast_io::fast_terminate();
			}
		}
		U magnitude{};
		for (int i{}; i != places; ++i)
		{
			U digit{static_cast<U>(static_cast<::std::size_t>(i) < value.size ? value.digits[i] : 0u)};
			if (limit / 10u < magnitude || (magnitude == limit / 10u && limit % 10u < digit))
			{
				::fast_io::fast_terminate();
			}
			magnitude = static_cast<U>(magnitude * 10u + digit);
		}
		return static_cast<To>(value.negative ? static_cast<U>(U{} - magnitude) : magnitude);
	}
}

/// Compiler object words follow byte order, while the public portable pair is
/// always {low, high}. The same swap is its own inverse.
inline constexpr binary128_words binary_native_word_order(binary128_words words) noexcept
{
	if constexpr (::std::endian::native == ::std::endian::big)
	{
		return {words.high, words.low};
	}
	return words;
}

template <typename From>
inline constexpr interchange_binary_parts cast_source_binary_parts(From const &source) noexcept
{
	if constexpr (is_software_binary<From>)
	{
		return binary_interchange_parts(source);
	}
	else if constexpr (ofp8_value<From>)
	{
		constexpr auto m{fp8_fraction_bits<From::format>};
		return binary_parts_from_words({source.bits, 0u}, m, 7u - m, From::format == fp8_format::e4m3fn);
	}
	else if constexpr (my_integral<From>)
	{
		bool negative{};
		if constexpr (my_signed_integral<From>)
		{
			negative = source < 0;
		}
		return {decimal_split_words(cast_integer_magnitude(source)), 0, negative, decimal_class::finite, 0u};
	}
	else if constexpr (native_binary128_interchange<From>)
	{
		auto const words{binary_native_word_order(::fast_io::bit_cast<binary128_words>(source))};
		return binary_parts_from_words(words, 112u, 15u);
	}
	else
	{
		using L = iec559_traits<From>;
		auto const fields{[&]() constexpr {
			if constexpr (fp_floating_point_is_ibm_double_double<From>)
			{
				return get_punned_result(source);
			}
			else
			{
				return compiler_constant_floating_capture_fields<From>(source);
			}
		}()};
		auto words{decimal_split_words(fields.mantissa)};
		decimal_set_field(words, L::mbits, L::ebits, fields.exponent);
		if (fields.sign)
		{
			decimal_set_field(words, L::mbits + L::ebits, 1u, 1u);
		}
		if constexpr (fp_native_nan_high_bit_signals<L::mbits>)
		{
			words.low = fp_translate_nan_encoding<L::mbits, L::ebits>(words.low);
		}
		return binary_parts_from_words(words, L::mbits, L::ebits);
	}
}

template <my_integral To>
inline constexpr To cast_binary_parts_to_integer(interchange_binary_parts parts) noexcept
{
	if constexpr (::std::same_as<To, bool>)
	{
		return parts.category != decimal_class::finite || binary_words_nonzero(parts.coefficient);
	}
	else
	{
		if (parts.category != decimal_class::finite)
		{
			::fast_io::fast_terminate();
		}
		auto const width{binary_words_width(parts.coefficient)};
		if (!width || width + parts.exponent <= 0)
		{
			return 0;
		}
		using U = my_make_unsigned_t<To>;
		if ((parts.negative && !my_signed_integral<To>) || ::std::numeric_limits<U>::digits < width + parts.exponent)
		{
			::fast_io::fast_terminate();
		}
		U const magnitude{decimal_join_words<U>(binary_words_shift(parts.coefficient, parts.exponent))};
		if constexpr (my_signed_integral<To>)
		{
			auto limit{static_cast<U>((::std::numeric_limits<To>::max)())};
			if (parts.negative)
			{
				++limit;
			}
			if (limit < magnitude)
			{
				::fast_io::fast_terminate();
			}
		}
		return static_cast<To>(parts.negative ? static_cast<U>(U{} - magnitude) : magnitude);
	}
}

template <decimal_interchange_value To>
inline constexpr To cast_binary_words_special_to_decimal(interchange_binary_parts parts) noexcept
{
	using L = decimal_interchange_binding<To>;
	decimal_components<L::bits> result{};
	result.category = parts.category == decimal_class::infinity ? decimal_class::infinity : decimal_class::quiet_nan;
	result.negative = parts.negative;
	if (binary_words_nonzero(parts.coefficient))
	{
		// Up to 111 diagnostic bits occupy at most four base-10^9 blocks.
		// Cross-radix payloads are integers, so a narrow target keeps low digits.
		unsigned char digits[36]{};
		unsigned first{36u};
		do
		{
			first -= 9u;
			exact_decimal_write_nine_digits(digits + first, decimal_divide_billion(parts.coefficient));
		} while (binary_words_nonzero(parts.coefficient));
		constexpr unsigned cutoff{36u - (L::precision - 1u)};
		if (first < cutoff)
		{
			first = cutoff;
		}
		while (first < 35u && !digits[first])
		{
			++first;
		}
		result.size = 36u - first;
		::fast_io::details::non_overlapped_copy_n(digits + first, result.size, result.digits);
	}
	return decimal_encode<To>(result);
}

template <typename To, unsigned W>
	requires is_software_binary<To>
inline constexpr To cast_decimal_to_software_binary(decimal_components<W> const &source) noexcept
{
	if (source.category == decimal_class::finite)
	{
		return binary_interchange_from_decimal<To>(source, source.negative);
	}
	using L = binary_interchange_layout<To::format>;
	binary128_words payload{};
	for (::std::size_t i{}; i != source.size; ++i)
	{
		decimal_multiply_add(payload, 10u, source.digits[i]);
	}
	return binary_interchange_from_words<To>(binary_words_special(L::mbits, L::ebits, source.negative, source.category == decimal_class::infinity, payload));
}

template <typename To, typename From>
inline constexpr To cast_with_software_binary(From const &source) noexcept
{
	if constexpr (::std::same_as<To, software_float16> &&
				  (::std::same_as<From, software_float32> || ::std::same_as<From, software_float128>))
	{
		return binary_narrow_to_half(source);
	}
	else if constexpr (is_software_binary<To> && decimal_interchange_value<From>)
	{
		return cast_decimal_to_software_binary<To>(decimal_decode(source));
	}
	else
	{
		if constexpr (is_software_binary<From> && is_software_binary<To>)
		{
			using S = binary_interchange_layout<From::format>;
			using D = binary_interchange_layout<To::format>;
			if constexpr (S::mbits <= D::mbits && S::ebits <= D::ebits)
			{
				// A normal source has value (-1)^sign (1 + fraction / 2^S::mbits)
				// times 2^(exponent - S::bias). Field widening appends zero fraction
				// bits and translates that exponent exactly. The existing general
				// conversion owns subnormal normalization and NaN quieting.
				auto const raw{binary_interchange_words(source)};
				auto const exponent{decimal_field(raw, S::mbits, S::ebits)};
				if (exponent - 1u < (1u << S::ebits) - 2u)
				{
					auto result{binary_words_shift(decimal_low_bits(raw, S::mbits), D::mbits - S::mbits)};
					decimal_set_field(result, D::mbits, D::ebits, exponent + static_cast<unsigned>(D::bias - S::bias));
					decimal_set_field(result, D::mbits + D::ebits, 1u, decimal_field(raw, S::mbits + S::ebits, 1u));
					return binary_interchange_from_words<To>(result);
				}
			}
		}
#if defined(__SIZEOF_INT128__)
		if constexpr (fp_floating_point_is_ibm_double_double<From>)
		{
			if (ibm_double_double_has_sparse_significand(source))
			{
				auto const high{::fast_io::bit_cast<ibm_double_double_storage>(source).high};
				return binary_interchange_from_decimal<To>(exact_decimal_from_sparse_ibm_double_double(source), get_punned_result(high).sign);
			}
		}
#endif
		auto const parts{cast_source_binary_parts(source)};
		if constexpr (is_software_binary<To>)
		{
			using L = binary_interchange_layout<To::format>;
			return binary_interchange_from_words<To>(binary_cast_words(parts, L::mbits, L::ebits));
		}
		else if constexpr (ofp8_value<To>)
		{
			constexpr auto m{fp8_fraction_bits<To::format>};
			auto bits{binary_cast_words(parts, m, 7u - m, To::format == fp8_format::e4m3fn)};
			return To::from_bits(static_cast<::std::uint_least8_t>(bits.low));
		}
		else if constexpr (my_integral<To>)
		{
			return cast_binary_parts_to_integer<To>(parts);
		}
		else if constexpr (decimal_interchange_value<To>)
		{
			if (parts.category != decimal_class::finite)
			{
				return cast_binary_words_special_to_decimal<To>(parts);
			}
			return cast_finite_decimal<To>(binary_interchange_exact_decimal(source), parts.negative, true);
		}
		else if constexpr (native_binary128_interchange<To>)
		{
			return ::std::bit_cast<To>(binary_native_word_order(binary_cast_words(parts, 112u, 15u)));
		}
		else
		{
			using L = iec559_traits<To>;
			To result{};
#if defined(__SIZEOF_INT128__)
			if constexpr (fp_floating_point_is_ibm_double_double<To>)
			{
				if (parts.category == decimal_class::finite && binary_words_nonzero(parts.coefficient))
				{
					result = cast_exact_decimal_to_native_binary<To>(binary_interchange_exact_decimal(source), parts.negative);
				}
				else
				{
					auto const hi{binary_cast_words(parts, 52u, 11u)};
					fp_assign_ibm_double_double_components(result, ::fast_io::bit_cast<double>(hi.low), 0.0);
				}
			}
			else
#endif
			{
				auto const raw{binary_cast_words(parts, L::mbits, L::ebits)};
				if constexpr (fp_floating_point_is_float80<To>)
				{
					auto const exponent{decimal_field(raw, 63u, 15u)};
					auto fraction{decimal_low_bits(raw, 63u).low};
					if (exponent)
					{
						fraction |= UINT64_C(1) << 63u;
					}
					fp_assign_float80_bits(result, fraction, exponent, parts.negative);
				}
				else
				{
					fp_assign_bits(result, fp_translate_nan_encoding<L::mbits, L::ebits>(decimal_join_words<typename L::mantissa_type>(raw)));
				}
			}
			return result;
		}
	}
}

template <typename To, typename From>
[[nodiscard]] inline constexpr To interchange_numeric_cast(From const &source) noexcept
{
	if constexpr (is_software_binary<To> || is_software_binary<From>)
	{
		return cast_with_software_binary<To>(source);
	}
	else if constexpr (native_binary128_interchange<From> && !scan_decfloat_supported_floating_point<From>)
	{
		return interchange_numeric_cast<To>(binary_interchange_from_words<software_float128>(binary_native_word_order(::fast_io::bit_cast<binary128_words>(source))));
	}
	else if constexpr (native_binary128_interchange<To> && !scan_decfloat_supported_floating_point<To>)
	{
		auto const intermediate{interchange_numeric_cast<software_float128>(source)};
		return ::std::bit_cast<To>(binary_native_word_order(binary_interchange_words(intermediate)));
	}
	else if constexpr (decimal_interchange_value<From>)
	{
		auto const value{decimal_decode(source)};
		if constexpr (decimal_interchange_value<To>)
		{
			if (value.category != decimal_class::finite)
			{
				return cast_decimal_special<To>(value);
			}
			return cast_finite_decimal<To>(value, value.negative, false);
		}
		else if constexpr (ofp8_value<To>)
		{
			if (value.category == decimal_class::infinity)
			{
				return cast_fp8_code<To>(cast_fp8_overflow<To>, value.negative);
			}
			if (value.category != decimal_class::finite)
			{
				return cast_fp8_code<To>(To::format == fp8_format::e4m3fn ? 127u : 126u | (value.digits[value.size - 1u] & 1u), value.negative);
			}
			return cast_decimal_to_fp8<To>(value, value.negative);
		}
		else if constexpr (my_integral<To>)
		{
			return cast_decimal_to_integer<To>(value);
		}
		else
		{
			return cast_decimal_to_binary<To>(value);
		}
	}
	else if constexpr (ofp8_value<From>)
	{
		auto const code{static_cast<unsigned>(source.bits & 127u)};
		auto const negative{(source.bits & 128u) != 0u};
		if (fp8_last_finite<From::format> < code)
		{
			auto const inf{From::format == fp8_format::e5m2 && code == 124u};
			auto const payload{From::format == fp8_format::e5m2 ? code & 1u : 3u};
			if constexpr (ofp8_value<To>)
			{
				return cast_fp8_code<To>(inf ? cast_fp8_overflow<To> : (To::format == fp8_format::e4m3fn ? 127u : 126u | (payload & 1u)), negative);
			}
			else if constexpr (decimal_interchange_value<To>)
			{
				return cast_binary_special_to_decimal<To>(payload, inf, negative);
			}
			else if constexpr (::std::same_as<To, bool>)
			{
				return true;
			}
			else if constexpr (my_integral<To>)
			{
				::fast_io::fast_terminate();
			}
			else
			{
				using trait = iec559_traits<To>;
				using U = typename trait::mantissa_type;
				return cast_binary_special<To>(static_cast<U>(static_cast<U>(payload) << (trait::mbits - fp8_fraction_bits<From::format>)), inf, negative);
			}
		}
		auto const exact{fp8_expand(source)};
		if constexpr (decimal_interchange_value<To> || ofp8_value<To>)
		{
			return interchange_numeric_cast<To>(exact);
		}
		else if constexpr (::std::same_as<To, bool>)
		{
			return code != 0u;
		}
		else if constexpr (my_integral<To>)
		{
			// Every finite OFP8 value fits in a signed 32-bit integer.
			auto const trunc{static_cast<::std::int_least32_t>(exact)};
			if constexpr (::std::numeric_limits<To>::digits < 31)
			{
				if (trunc < static_cast<::std::int_least32_t>((::std::numeric_limits<To>::min)()) ||
					static_cast<::std::int_least32_t>((::std::numeric_limits<To>::max)()) < trunc)
				{
					::fast_io::fast_terminate();
				}
			}
			if constexpr (!my_signed_integral<To>)
			{
				if (trunc < 0)
				{
					::fast_io::fast_terminate();
				}
			}
			return static_cast<To>(trunc);
		}
		else
		{
			return static_cast<To>(exact); // Every supported native binary type contains every finite OFP8 value.
		}
	}
	else if constexpr (my_integral<From>)
	{
		auto const magnitude{cast_integer_magnitude(source)};
		bool negative{};
		if constexpr (my_signed_integral<From>)
		{
			negative = source < 0;
		}
		if constexpr (decimal_interchange_value<To>)
		{
			return cast_finite_decimal<To>(cast_integer_digits(magnitude), negative, true);
		}
		else
		{
			if (65536u <= magnitude)
			{
				return cast_fp8_code<To>(cast_fp8_overflow<To>, negative);
			}
			auto exact{static_cast<float>(static_cast<unsigned>(magnitude))};
			return interchange_numeric_cast<To>(negative ? -exact : exact);
		}
	}
	else
	{
#if defined(__SIZEOF_INT128__)
		if constexpr (fp_floating_point_is_ibm_double_double<From>)
		{
			if (ibm_double_double_has_sparse_significand(source))
			{
				auto const decimal{exact_decimal_from_sparse_ibm_double_double(source)};
				auto const high{::fast_io::bit_cast<ibm_double_double_storage>(source).high};
				auto const negative{get_punned_result(high).sign};
				if constexpr (decimal_interchange_value<To>)
				{
					return cast_finite_decimal<To>(decimal, negative, true);
				}
				else
				{
					return cast_decimal_to_fp8<To>(decimal, negative);
				}
			}
		}
#endif
		using trait = iec559_traits<From>;
		using U = typename trait::mantissa_type;
		auto const fields{[&]() constexpr {
			if constexpr (fp_floating_point_is_ibm_double_double<From>)
			{
				return get_punned_result(source);
			}
			else
			{
				return compiler_constant_floating_capture_fields<From>(source);
			}
		}()};
		if (fields.exponent == (1u << trait::ebits) - 1u)
		{
			auto const infinity{fields.mantissa == 0};
			constexpr auto quiet{static_cast<U>(U{1} << (trait::mbits - 1u))};
			auto const diagnostic{static_cast<U>(fields.mantissa & (quiet - 1u))};
			if constexpr (decimal_interchange_value<To>)
			{
#if defined(__SIZEOF_INT128__)
				if constexpr (fp_floating_point_is_ibm_double_double<From>)
				{
					// IBM's diagnostic comes from its leading binary64. Field
					// extraction left-aligns it in the synthetic significand for
					// binary consumers. Recover the integer before changing radix.
					constexpr auto shift{trait::mbits - iec559_traits<double>::mbits};
					return cast_binary_special_to_decimal<To>(
						static_cast<::std::uint_least64_t>(diagnostic >> shift), infinity, fields.sign);
				}
				else
#endif
				{
					return cast_binary_special_to_decimal<To>(diagnostic, infinity, fields.sign);
				}
			}
			else
			{
				return cast_fp8_code<To>(infinity ? cast_fp8_overflow<To> : (To::format == fp8_format::e4m3fn ? 127u : 126u | static_cast<unsigned>(diagnostic >> (trait::mbits - 2u))), fields.sign);
			}
		}
		if constexpr (decimal_interchange_value<To>)
		{
			if (!fields.exponent && !fields.mantissa)
			{
				decimal_components<decimal_interchange_binding<To>::bits> zero{};
				zero.negative = fields.sign;
				return decimal_encode<To>(zero);
			}
			auto const decimal{exact_precision_from_binary<From>(fields.mantissa, fields.exponent)};
			return cast_finite_decimal<To>(decimal, fields.sign, true);
		}
		else
		{
			auto magnitude{fields.sign ? -source : source};
			// All OFP8 midpoints, including the half-minimum subnormal and
			// overflow threshold, are exactly representable in every native
			// binary input format. Compare in that input format.
			return cast_fp8_interval<To>(fields.sign, [&](float middle) constexpr {
				auto const boundary{static_cast<From>(middle)};
				return magnitude < boundary ? -1 : static_cast<int>(magnitude > boundary);
			});
		}
	}
}

} // namespace fast_io::details
