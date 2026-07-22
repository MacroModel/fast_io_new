#pragma once

#include "../../fast_io_core.h"

namespace fast_io::json::details
{

enum class unicode_decode_status : unsigned char
{
	ok,
	end,
	invalid
};

struct unicode_decode_result
{
	::std::uint_least32_t code_point{};
	::std::size_t next{};
	unicode_decode_status status{unicode_decode_status::end};
};

[[nodiscard]] inline constexpr bool json_unicode_scalar(::std::uint_least32_t code_point) noexcept
{
	return code_point <= 0x10ffffu && !(0xd800u <= code_point && code_point <= 0xdfffu);
}

template <::std::integral char_type>
	requires(!::std::same_as<::std::remove_cv_t<char_type>, bool>)
[[nodiscard]] inline constexpr ::std::uint_least32_t json_code_unit(char_type value) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<::std::remove_cv_t<char_type>>;
	return static_cast<::std::uint_least32_t>(static_cast<unsigned_char_type>(value));
}

/*
Decode exactly one Unicode scalar from a contiguous string.

For the one-byte case, the leading-byte ranges select a unique UTF-8 length.
The lower bounds 0x80, 0x800 and 0x10000 reject every overlong encoding; the
upper scalar bound and surrogate exclusion reject precisely the non-scalar
UTF-8 sequences.  UTF-16 combines a high/low surrogate pair as
0x10000 + (high-0xd800)*1024 + (low-0xdc00), a bijection onto
[0x10000,0x10ffff].  Consequently every successful result is one Unicode
scalar and advances by exactly the number of source code units that encode it.
On failure `next` remains at `position`, so a caller never commits a partial
code point.
*/
template <::std::integral char_type>
	requires(!::std::same_as<::std::remove_cv_t<char_type>, bool>)
[[nodiscard]] inline constexpr unicode_decode_result decode_json_code_point(
	char_type const *data, ::std::size_t size, ::std::size_t position) noexcept
{
	if (position == size)
	{
		return {0u, position, unicode_decode_status::end};
	}
	if (size < position)
	{
		return {0u, position, unicode_decode_status::invalid};
	}

	if constexpr (sizeof(char_type) == 1u)
	{
		auto const first{json_code_unit(data[position])};
		if (first <= 0x7fu)
		{
			return {first, position + 1u, unicode_decode_status::ok};
		}

		::std::size_t length{};
		::std::uint_least32_t code_point{};
		::std::uint_least32_t minimum{};
		if (0xc2u <= first && first <= 0xdfu)
		{
			length = 2u;
			code_point = first & 0x1fu;
			minimum = 0x80u;
		}
		else if (0xe0u <= first && first <= 0xefu)
		{
			length = 3u;
			code_point = first & 0x0fu;
			minimum = 0x800u;
		}
		else if (0xf0u <= first && first <= 0xf4u)
		{
			length = 4u;
			code_point = first & 0x07u;
			minimum = 0x10000u;
		}
		else
		{
			return {0u, position, unicode_decode_status::invalid};
		}

		if (size - position < length)
		{
			return {0u, position, unicode_decode_status::invalid};
		}
		for (::std::size_t index{1u}; index != length; ++index)
		{
			auto const continuation{json_code_unit(data[position + index])};
			if (continuation < 0x80u || 0xbfu < continuation)
			{
				return {0u, position, unicode_decode_status::invalid};
			}
			code_point = static_cast<::std::uint_least32_t>((code_point << 6u) | (continuation & 0x3fu));
		}
		if (code_point < minimum || !json_unicode_scalar(code_point))
		{
			return {0u, position, unicode_decode_status::invalid};
		}
		return {code_point, position + length, unicode_decode_status::ok};
	}
	else if constexpr (sizeof(char_type) == 2u)
	{
		auto const first{json_code_unit(data[position])};
		if (first < 0xd800u || 0xdfffu < first)
		{
			return {first, position + 1u, unicode_decode_status::ok};
		}
		if (0xdbffu < first || size - position < 2u)
		{
			return {0u, position, unicode_decode_status::invalid};
		}
		auto const second{json_code_unit(data[position + 1u])};
		if (second < 0xdc00u || 0xdfffu < second)
		{
			return {0u, position, unicode_decode_status::invalid};
		}
		auto const code_point{static_cast<::std::uint_least32_t>(
			0x10000u + ((first - 0xd800u) << 10u) + (second - 0xdc00u))};
		return {code_point, position + 2u, unicode_decode_status::ok};
	}
	else
	{
		auto const code_point{json_code_unit(data[position])};
		if (!json_unicode_scalar(code_point))
		{
			return {0u, position, unicode_decode_status::invalid};
		}
		return {code_point, position + 1u, unicode_decode_status::ok};
	}
}

