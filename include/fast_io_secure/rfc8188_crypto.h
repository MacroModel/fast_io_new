#pragma once

#include "key_material.h"
#include <cstring>

#if defined(__PCLMUL__) && (defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)) && !defined(_M_ARM64EC)
#if defined(_MSC_VER) && !defined(__clang__)
#include <immintrin.h>
#define FAST_IO_GHASH_CLMUL
#elif defined(__has_builtin)
#if defined(__SSE2__) && __has_builtin(__builtin_ia32_pclmulqdq128) && __has_builtin(__builtin_shufflevector)
#define FAST_IO_GHASH_CLMUL
#endif
#endif
#endif

namespace fast_io::secure::details
{
/** @brief HMAC-SHA-256 for the RFC 8188 extract and single-block expand steps. Both keys fit one SHA block. */
inline void record_hmac(::std::byte const *key, ::std::size_t key_length,
	::std::byte const *message, ::std::size_t length, ::std::byte (&digest)[32]) noexcept
{
	sha256_hmac({key,key_length},digest,bytes_view{message,length});
}

/** @brief AES-GCM with a 96-bit nonce, bounded additional data and a full authentication tag. */
class aes128gcm_key
{
	using word = ::std::uint_least64_t;
	::fast_io::aes128 cipher;

#ifdef FAST_IO_GHASH_CLMUL
	using field = ::fast_io::intrinsics::x86_64_m128i;
	static constexpr unsigned group_size{8};
	field powers[group_size];

#if defined(_MSC_VER) && !defined(__clang__)
	static field field_xor(field a, field b) noexcept { return _mm_xor_si128(a, b); }
	static field swap_halves(field a) noexcept { return _mm_shuffle_epi32(a, 0x4e); }
	static field to_high(field a) noexcept { return _mm_slli_si128(a, 8); }
	static field to_low(field a) noexcept { return _mm_srli_si128(a, 8); }

	template <int Select>
	static field carryless_product(field a, field b) noexcept { return _mm_clmulepi64_si128(a, b, Select); }

	static field reflect(field x) noexcept
	{
#if defined(__SSSE3__) || defined(__AVX2__)
		auto table{_mm_setr_epi8(0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15)};
		auto nibbles{_mm_set1_epi8(15)};
		return _mm_or_si128(_mm_slli_epi16(_mm_shuffle_epi8(table, _mm_and_si128(x, nibbles)), 4),
			_mm_shuffle_epi8(table, _mm_and_si128(_mm_srli_epi16(x, 4), nibbles)));
#else
		auto swap = []<int n>(field a, char low) {
			auto m{_mm_set1_epi8(low)};
			return _mm_or_si128(_mm_slli_epi16(_mm_and_si128(a, m), n), _mm_and_si128(_mm_srli_epi16(a, n), m));
		};
		return swap.template operator()<4>(swap.template operator()<2>(swap.template operator()<1>(x, 0x55), 0x33), 0x0f);
#endif
	}
#else
	static field field_xor(field a, field b) noexcept { return a ^ b; }
	static field swap_halves(field a) noexcept { return __builtin_shufflevector(a, a, 1, 0); }
	static field to_high(field a) noexcept { return __builtin_shufflevector(field{}, a, 0, 2); }
	static field to_low(field a) noexcept { return __builtin_shufflevector(a, field{}, 1, 2); }

	template <int Select>
	static field carryless_product(field a, field b) noexcept { return __builtin_ia32_pclmulqdq128(a, b, Select); }

