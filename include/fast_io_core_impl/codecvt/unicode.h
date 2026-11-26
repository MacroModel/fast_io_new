#pragma once
#include "unicode_wire_cursor.h"

namespace fast_io::details::codecvt
{
/** @brief Reports why conversion stopped at a complete scalar boundary. */
enum class unicode_status : unsigned char
{
	complete,
	incomplete,
	invalid,
	output_full
};

/** @brief Preserves the exact input and output cursors for bounded and streaming callers. */
template <typename From, typename To>
struct unicode_progress
{
	From const *input;
	To *output;
	unicode_status status;
};

/** @brief Loads a wire-ordered code unit independently of the host's byte or word order. */
template <encoding_scheme Order, typename T>
inline constexpr ::std::uint_least32_t unicode_load(T value) noexcept
{
	using U = ::std::make_unsigned_t<T>;
	if constexpr (sizeof(T) == 1 || Order == encoding_scheme::utf)
	{
		return static_cast<U>(value);
	}
	else
	{
		auto bytes{::std::bit_cast<::fast_io::freestanding::array<unsigned char, sizeof(T)>>(value)};
		::std::uint_least32_t result{};
		for (::std::size_t i{}; i != sizeof(T); ++i)
		{
			result = (result << 8u) | bytes[Order == encoding_scheme::utf_be ? i : sizeof(T) - 1 - i];
		}
		return result;
	}
}

/** @brief Constructs the numeric code unit whose object representation has the requested wire order. */
template <encoding_scheme Order, typename T>
inline constexpr T unicode_unit(::std::uint_least32_t value) noexcept
{
	if constexpr (sizeof(T) == 1 || Order == encoding_scheme::utf)
	{
		return static_cast<T>(value);
	}
	else
	{
		::fast_io::freestanding::array<unsigned char, sizeof(T)> bytes{};
		for (::std::size_t i{}; i != sizeof(T); ++i)
		{
			bytes[Order == encoding_scheme::utf_be ? sizeof(T) - 1 - i : i] = static_cast<unsigned char>(value & 255u);
			value >>= 8u;
		}
		return ::std::bit_cast<T>(bytes);
	}
}

/** @brief Records the scalar or the maximal subpart of one ill-formed sequence. */
struct unicode_scalar
{
	::std::uint_least32_t value;
	unsigned units;
	unicode_status status;
};

/** @brief Validates one scalar without consuming a noncontinuation or reading past the input. */
template <encoding_scheme Order, typename Input>
inline constexpr unicode_scalar unicode_decode(Input p, ::std::size_t n) noexcept
{
	using T = unicode_cursor_value_t<Input>;
	auto a{unicode_load<Order>(p[0])};
	constexpr auto bad{unicode_status::invalid}, short_input{unicode_status::incomplete}, ok{unicode_status::complete};
	if constexpr (sizeof(T) == 4)
	{
		return {a, 1u, a > 0x10ffffu || (a >= 0xd800u && a <= 0xdfffu) ? bad : ok};
	}
	else if constexpr (sizeof(T) == 2)
	{
		if (a < 0xd800u || a > 0xdfffu)
		{
			return {a, 1u, ok};
		}
		if (a >= 0xdc00u)
		{
			return {0, 1u, bad};
		}
		if (n == 1u)
		{
			return {0, 1u, short_input};
		}
		auto b{unicode_load<Order>(p[1])};
		if (b < 0xdc00u || b > 0xdfffu)
		{
			return {0, 1u, bad};
		}
		return {static_cast<::std::uint_least32_t>(0x10000ul + ((a - 0xd800ul) << 10u) + b - 0xdc00ul), 2u, ok};
	}
	else
	{
		if (a < 0x80u)
		{
			return {a, 1u, ok};
		}
		if (a < 0xc2u || a > 0xf4u)
		{
			return {0, 1u, bad};
		}
		unsigned width{a < 0xe0u ? 2u : a < 0xf0u ? 3u
												  : 4u};
		auto cp{a & (0x7fu >> width)};
		for (unsigned i{1}; i != width; ++i)
		{
			if (i == n)
			{
				return {0, i, short_input};
			}
			auto const b{unicode_load<Order>(p[i])};
			if ((b & 0xc0u) != 0x80u || (i == 1 && ((a == 0xe0u && b < 0xa0u) || (a == 0xedu && b >= 0xa0u) ||
													(a == 0xf0u && b < 0x90u) || (a == 0xf4u && b >= 0x90u))))
			{
				return {0, i, bad};
			}
			cp = (cp << 6u) | (b & 63u);
		}
		return {cp, width, ok};
	}
}

/** @brief Returns the number of destination units needed for an already validated scalar. */
template <typename T>
inline constexpr unsigned unicode_width(::std::uint_least32_t cp) noexcept
{
	if constexpr (sizeof(T) == 4)
	{
		return 1;
	}
	if constexpr (sizeof(T) == 2)
	{
		return 1u + (cp >= 0x10000u);
	}
	return 1u + (cp >= 0x80u) + (cp >= 0x800u) + (cp >= 0x10000u);
}

/** @brief Emits one validated scalar with exact-width stores in the destination's specified order. */
template <encoding_scheme Order, typename Output>
inline constexpr void unicode_emit(::std::uint_least32_t cp, unsigned width, Output out) noexcept
{
	using T = unicode_cursor_value_t<Output>;
	if constexpr (sizeof(T) == 4)
	{
		*out = unicode_unit<Order, T>(cp);
	}
	else if constexpr (sizeof(T) == 2)
	{
		if (width == 1)
		{
			*out = unicode_unit<Order, T>(cp);
		}
		else
		{
			cp -= 0x10000u;
			out[0] = unicode_unit<Order, T>(0xd800u | (cp >> 10u));
			out[1] = unicode_unit<Order, T>(0xdc00u | (cp & 1023u));
		}
	}
	else if (width == 1)
	{
		*out = static_cast<T>(cp);
	}
	else if (width == 2)
	{
		out[0] = static_cast<T>(0xc0u | (cp >> 6u));
		out[1] = static_cast<T>(0x80u | (cp & 63u));
	}
	else if (width == 3)
	{
		out[0] = static_cast<T>(0xe0u | (cp >> 12u));
		out[1] = static_cast<T>(0x80u | ((cp >> 6u) & 63u));
		out[2] = static_cast<T>(0x80u | (cp & 63u));
	}
	else
	{
		out[0] = static_cast<T>(0xf0u | (cp >> 18u));
		out[1] = static_cast<T>(0x80u | ((cp >> 12u) & 63u));
		out[2] = static_cast<T>(0x80u | ((cp >> 6u) & 63u));
		out[3] = static_cast<T>(0x80u | (cp & 63u));
	}
}
} // namespace fast_io::details::codecvt

