#pragma once

#include "../floating/interchange_types.h"

namespace fast_io
{
namespace details
{

template <unsigned width>
struct decimal_layout
{
	inline static constexpr unsigned bits{width};
	inline static constexpr unsigned precision{width == 32u ? 7u : width == 64u ? 16u
																				: 34u};
	inline static constexpr unsigned declets{(precision - 1u) / 3u};
	inline static constexpr unsigned continuation{width - 6u - declets * 10u};
	inline static constexpr unsigned bid_coefficient{width - 3u - continuation};
	inline static constexpr int bias{width == 32u ? 101 : width == 64u ? 398
																	   : 6176};
	inline static constexpr int minimum_quantum{-bias};
	inline static constexpr int maximum_quantum{static_cast<int>(3u << continuation) - 1 - bias};
};

template <typename T>
struct decimal_interchange_binding;

template <unsigned width, decimal_encoding code>
struct decimal_interchange_binding<basic_decimal_interchange<width, code>> : decimal_layout<width>
{
	inline static constexpr auto encoding{code};
	inline static constexpr bool native{};
};

#if defined(FAST_IO_HAS_NATIVE_DECIMAL_FLOAT)
template <unsigned width>
struct native_decimal_binding : decimal_layout<width>
{
#if defined(__DECIMAL_BID_FORMAT__) && __DECIMAL_BID_FORMAT__
	inline static constexpr auto encoding{decimal_encoding::bid};
#else
	inline static constexpr auto encoding{decimal_encoding::dpd};
#endif
	inline static constexpr bool native{true};
};

template <>
struct decimal_interchange_binding<decimal32> : native_decimal_binding<32u>
{};
template <>
struct decimal_interchange_binding<decimal64> : native_decimal_binding<64u>
{};
template <>
struct decimal_interchange_binding<decimal128> : native_decimal_binding<128u>
{};
#endif

template <typename T>
concept decimal_interchange_value = requires { decimal_interchange_binding<::std::remove_cvref_t<T>>::precision; };

template <typename U>
inline constexpr decimal128_words decimal_split_words(U bits) noexcept
{
	if constexpr (::std::same_as<U, decimal128_words>)
	{
		return bits;
	}
	else if constexpr (::std::numeric_limits<U>::digits > 64)
	{
		return {static_cast<::std::uint_least64_t>(bits), static_cast<::std::uint_least64_t>(bits >> 64u)};
	}
	else
	{
		return {bits, 0u};
	}
}

template <typename U>
inline constexpr U decimal_join_words(decimal128_words words) noexcept
{
	if constexpr (::std::same_as<U, decimal128_words>)
	{
		return words;
	}
	else if constexpr (::std::numeric_limits<U>::digits > 64)
	{
		return (static_cast<U>(words.high) << 64u) | words.low;
	}
	else
	{
		return static_cast<U>(words.low);
	}
}

template <decimal_interchange_value T>
inline constexpr decimal128_words decimal_bits(T value) noexcept
{
	using layout = decimal_interchange_binding<T>;
	if constexpr (!layout::native)
	{
		return decimal_split_words(value.bits);
	}
	else
	{
		using storage = decimal_storage<layout::bits>;
		auto words{decimal_split_words(::fast_io::bit_cast<storage>(value))};
		if constexpr (::std::same_as<storage, decimal128_words> && ::std::endian::native == ::std::endian::big)
		{
			return {words.high, words.low};
		}
		return words;
	}
}

template <decimal_interchange_value T>
inline constexpr T decimal_from_bits(decimal128_words words) noexcept
{
	using layout = decimal_interchange_binding<T>;
	using storage = decimal_storage<layout::bits>;
	if constexpr (layout::native)
	{
		if constexpr (::std::same_as<storage, decimal128_words> && ::std::endian::native == ::std::endian::big)
		{
			words = {words.high, words.low};
		}
		return ::std::bit_cast<T>(decimal_join_words<storage>(words));
	}
	else
	{
		return T::from_bits(decimal_join_words<storage>(words));
	}
}

// Interchange exponents span up to fifteen bits. A decimal128 declet
// can straddle bit 64, so the word split is performed explicitly in both directions.
inline constexpr unsigned decimal_field(decimal128_words words, unsigned offset, unsigned count) noexcept
{
	auto mask{(1u << count) - 1u};
	if (64u <= offset)
	{
		return static_cast<unsigned>(words.high >> (offset - 64u)) & mask;
	}
	auto value{words.low >> offset};
	if (64u - offset < count)
	{
		value |= words.high << (64u - offset);
	}
	return static_cast<unsigned>(value) & mask;
}

inline constexpr void decimal_set_field(decimal128_words &words, unsigned offset, unsigned count, unsigned field) noexcept
{
	auto value{static_cast<::std::uint_least64_t>(field)};
	if (64u <= offset)
	{
		words.high |= value << (offset - 64u);
	}
	else
	{
		words.low |= value << offset;
		if (64u - offset < count)
		{
			words.high |= value >> (64u - offset);
		}
	}
}

inline constexpr decimal128_words decimal_low_bits(decimal128_words words, unsigned count) noexcept
{
	if (64u < count)
	{
		words.high &= (static_cast<::std::uint_least64_t>(1u) << (count - 64u)) - 1u;
	}
	else
	{
		words.high = 0;
		if (count < 64u)
		{
			words.low &= (static_cast<::std::uint_least64_t>(1u) << count) - 1u;
		}
	}
	return words;
}

/**
 * Each DPD declet encodes three digits. Their high bits select one of eight
 * layouts, their low bits always occupy positions 7, 4 and 0 respectively.
 * The 24 redundant encodings of three large digits differ only in bits 9:8.
 * See Cowlishaw, "Densely Packed Decimal Encoding" (2002), compression table.
 */
inline constexpr unsigned decimal_pack_declet(unsigned a, unsigned b, unsigned c) noexcept
{
	auto const low{((a & 1u) << 7u) | ((b & 1u) << 4u) | (c & 1u)};
	auto const a_mid{a & 6u};
	auto const b_mid{b & 6u};
	auto const c_mid{c & 6u};
	switch (((a >> 3u) << 2u) | ((b >> 3u) << 1u) | (c >> 3u))
	{
	case 0u:
		return low | (a_mid << 7u) | (b_mid << 4u) | c_mid;
	case 1u:
		return low | (a_mid << 7u) | (b_mid << 4u) | 8u;
	case 2u:
		return low | (a_mid << 7u) | (c_mid << 4u) | 10u;
	case 3u:
		return low | (a_mid << 7u) | 78u;
	case 4u:
		return low | (c_mid << 7u) | (b_mid << 4u) | 12u;
	case 5u:
		return low | (b_mid << 7u) | 46u;
	case 6u:
		return low | (c_mid << 7u) | 14u;
	default:
		return low | 110u;
	}
}

struct decimal_declet_tables
{
	::std::uint_least16_t to_dpd[1000]{};
	::std::uint_least16_t from_dpd[1024]{};