	/** @brief Reflects bits within each octet into polynomial order. The same permutation restores wire order. */
	static field reflect(field x) noexcept
	{
#if defined(__GFNI__) && __has_builtin(__builtin_ia32_vgf2p8affineqb_v16qi)
		using bytes = ::fast_io::intrinsics::x86_64_v16qi;
		// The fixed affine matrix reverses bits; it performs no inversion or secret-indexed lookup.
		constexpr auto reverse_word{::std::bit_cast<long long>(0x8040201008040201ULL)};
		constexpr field reverse{reverse_word, reverse_word};
		auto value{__builtin_bit_cast(bytes, x)};
		auto matrix{__builtin_bit_cast(bytes, reverse)};
		return __builtin_bit_cast(field, __builtin_ia32_vgf2p8affineqb_v16qi(value, matrix, 0));
#elif defined(__SSSE3__) && __has_builtin(__builtin_ia32_pshufb128)
		using bytes = ::fast_io::intrinsics::x86_64_v16qu;
		using shuffle_bytes = ::fast_io::intrinsics::x86_64_v16qi;
		auto v{__builtin_bit_cast(bytes, x)};
		constexpr shuffle_bytes table{0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};
		auto low{__builtin_ia32_pshufb128(table, __builtin_bit_cast(shuffle_bytes, v & 15u))};
		auto high{__builtin_ia32_pshufb128(table, __builtin_bit_cast(shuffle_bytes, v >> 4u))};
		return __builtin_bit_cast(field, (__builtin_bit_cast(bytes, low) << 4u) | __builtin_bit_cast(bytes, high));
#else
		using bytes = ::fast_io::intrinsics::x86_64_v16qu;
		auto v{__builtin_bit_cast(bytes, x)};
		v = ((v & 0x55u) << 1u) | ((v >> 1u) & 0x55u);
		v = ((v & 0x33u) << 2u) | ((v >> 2u) & 0x33u);
		return __builtin_bit_cast(field, (v << 4u) | (v >> 4u));
#endif
	}
#endif

	static field load(unsigned char const *p) noexcept
	{
		field value;
		::std::memcpy(&value, p, 16);
		return reflect(value);
	}

	struct product
	{
		field lo{}, cross{}, hi{};

		void add(field a, field b) noexcept
		{
			lo = field_xor(lo, carryless_product<0>(a, b));
			hi = field_xor(hi, carryless_product<17>(a, b));
			auto ax{field_xor(a, swap_halves(a))};
			auto bx{field_xor(b, swap_halves(b))};
			cross = field_xor(cross, carryless_product<0>(ax, bx));
		}

		field reduce() const noexcept
		{
			auto middle{field_xor(cross, field_xor(lo,hi))};
			auto low{field_xor(lo,to_high(middle))};
			auto high{field_xor(hi,to_low(middle))};
			// x^128 = x^7 + x^2 + x + 1. Fold the seven possible overflow bits a second time.
			constexpr word coefficients[2]{0x87, 0};
			auto polynomial{::std::bit_cast<field>(coefficients)};
			auto a{carryless_product<0>(high, polynomial)};
			auto b{carryless_product<1>(high, polynomial)};
			return field_xor(field_xor(low,a),
				field_xor(to_high(b),carryless_product<1>(b, polynomial)));
		}
	};

	static field multiply(field a, field b) noexcept
	{
		product value;
		value.add(a,b);
		return value.reduce();
	}

	field update(field state, unsigned char const *p) const noexcept { return multiply(field_xor(state,load(p)),powers[0]); }
#else
	// Network-order bit 127 represents the constant coefficient in SP 800-38D Algorithm 1.
	struct field { word high{}, low{}; };
	field h;