template <::std::integral char_type>
struct basic_json_escape_buffer
{
	// Two UTF-16 JSON escapes ("\\uXXXX\\uXXXX") are the longest spelling.
	char_type buffer[12u]{};
	::std::size_t size{};
};

template <char8_t ch, ::std::integral char_type>
inline constexpr void json_escape_push_literal(basic_json_escape_buffer<char_type> &result) noexcept
{
	result.buffer[result.size++] = ::fast_io::char_literal_v<ch, char_type>;
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr char_type json_escape_hex_digit(unsigned value) noexcept
{
	constexpr char_type digits[]{
		::fast_io::char_literal_v<u8'0', char_type>, ::fast_io::char_literal_v<u8'1', char_type>,
		::fast_io::char_literal_v<u8'2', char_type>, ::fast_io::char_literal_v<u8'3', char_type>,
		::fast_io::char_literal_v<u8'4', char_type>, ::fast_io::char_literal_v<u8'5', char_type>,
		::fast_io::char_literal_v<u8'6', char_type>, ::fast_io::char_literal_v<u8'7', char_type>,
		::fast_io::char_literal_v<u8'8', char_type>, ::fast_io::char_literal_v<u8'9', char_type>,
		::fast_io::char_literal_v<u8'a', char_type>, ::fast_io::char_literal_v<u8'b', char_type>,
		::fast_io::char_literal_v<u8'c', char_type>, ::fast_io::char_literal_v<u8'd', char_type>,
		::fast_io::char_literal_v<u8'e', char_type>, ::fast_io::char_literal_v<u8'f', char_type>};
	return digits[value & 0x0fu];
}

template <::std::integral char_type>
inline constexpr void json_escape_push_u16(
	basic_json_escape_buffer<char_type> &result, ::std::uint_least32_t value) noexcept
{
	json_escape_push_literal<u8'\\'>(result);
	json_escape_push_literal<u8'u'>(result);
	result.buffer[result.size++] = json_escape_hex_digit<char_type>(static_cast<unsigned>(value >> 12u));
	result.buffer[result.size++] = json_escape_hex_digit<char_type>(static_cast<unsigned>(value >> 8u));
	result.buffer[result.size++] = json_escape_hex_digit<char_type>(static_cast<unsigned>(value >> 4u));
	result.buffer[result.size++] = json_escape_hex_digit<char_type>(static_cast<unsigned>(value));
}

template <::std::integral char_type>
inline constexpr void json_escape_push_short(
	basic_json_escape_buffer<char_type> &result, char8_t suffix) noexcept
{
	json_escape_push_literal<u8'\\'>(result);
	switch (suffix)
	{
	case u8'"':
		json_escape_push_literal<u8'"'>(result);
		break;
	case u8'\\':
		json_escape_push_literal<u8'\\'>(result);
		break;
	case u8'/':
		json_escape_push_literal<u8'/'>(result);
		break;
	case u8'b':
		json_escape_push_literal<u8'b'>(result);
		break;
	case u8'f':
		json_escape_push_literal<u8'f'>(result);
		break;
	case u8'n':
		json_escape_push_literal<u8'n'>(result);
		break;
	case u8'r':
		json_escape_push_literal<u8'r'>(result);
		break;
	default:
		json_escape_push_literal<u8't'>(result);
		break;
	}
}

template <::std::integral char_type>
inline constexpr void json_encode_scalar(
	basic_json_escape_buffer<char_type> &result, ::std::uint_least32_t code_point) noexcept
{
	if constexpr (sizeof(char_type) == 1u)
	{
		if (code_point <= 0x7fu)
		{
			result.buffer[result.size++] = static_cast<char_type>(code_point);
		}
		else if (code_point <= 0x7ffu)
		{
			result.buffer[result.size++] = static_cast<char_type>(0xc0u | (code_point >> 6u));
			result.buffer[result.size++] = static_cast<char_type>(0x80u | (code_point & 0x3fu));
		}
		else if (code_point <= 0xffffu)
		{
			result.buffer[result.size++] = static_cast<char_type>(0xe0u | (code_point >> 12u));
			result.buffer[result.size++] = static_cast<char_type>(0x80u | ((code_point >> 6u) & 0x3fu));
			result.buffer[result.size++] = static_cast<char_type>(0x80u | (code_point & 0x3fu));
		}
		else
		{
			result.buffer[result.size++] = static_cast<char_type>(0xf0u | (code_point >> 18u));
			result.buffer[result.size++] = static_cast<char_type>(0x80u | ((code_point >> 12u) & 0x3fu));
			result.buffer[result.size++] = static_cast<char_type>(0x80u | ((code_point >> 6u) & 0x3fu));
			result.buffer[result.size++] = static_cast<char_type>(0x80u | (code_point & 0x3fu));
		}
	}
	else if constexpr (sizeof(char_type) == 2u)
	{
		if (code_point <= 0xffffu)
		{
			result.buffer[result.size++] = static_cast<char_type>(code_point);
		}
		else
		{
			code_point -= 0x10000u;
			result.buffer[result.size++] = static_cast<char_type>(0xd800u + (code_point >> 10u));
			result.buffer[result.size++] = static_cast<char_type>(0xdc00u + (code_point & 0x3ffu));
		}
	}
	else
	{
		result.buffer[result.size++] = static_cast<char_type>(code_point);
	}
}

/*
Forms the complete output for one already-decoded scalar.  JSON escapes are
ASCII grammar and therefore use char_literal_v for every syntax code unit.
When no escape is required, json_encode_scalar emits the unique UTF-8,
UTF-16, or UTF-32 representation selected by the width of the destination
character type.  The twelve-element result is sufficient because the largest
case is a supplementary scalar in ASCII-only mode: two six-unit U+XXXX escapes.
*/
template <::std::integral char_type>
[[nodiscard]] inline constexpr bool escape_json_code_point(basic_json_escape_buffer<char_type> &result,
														   ::std::uint_least32_t code_point, bool ascii_only, bool javascript_safe, bool escape_solidus) noexcept
{
	result.size = 0u;
	if (!json_unicode_scalar(code_point))
	{
		return false;
	}

	switch (code_point)
	{
	case 0x22u:
		json_escape_push_short(result, u8'"');
		return true;
	case 0x5cu:
		json_escape_push_short(result, u8'\\');
		return true;
	case 0x2fu:
		if (escape_solidus)
		{
			json_escape_push_short(result, u8'/');
			return true;
		}
		break;
	case 0x08u:
		json_escape_push_short(result, u8'b');
		return true;
	case 0x0cu:
		json_escape_push_short(result, u8'f');
		return true;
	case 0x0au:
		json_escape_push_short(result, u8'n');
		return true;
	case 0x0du:
		json_escape_push_short(result, u8'r');
		return true;
	case 0x09u:
		json_escape_push_short(result, u8't');
		return true;
	default:
		break;
	}

	if (code_point < 0x20u || (javascript_safe && (code_point == 0x2028u || code_point == 0x2029u)) ||
		(ascii_only && 0x7fu < code_point))
	{
		if (code_point <= 0xffffu)
		{
			json_escape_push_u16(result, code_point);
		}
		else
		{
			auto const supplementary{code_point - 0x10000u};
			json_escape_push_u16(result, 0xd800u + (supplementary >> 10u));
			json_escape_push_u16(result, 0xdc00u + (supplementary & 0x3ffu));
		}
		return true;
	}

	json_encode_scalar(result, code_point);
	return true;
}

} // namespace fast_io::json::details