	consteval decimal_declet_tables() noexcept
	{
		for (unsigned value{}; value != 1000u; ++value)
		{
			auto const a{value / 100u};
			auto const b{value / 10u % 10u};
			auto const c{value % 10u};
			auto const code{decimal_pack_declet(a, b, c)};
			to_dpd[value] = static_cast<::std::uint_least16_t>(code);
			auto const bcd{static_cast<::std::uint_least16_t>((a << 8u) | (b << 4u) | c)};
			from_dpd[code] = bcd;
			if (8u <= a && 8u <= b && 8u <= c)
			{
				for (unsigned alias{1u}; alias != 4u; ++alias)
				{
					from_dpd[code | (alias << 8u)] = bcd;
				}
			}
		}
	}
};

inline constexpr decimal_declet_tables decimal_declets{};

enum class decimal_class : unsigned char
{
	finite,
	infinity,
	quiet_nan,
	signaling_nan
};

// Numeric digits match the existing exact decimal presentation writers. One
// extra slot holds the guard digit while scanning an arbitrarily long token.
template <unsigned width>
struct decimal_components
{
	unsigned char digits[decimal_layout<width>::precision + 1u]{};
	::std::size_t size{1u};
	::std::int_least32_t exponent{};
	bool negative{};
	decimal_class category{};
};

inline constexpr unsigned decimal_divide_billion(decimal128_words &value) noexcept
{
	constexpr ::std::uint_least64_t base{1000000000u};
	if (!value.high)
	{
		auto const remainder{static_cast<unsigned>(value.low % base)};
		value.low /= base;
		return remainder;
	}
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	!defined(_MSC_VER) && defined(__GNUC__) && !defined(__clang__)
	if (!::std::is_constant_evaluated())
	{
		// The high-word remainder is below the divisor, so the low-word
		// quotient fits in one word and needs only one hardware divide.
		auto const low{::fast_io::intrinsics::udivbigbysmalltosmalldefault(value.high % base, value.low, base)};
		value = {low.quotient, value.high / base};
		return static_cast<unsigned>(low.remainder);
	}
#endif
#if defined(__SIZEOF_INT128__)
	auto const full{decimal_join_words<__uint128_t>(value)};
	value = decimal_split_words(full / base);
	return static_cast<unsigned>(full % base);
#else
	// Long division in radix 2^32. Each dividend is under 10^9 * 2^32.
	auto remainder{value.high % base};
	value.high /= base;
	auto pair{(remainder << 32u) | (value.low >> 32u)};
	auto quotient{(pair / base) << 32u};
	pair = ((pair % base) << 32u) | (value.low & 0xffffffffu);
	value.low = quotient | (pair / base);
	return static_cast<unsigned>(pair % base);
#endif
}

inline constexpr void decimal_multiply_add(decimal128_words &value, unsigned multiplier, unsigned addend) noexcept
{
	auto const low{(value.low & 0xffffffffu) * multiplier + addend};
	auto const high{(value.low >> 32u) * multiplier + (low >> 32u)};
	value.low = (high << 32u) | (low & 0xffffffffu);
	value.high = value.high * multiplier + (high >> 32u);
}

template <unsigned width>
inline constexpr void decimal_bid_digits(decimal_components<width> &out, decimal128_words coefficient, unsigned maximum_digits) noexcept
{
	// Four base-10^9 blocks suffice for every 114-bit BID coefficient.
	// Expand each block into a complete nine-digit slot. Narrow coefficients
	// get their own smaller buffer and never materialize a 128-bit quotient.
	constexpr auto capacity{(decimal_layout<width>::precision + 9u) / 9u * 9u};
	unsigned char digits[capacity];
	auto first{capacity};
	do
	{
		first -= 9u;
		::std::uint_least32_t block{};
		if constexpr (width < 128u)
		{
			block = static_cast<::std::uint_least32_t>(coefficient.low % 1000000000u);
			coefficient.low /= 1000000000u;
		}
		else
		{
			block = decimal_divide_billion(coefficient);
		}
		exact_decimal_write_nine_digits(digits + first, block);
	} while (first && (coefficient.low || coefficient.high));
	while (first + 1u != capacity && !digits[first])
	{
		++first;
	}
	auto const size{capacity - first};
	if (maximum_digits < size)
	{
		return; // The out-of-range BID coefficient represents zero.
	}
	out.size = size;
	::fast_io::details::non_overlapped_copy_n(digits + first, size, out.digits);
}

#if defined(FAST_IO_ENABLE_POWER_DFP) && defined(__powerpc__) && defined(FAST_IO_HAS_NATIVE_DECIMAL_FLOAT) && \
	!(defined(__DECIMAL_BID_FORMAT__) && __DECIMAL_BID_FORMAT__)
// GCC exposes the same predefined macros and __has_builtin answers for both
// -mhard-dfp and -mno-hard-dfp. The build must explicitly enable this leaf
// when its minimum processor and floating-point ABI support hard DFP.
#define FAST_IO_DETAIL_POWER_DECIMAL_DPD 1

/// @brief Expands the low N packed BCD digits in most-significant-first order.
template <unsigned N>
inline void decimal_power_unpack(unsigned char *out, ::std::uint_least64_t packed) noexcept
{
	for (unsigned i{N}; i;)
	{
		out[--i] = static_cast<unsigned char>(packed & 15u);
		packed >>= 4u;
	}
}

/// @brief Decodes a finite DPD coefficient and its quantum using the POWER decimal unit.
/// @details DDEDPDQ returns the low 32 of 34 digits. The combination field
/// and most significant declet provide the remaining two. Widening decimal32
/// to decimal64 is exact and preserves the quantum. Special values never enter this leaf.
template <decimal_interchange_value T>
inline void decimal_power_decode_finite(decimal128_words words,
										unsigned combination, decimal_components<decimal_interchange_binding<T>::bits> &out) noexcept
{
	using layout = decimal_interchange_binding<T>;
	if constexpr (layout::bits == 128u)
	{
		auto source{decimal_from_bits<decimal128>(words)};
		out.exponent = static_cast<int>(__builtin_dxexq(source)) - layout::bias;
		auto packed{decimal_bits(__builtin_ddedpdq(0, source))};
		out.digits[0] = static_cast<unsigned char>(combination < 24u ? combination & 7u : 8u + (combination & 1u));
		out.digits[1] = static_cast<unsigned char>(decimal_declets.from_dpd[decimal_field(words, 100u, 10u)] >> 8u);
		decimal_power_unpack<16>(out.digits + 2, packed.high);
		decimal_power_unpack<16>(out.digits + 18, packed.low);
	}
	else
	{
		decimal64 source{};
		if constexpr (layout::bits == 32u)
		{
			source = static_cast<decimal64>(decimal_from_bits<decimal32>(words));
		}
		else
		{
			source = decimal_from_bits<decimal64>(words);
		}
		out.exponent = static_cast<int>(__builtin_dxex(source)) - decimal_layout<64>::bias;
		decimal_power_unpack<layout::precision>(out.digits, decimal_bits(__builtin_ddedpd(0, source)).low);
	}
}

/// @brief Packs at most sixteen validated decimal digits into one unsigned BCD word.
inline ::std::uint_least64_t decimal_power_pack(unsigned char const *first, unsigned char const *last) noexcept
{
	::std::uint_least64_t packed{};
	while (first != last)
	{
		packed = (packed << 4u) | *first++;
	}
	return packed;
}

/// @brief Packs a validated, finite decimal coefficient and its quantum without text or decimal arithmetic.
/// @details Denbcdq packs up to 32 digits. Coefficients of 33 or 34 digits replace the
/// high declet and combination field. The decimal32 result is already exactly representable.
template <decimal_interchange_value T>
inline T decimal_power_encode_finite(decimal_components<decimal_interchange_binding<T>::bits> const &value) noexcept
{
	using layout = decimal_interchange_binding<T>;
	decimal128_words words{};
	if constexpr (layout::bits == 128u)
	{
		auto const low_start{value.size > 16u ? value.size - 16u : 0u};
		auto const high_start{value.size > 32u ? value.size - 32u : 0u};
		auto const *digits{value.digits};
		auto const packed{decimal_from_bits<decimal128>({decimal_power_pack(digits + low_start, digits + value.size),
														 decimal_power_pack(digits + high_start, digits + low_start)})};
		auto const biased{static_cast<unsigned>(value.exponent + layout::bias)};
		words = decimal_bits(__builtin_diexq(biased, __builtin_denbcdq(0, packed)));
		if (high_start)
		{
			unsigned const lead{high_start == 2u ? digits[0] : 0u};
			auto const *top{digits + high_start - 1u};
			unsigned const high{biased >> layout::continuation};
			unsigned const combination{lead < 8u ? (high << 3u) | lead : 24u | (high << 1u) | (lead & 1u)};
			words.high &= ~((UINT64_C(31) << 58u) | (UINT64_C(1023) << 36u));
			decimal_set_field(words, 122u, 5u, combination);
			decimal_set_field(words, 100u, 10u, decimal_pack_declet(top[0], top[1], top[2]));
		}
	}
	else
	{
		auto const packed{decimal_from_bits<decimal64>({decimal_power_pack(value.digits, value.digits + value.size), 0u})};
		auto const native{__builtin_diex(value.exponent + decimal_layout<64>::bias, __builtin_denbcd(0, packed))};
		if constexpr (layout::bits == 32u)
		{
			words = decimal_bits(static_cast<decimal32>(native));
		}
		else
		{
			words = decimal_bits(native);
		}
	}
	if (value.negative)
	{
		decimal_set_field(words, layout::bits - 1u, 1u, 1u);
	}
	return decimal_from_bits<T>(words);
}
#endif

template <decimal_interchange_value T>
inline constexpr auto decimal_decode(T value) noexcept
{
	using layout = decimal_interchange_binding<T>;
	constexpr auto width{layout::bits};
	constexpr auto trailing{layout::declets * 10u};
	auto const words{decimal_bits(value)};
	decimal_components<width> result{};
	result.negative = decimal_field(words, width - 1u, 1u) != 0;
	auto const combination{decimal_field(words, width - 6u, 5u)};
	if (30u <= combination)
	{
		if (combination == 30u)
		{
			result.category = decimal_class::infinity;
			return result;
		}
		result.category = decimal_field(words, width - 7u, 1u) ? decimal_class::signaling_nan : decimal_class::quiet_nan;
	}
	if constexpr (layout::encoding == decimal_encoding::bid)
	{
		if (result.category != decimal_class::finite)
		{
			decimal_bid_digits(result, decimal_low_bits(words, trailing), layout::precision - 1u);
			return result;
		}
		auto const steered{24u <= combination};
		auto const stored{layout::bid_coefficient - (steered ? 2u : 0u)};
		auto coefficient{decimal_low_bits(words, stored)};
		if (steered)
		{
			decimal_set_field(coefficient, layout::bid_coefficient, 1u, 1u);
		}
		result.exponent = static_cast<int>(decimal_field(words, stored, layout::continuation + 2u)) - layout::bias;
		decimal_bid_digits(result, coefficient, layout::precision);
	}
	else
	{
#ifdef FAST_IO_DETAIL_POWER_DECIMAL_DPD
		if (!::std::is_constant_evaluated() && result.category == decimal_class::finite)
		{
			decimal_power_decode_finite<T>(words, combination, result);
		}
		else
#endif
		{
			if (result.category == decimal_class::finite)
			{
				auto const large{24u <= combination};
				auto const high_exponent{large ? (combination >> 1u) & 3u : combination >> 3u};
				result.exponent = static_cast<int>((high_exponent << layout::continuation) |
												   decimal_field(words, trailing, layout::continuation)) -
								  layout::bias;
				result.digits[0] = static_cast<unsigned char>(large ? 8u + (combination & 1u) : combination & 7u);
			}
			if constexpr (width == 128u)
			{
				// Coefficients of at most six digits fit in the two low declets.
				// Decode these without expanding all the leading zero groups.
				auto const coefficient{decimal_low_bits(words, trailing)};
				if (!result.digits[0] && !coefficient.high && (coefficient.low >> 20u) == 0u) [[unlikely]]
				{
					auto const upper{static_cast<unsigned>(decimal_declets.from_dpd[coefficient.low >> 10u])};
					auto bcd{(upper << 12u) | decimal_declets.from_dpd[coefficient.low & 1023u]};
					result.size = (static_cast<unsigned>(::std::bit_width(bcd | 1u)) + 3u) / 4u;
					for (auto i{result.size}; i; bcd >>= 4u)
					{
						result.digits[--i] = static_cast<unsigned char>(bcd & 15u);
					}
					return result;
				}
			}
			unsigned index{1u};
			for (unsigned offset{trailing}; offset;)
			{
				offset -= 10u;
				auto const bcd{decimal_declets.from_dpd[decimal_field(words, offset, 10u)]};
				result.digits[index++] = static_cast<unsigned char>(bcd >> 8u);
				result.digits[index++] = static_cast<unsigned char>((bcd >> 4u) & 15u);
				result.digits[index++] = static_cast<unsigned char>(bcd & 15u);
			}
		}
		unsigned first{};
		while (first + 1u < layout::precision && !result.digits[first])
		{
			++first;
		}
		result.size = layout::precision - first;
		for (unsigned i{}; first && i != result.size; ++i)
		{
			result.digits[i] = result.digits[first + i];
		}
	}
	return result;
}

template <decimal_interchange_value T>
inline constexpr T decimal_encode(decimal_components<decimal_interchange_binding<T>::bits> const &value) noexcept
{
	using layout = decimal_interchange_binding<T>;
	constexpr auto width{layout::bits};
	constexpr auto trailing{layout::declets * 10u};
	decimal128_words words{};
	auto const finite{value.category == decimal_class::finite};
	if constexpr (layout::encoding == decimal_encoding::bid)
	{
		if constexpr (width < 128u)
		{
			for (::std::size_t i{}; i != value.size; ++i)
			{
				words.low = words.low * 10u + value.digits[i];
			}
		}
		else
		{
			// Every complete block after the leading partial block advances
			// by the same power of ten.
			auto const first{(value.size - 1u) % 9u + 1u};
			for (::std::size_t i{}; i != first; ++i)
			{
				words.low = words.low * 10u + value.digits[i];
			}
			for (auto i{first}; i != value.size;)
			{
				unsigned block{};
				for (auto end{i + 9u}; i != end; ++i)
				{
					block = block * 10u + value.digits[i];
				}
				decimal_multiply_add(words, 1000000000u, block);
			}
		}
		if (finite)
		{
			auto const steered{decimal_field(words, layout::bid_coefficient, 1u) != 0u};
			auto const stored{layout::bid_coefficient - (steered ? 2u : 0u)};
			if (steered)
			{
				words = decimal_low_bits(words, stored);
				decimal_set_field(words, width - 3u, 2u, 3u);
			}
			decimal_set_field(words, stored, layout::continuation + 2u, static_cast<unsigned>(value.exponent + layout::bias));
		}
	}
	else
	{
#ifdef FAST_IO_DETAIL_POWER_DECIMAL_DPD
		if (!::std::is_constant_evaluated() && finite)
			return decimal_power_encode_finite<T>(value);
#endif
		auto i{value.size};
		for (unsigned offset{}; i && offset != trailing; offset += 10u)
		{
			unsigned group{value.digits[--i]};
			if (i)
			{
				group += 10u * value.digits[--i];
			}
			if (i)
			{
				group += 100u * value.digits[--i];
			}
			decimal_set_field(words, offset, 10u, decimal_declets.to_dpd[group]);
		}
		if (finite)
		{
			auto const lead{i ? value.digits[0] : 0u};
			auto const biased{static_cast<unsigned>(value.exponent + layout::bias)};
			auto const high{biased >> layout::continuation};
			auto const combination{lead < 8u ? (high << 3u) | lead : 24u | (high << 1u) | (lead & 1u)};
			decimal_set_field(words, trailing, layout::continuation, biased & ((1u << layout::continuation) - 1u));
			decimal_set_field(words, width - 6u, 5u, combination);
		}
	}
	if (!finite)
	{
		decimal_set_field(words, width - 6u, 5u, value.category == decimal_class::infinity ? 30u : 31u);
		if (value.category == decimal_class::signaling_nan)
		{
			decimal_set_field(words, width - 7u, 1u, 1u);
		}
	}
	if (value.negative)
	{
		decimal_set_field(words, width - 1u, 1u, 1u);
	}
	return decimal_from_bits<T>(words);
}

template <decimal_interchange_value T>
struct interchange_floating_traits<T>
{
	inline static constexpr unsigned decimal_width{decimal_interchange_binding<T>::bits};
	static constexpr bool negative(T value) noexcept
	{
		return decimal_field(decimal_bits(value), decimal_interchange_binding<T>::bits - 1u, 1u) != 0;
	}
	static constexpr bool finite(T value) noexcept
	{
		return decimal_field(decimal_bits(value), decimal_interchange_binding<T>::bits - 6u, 5u) < 30u;
	}
	static constexpr int adjusted_exponent(T value) noexcept
	{
		auto const fields{decimal_decode(value)};
		return fields.digits[0] ? fields.exponent + static_cast<int>(fields.size) - 1 : 0;
	}
};

} // namespace details
} // namespace fast_io

#ifdef FAST_IO_DETAIL_POWER_DECIMAL_DPD
#undef FAST_IO_DETAIL_POWER_DECIMAL_DPD
#endif