	static field load(unsigned char const *p) noexcept
	{
		field value;
		::std::memcpy(&value.high,p,8);
		::std::memcpy(&value.low,p+8,8);
		return {::fast_io::big_endian(value.high), ::fast_io::big_endian(value.low)};
	}
	field update(field state, unsigned char const *p) const noexcept
	{
		auto block{load(p)};
		auto v{h};
		field result{};
		word words[]{state.high ^ block.high, state.low ^ block.low};
		for (auto bits : words)
		{
			for (unsigned i{}; i != 64; ++i, bits <<= 1u)
			{
				auto mask{word{} - (bits >> 63u)};
				auto carry{word{} - (v.low & 1u)};
				// Keep the masks opaque so the compiler cannot replace either XOR with a secret-dependent branch.
#if (defined(__GNUC__) || defined(__clang__)) && defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ >= 8
				__asm__ __volatile__("" : "+r"(mask), "+r"(carry));
#else
				word const volatile kept_mask{mask}, kept_carry{carry};
				mask = kept_mask;
				carry = kept_carry;
#endif
				result.high ^= v.high & mask;
				result.low ^= v.low & mask;
				v.low = (v.low >> 1u) | (v.high << 63u);
				v.high = (v.high >> 1u) ^ (carry & 0xe100000000000000ULL);
			}
		}
		return result;
	}
#endif

public:
	explicit aes128gcm_key(::std::byte const *key) noexcept : cipher(key)
	{
		unsigned char block[16]{};
		auto *bytes{reinterpret_cast<::std::byte *>(block)};
		cipher.encrypt(bytes,1,bytes);
#ifdef FAST_IO_GHASH_CLMUL
		powers[0] = load(block);
		for (unsigned i{1}; i != group_size; ++i) { powers[i] = multiply(powers[i-1],powers[0]); }
#else
		h = load(block);
#endif
		::fast_io::secure_clear_array(block);
	}
	aes128gcm_key(aes128gcm_key const &) = delete;
	aes128gcm_key &operator=(aes128gcm_key const &) = delete;

	~aes128gcm_key()
	{
#ifdef FAST_IO_GHASH_CLMUL
		::fast_io::secure_clear(powers,sizeof(powers));
#else
		::fast_io::secure_clear(&h,sizeof(h));
#endif
	}

	/** @brief Applies CTR to at most 2^32 - 2 payload blocks. Counter 1 masks the tag and payload starts at 2. */
	void crypt(unsigned char const (&nonce)[12], unsigned char const *src, ::std::size_t size, unsigned char *out) const noexcept
	{
		::std::byte counter[16]{};
		::std::memcpy(counter,nonce,12);
		counter[15] = ::std::byte{2};
		if (!size) return;
		auto n{cipher.transform_ctr_bulk(counter,reinterpret_cast<::std::byte const *>(src),reinterpret_cast<::std::byte *>(out),size)};
		src += n;
		out += n;
		size -= n;
		if (size)
		{
			::std::byte stream[256];
			auto index{static_cast<::std::uint_least32_t>(2u + n / 16u)};
			while (size)
			{
				auto batch{size < sizeof(stream) ? size : sizeof(stream)};
				auto blocks{(batch + 15u) / 16u};
				for (::std::size_t i{}; i != blocks; ++i)
				{
					auto wire{::fast_io::big_endian(index++)};
					::std::memcpy(stream + i*16u,nonce,12);
					::std::memcpy(stream + i*16u + 12u,&wire,4);
				}
				cipher.encrypt(stream,blocks,stream);
				for (::std::size_t i{}; i != batch; ++i) { out[i] = src[i] ^ ::std::to_integer<unsigned char>(stream[i]); }
				size -= batch;
				out += batch;
				src += batch;
			}
			::fast_io::secure_clear_array(stream);
		}
	}

