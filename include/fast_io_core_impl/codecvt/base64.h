#pragma once

namespace fast_io::transcoders
{
namespace details
{
/** @brief Publishes the wire alphabet as ASCII octets even on hosts with a different execution encoding. */
inline constexpr auto base64_alphabet = [] {
	::fast_io::freestanding::array<char, 64> result{};
	for (unsigned i{}; i != 64; ++i)
	{
		result[i] = static_cast<char>(i < 26u ? 65u + i : i < 52u ? 71u + i : i < 62u ? i - 4u : i == 62u ? 43u : 47u);
	}
	return result;
}();

/** @brief Converts a Base64 alphabet byte to a sextet, using 255 for padding or an invalid symbol. */
inline constexpr auto base64_inverse = [] {
	::fast_io::freestanding::array<unsigned char, 256> table{};
	for (auto &v : table.element) { v = 255; }
	for (unsigned i{}; i != 64; ++i) { table[static_cast<unsigned char>(base64_alphabet[i])] = static_cast<unsigned char>(i); }
	return table;
}();

#if defined(__has_builtin)
#if __has_builtin(__builtin_shufflevector) && __has_builtin(__builtin_bit_cast) && __CHAR_BIT__ == 8 && \
	defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) && \
	defined(__SIZEOF_INT__) && __SIZEOF_INT__ == 4 && (defined(__SSSE3__) || defined(__ARM_NEON) || defined(__wasm_simd128__) || \
	defined(__ALTIVEC__) || defined(__riscv_vector) || defined(__loongarch_sx) || (defined(__s390x__) && defined(__VEC__)))
#define FAST_IO_BASE64_GROUP_SIMD
#if defined(__clang__) && defined(__ALTIVEC__)
using b64_octets __attribute__((ext_vector_type(16))) = unsigned char;
using b64_words __attribute__((ext_vector_type(4))) = unsigned;
#else
using b64_octets [[gnu::vector_size(16)]] = unsigned char;
using b64_words [[gnu::vector_size(16)]] = unsigned;
#endif

/** @brief Places the low byte of each numeric word in the first memory lane of its quartet. */
inline b64_octets base64_word_bytes(b64_octets bytes) noexcept
{
	if constexpr (::std::endian::native == ::std::endian::big)
	{
		return __builtin_shufflevector(bytes, bytes, 3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);
	}
	return bytes;
}

/** @brief Turns twelve source octets into sixteen sextets and maps every lane to the RFC 4648 alphabet. */
inline void base64_encode_four(unsigned char const *src, char *dest) noexcept
{
	b64_octets data{};
	__builtin_memcpy(__builtin_addressof(data), src, 12);
	auto arranged{__builtin_shufflevector(data, data, 2,1,0,12, 5,4,3,12, 8,7,6,12, 11,10,9,12)};
	auto word{__builtin_bit_cast(b64_words, base64_word_bytes(arranged))};
	auto parts{((word >> 18u) & 63u) | ((word >> 4u) & 0x3f00u) | ((word << 10u) & 0x3f0000u) | (word << 24u)};
	auto x{base64_word_bytes(__builtin_bit_cast(b64_octets, parts))};
	x &= 63;
	auto ascii{x + 65 + ((x > 25) & 6) - ((x > 51) & 75) - ((x > 61) & 15) + ((x == 63) & 3)};
	__builtin_memcpy(dest, __builtin_addressof(ascii), 16);
}

/** @brief Validates sixteen unpadded symbols before storing their twelve decoded octets. */
inline bool base64_decode_four(char const *src, unsigned char *dest) noexcept
{
	b64_octets c;
	__builtin_memcpy(__builtin_addressof(c), src, 16);
	auto upper{(c >= 65) & (c <= 90)}, lower{(c >= 97) & (c <= 122)}, digit{(c >= 48) & (c <= 57)};
	auto plus{c == 43}, slash{c == 47};
	auto valid{__builtin_bit_cast(b64_words, upper | lower | digit | plus | slash)};
	if ((valid[0] & valid[1] & valid[2] & valid[3]) != ~0u) { return false; }
	auto sextets{(upper & (c - 65)) | (lower & (c - 71)) | (digit & (c + 4)) | (plus & 62) | (slash & 63)};
	auto s{__builtin_bit_cast(b64_words, base64_word_bytes(sextets))};
	auto word{((s & 63u) << 18u) | ((s & 0x3f00u) << 4u) | ((s >> 10u) & 0xfc0u) | (s >> 24u)};
	auto bytes{base64_word_bytes(__builtin_bit_cast(b64_octets, word))};
	auto packed{__builtin_shufflevector(bytes, bytes, 2,1,0, 6,5,4, 10,9,8, 14,13,12, 0,0,0,0)};
	__builtin_memcpy(dest, __builtin_addressof(packed), 12);
	return true;
}
#endif
#endif
} // namespace details