#include "unicode_simd.h"

namespace fast_io::details::codecvt
{
/**
 * @brief Transcodes all UTF-8, UTF-16 and UTF-32 width and byte-order pairs with exact bounded progress.
 * @details A vector block commits only after validation and capacity checks. Scalar recovery starts at
 * the same source position. The replacement policy consumes one maximal subpart per U+FFFD. A valid
 * truncated suffix remains with the caller for the next input range in either policy.
 */
template <typename Input, typename Output>
struct unicode_cursor_progress
{
	Input input;
	Output output;
	unicode_status status;
};

#if defined(FAST_IO_UNICODE_FIXED_BMP_WIDEN)
inline constexpr bool unicode_has_bmp_packet{true};
#else
inline constexpr bool unicode_has_bmp_packet{false};

template <unsigned Units, encoding_scheme A, encoding_scheme B, typename Input, typename Output>
inline constexpr bool utf_widen_bmp_packet(Input, Output) noexcept
{
	return false;
}
#endif
#undef FAST_IO_UNICODE_FIXED_BMP_WIDEN

template <encoding_scheme FromOrder, encoding_scheme ToOrder, bool Replace, typename Input, typename Output>
inline constexpr unicode_cursor_progress<Input, Output> unicode_convert_range(Input first, Input last, Output out,
																			 ::std::size_t room) noexcept
{
	using To = unicode_cursor_value_t<Output>;
#if defined(FAST_IO_UNICODE_SHORT_INPUT)
	if constexpr (sizeof(unicode_cursor_value_t<Input>) == 1 && sizeof(To) > 1)
	{
		auto const extent{last - first};
		if (!::std::is_constant_evaluated() && extent >= 16)
		{
			if (extent < 128)
			{
				if (utf_short_input_run<ToOrder>(first, last, out, room))
				{
					return {first, out, unicode_status::complete};
				}
			}
			else if constexpr (!Replace && sizeof(To) == 2)
			{
				// Large strict ranges can reject an impossible first byte before probing SIMD.
				if (utf_invalid_lead(static_cast<unsigned char>(*first))) [[unlikely]]
				{
					return {first, out, unicode_status::invalid};
				}
			}
		}
	}
#endif
#if defined(FAST_IO_UNICODE_SHORT_WIDE)
	if constexpr (sizeof(unicode_cursor_value_t<Input>) == 2 && sizeof(To) == 4)
	{
		auto const extent{last - first};
		if (!::std::is_constant_evaluated())
		{
			if (extent < 32)
			{
				if (extent == 16)
				{
					if (room >= 16u && utf_widen_bmp_packet<16, FromOrder, ToOrder>(first, out))
					{
						return {last, out + 16u, unicode_status::complete};
					}
					if (room >= 8u && utf_widen_pair_packet<FromOrder, ToOrder>(first, out))
					{
						return {last, out + 8u, unicode_status::complete};
					}
				}
				if (utf_short_widen<FromOrder, ToOrder>(first, last, out, room))
				{
					return {first, out, unicode_status::complete};
				}
			}
			else if (extent < 128 && utf_widen_mixed_prefix<FromOrder>(first, room < static_cast<::std::size_t>(extent)) &&
				utf_medium_widen<FromOrder, ToOrder>(first, last, out, room))
			{
				return {first, out, unicode_status::complete};
			}
		}
	}
	else if constexpr (sizeof(unicode_cursor_value_t<Input>) == 4 && sizeof(To) == 2)
	{
		auto const extent{static_cast<::std::size_t>(last - first)};
		// A mixed packet may fit exactly even though its worst-case spelling would not.
		if (!::std::is_constant_evaluated() && extent >= 16u && extent < 128u &&
			room > extent && room - extent < extent &&
			utf_narrow_medium<FromOrder, ToOrder>(first, last, out, room))
		{
			return {first, out, unicode_status::complete};
		}
	}
#endif
	while (first != last)
	{
#if defined(FAST_IO_UNICODE_BLOCKS)
		if (!::std::is_constant_evaluated() && unicode_block<FromOrder, ToOrder, Replace>(first, last, out, room))
		{
			continue;
		}
#endif
		auto scalar{unicode_decode<FromOrder>(first, static_cast<::std::size_t>(last - first))};
		if (scalar.status != unicode_status::complete)
		{
			if (!Replace || scalar.status == unicode_status::incomplete)
			{
				return {first, out, scalar.status};
			}
			scalar.value = 0xfffdu;
		}
		auto width{unicode_width<To>(scalar.value)};
		if (width > room)
		{
			return {first, out, unicode_status::output_full};
		}
		unicode_emit<ToOrder>(scalar.value, width, out);
		first += scalar.units;
		out += width;
		room -= width;
	}
	return {first, out, unicode_status::complete};
}
template <encoding_scheme FromOrder, encoding_scheme ToOrder, bool Replace, typename From, typename To>
inline constexpr unicode_progress<From, To> unicode_convert(From const *first, From const *last, To *out,
														   ::std::size_t room) noexcept
{
	if constexpr (unicode_has_bmp_packet && sizeof(From) == 2 && sizeof(To) == 4)
	{
		if (!::std::is_constant_evaluated() && last - first == 64 && room >= 64u &&
			utf_widen_bmp_packet<64, FromOrder, ToOrder>(first, out))
		{
			return {last, out + 64u, unicode_status::complete};
		}
	}
	auto r{unicode_convert_range<FromOrder, ToOrder, Replace>(first, last, out, room)};
	return {r.input, r.output, r.status};
}
} // namespace fast_io::details::codecvt
#undef FAST_IO_UNICODE_BLOCKS
#undef FAST_IO_UNICODE_SHORT_WIDE
#undef FAST_IO_UNICODE_SHORT_INPUT