	private:
	field hash_padded(field state, unsigned char const *p, ::std::size_t remaining) const noexcept
	{
#ifdef FAST_IO_GHASH_CLMUL
		while (remaining >= 16u * group_size)
		{
			product value;
			value.add(field_xor(state,load(p)),powers[group_size-1]);
			for (unsigned i{1}; i != group_size; ++i) { value.add(load(p + 16u*i),powers[group_size-1u-i]); }
			state = value.reduce();
			p += 16u * group_size;
			remaining -= 16u * group_size;
		}
		// Expanding the GHASH recurrence lets all complete blocks in this short group share one reduction.
		if (remaining >= 32u)
		{
			auto blocks{static_cast<unsigned>(remaining / 16u)};
			product value;
			value.add(field_xor(state, load(p)), powers[blocks - 1]);
			for (unsigned i{1}; i != blocks; ++i)
			{
				value.add(load(p + 16u * i), powers[blocks - 1u - i]);
			}
			state = value.reduce();
			p += 16u * blocks;
			remaining -= 16u * blocks;
		}
#endif
		while (remaining >= 16u)
		{
			state = update(state, p);
			p += 16;
			remaining -= 16;
		}
		if (remaining)
		{
			unsigned char block[16]{};
			::std::memcpy(block,p,remaining);
			state = update(state,block);
		}
		return state;
	}

public:
	void tag(unsigned char const (&nonce)[12], unsigned char const *p, ::std::size_t size, unsigned char *out,
		bytes_view aad = {}) const noexcept
	{
		auto state{hash_padded(field{},reinterpret_cast<unsigned char const *>(aad.data()),aad.size())};
		state = hash_padded(state,p,size);
		unsigned char length_block[16]{};
		auto aad_bits{::fast_io::big_endian(static_cast<word>(aad.size()) * 8u)};
		::std::memcpy(length_block,&aad_bits,8);
		auto bits{::fast_io::big_endian(static_cast<word>(size) * 8u)};
		::std::memcpy(length_block+8,&bits,8);
		state = update(state,length_block);
		unsigned char mask[16]{};
		::std::memcpy(mask,nonce,12);
		mask[15] = 1;
		auto *bytes{reinterpret_cast<::std::byte *>(mask)};
		cipher.encrypt(bytes,1,bytes);
#ifdef FAST_IO_GHASH_CLMUL
		field pad;
		::std::memcpy(&pad,mask,16);
		auto result{field_xor(reflect(state),pad)};
		::std::memcpy(out,&result,16);
#else
		state.high = ::fast_io::big_endian(state.high);
		state.low = ::fast_io::big_endian(state.low);
		::std::memcpy(out,&state.high,8);
		::std::memcpy(out+8,&state.low,8);
		for (unsigned i{}; i != 16; ++i) { out[i] ^= mask[i]; }
#endif
		::fast_io::secure_clear_array(mask);
	}

	static bool equal_tag(unsigned char const (&expected)[16], unsigned char const *received) noexcept
	{
		// Volatile input reads retain the full sixteen-byte comparison, including on an early mismatch.
		unsigned difference{};
		unsigned char const volatile *tag_bytes{received};
		for (unsigned i{}; i != 16; ++i) { difference |= expected[i] ^ tag_bytes[i]; }
		return difference == 0;
	}

	/** @brief Encrypts one bounded message and appends its detached 128-bit authentication tag.
	 * @details The caller enforces GCM length and per-key usage limits and supplies a unique nonce.
	 * Input and output may coincide exactly. The tag destination must follow or be disjoint from the
	 * ciphertext, and additional data must remain readable until authentication completes.
	 */
	void seal(unsigned char const (&nonce)[12], unsigned char const *input, ::std::size_t size,
	          unsigned char *output, unsigned char *output_tag, bytes_view aad = {}) const noexcept
	{
		crypt(nonce, input, size, output);
		tag(nonce, output, size, output_tag, aad);
	}

	/** @brief Checks the full 128-bit tag before decrypting any byte into the destination.
	 * @details The caller supplies bounded input and stable associated data. On failure the entire
	 * destination retains its original contents, including when opening exactly in place.
	 */
	[[nodiscard]] bool try_open(unsigned char const (&nonce)[12], unsigned char const *input, ::std::size_t size,
	                            unsigned char const *input_tag, unsigned char *output, bytes_view aad = {}) const noexcept
	{
		unsigned char expected[16];
		tag(nonce, input, size, expected, aad);
		bool const matches{equal_tag(expected, input_tag)};
		::fast_io::secure_clear_array(expected);
		if (matches)
		{
			crypt(nonce, input, size, output);
		}
		return matches;
	}
};
} // namespace fast_io::secure::details

#undef FAST_IO_GHASH_CLMUL