/**
 * @brief Encodes complete RFC 4648 quanta. Each three input bytes require four writable output bytes.
 * @details The ranges must be disjoint. Padding belongs to the terminal quantum. No terminator is
 * written. Each vector load and store stays inside its declared range, even at a page boundary.
 */
inline constexpr void base64_encode_blocks(unsigned char const *src, ::std::size_t groups, char *dest) noexcept
{
#ifdef FAST_IO_BASE64_GROUP_SIMD
	if (!::std::is_constant_evaluated())
	{
		while (groups >= 4) { details::base64_encode_four(src, dest); src += 12; dest += 16; groups -= 4; }
	}
#endif
	for (; groups != 0; --groups, src += 3, dest += 4)
	{
		unsigned a{src[0] & 255u}, b{src[1] & 255u}, c{src[2] & 255u};
		// Every sextet comes from at most two octets, including on hosts with a 16-bit unsigned int.
		dest[0] = details::base64_alphabet[a >> 2u];
		dest[1] = details::base64_alphabet[((a & 3u) << 4u) | (b >> 4u)];
		dest[2] = details::base64_alphabet[((b & 15u) << 2u) | (c >> 6u)];
		dest[3] = details::base64_alphabet[c & 63u];
	}
}

/**
 * @brief Decodes consecutive complete unpadded RFC 4648 quanta and returns the number consumed.
 * @details Each four source characters require three writable destination bytes in disjoint storage.
 * Padding or an invalid character stops the operation before the affected quartet. The caller can
 * distinguish a legal canonical terminal quartet from malformed data without losing its input cursor.
 */
inline constexpr ::std::size_t base64_decode_blocks(char const *src, ::std::size_t groups, unsigned char *dest) noexcept
{
	::std::size_t done{};
#ifdef FAST_IO_BASE64_GROUP_SIMD
	if (!::std::is_constant_evaluated())
	{
		while (groups - done >= 4 && details::base64_decode_four(src, dest))
		{ src += 16; dest += 12; done += 4; }
	}
#endif
	for (; done < groups; ++done, src += 4, dest += 3)
	{
		if constexpr (::std::numeric_limits<unsigned char>::digits > 8)
		{
			if ((static_cast<unsigned char>(src[0]) | static_cast<unsigned char>(src[1]) |
				 static_cast<unsigned char>(src[2]) | static_cast<unsigned char>(src[3])) > 255u) { break; }
		}
		auto const &lut{details::base64_inverse};
		unsigned a{lut[static_cast<unsigned char>(src[0])]}, b{lut[static_cast<unsigned char>(src[1])]};
		unsigned c{lut[static_cast<unsigned char>(src[2])]}, d{lut[static_cast<unsigned char>(src[3])]};
		if ((a | b | c | d) >= 64u) { break; }
		dest[0] = static_cast<unsigned char>(((a << 2u) | (b >> 4u)) & 255u);
		dest[1] = static_cast<unsigned char>(((b << 4u) | (c >> 2u)) & 255u);
		dest[2] = static_cast<unsigned char>(((c << 6u) | d) & 255u);
	}
	return done;
}
} // namespace fast_io::transcoders

#undef FAST_IO_BASE64_GROUP_SIMD
