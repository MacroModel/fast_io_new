#pragma once

namespace fast_io::details::aes
{

using x86_aes_block [[gnu::vector_size(16)]] = long long;

/**
 * @brief Computes the FIPS 197 SubWord operation with one AES last-round instruction.
 * @details Identical columns make ShiftRows an identity. With a zero round key,
 * AESENCLAST substitutes each byte without a secret-indexed memory access.
 */
inline ::std::uint_least32_t x86_aes_sub_word(::std::uint_least32_t word) noexcept
{
	auto repeated{static_cast<::std::uint_least64_t>(word) | (static_cast<::std::uint_least64_t>(word) << 32u)};
	auto lane{static_cast<long long>(repeated)};
	auto transformed{__builtin_ia32_aesenclast128(x86_aes_block{lane, lane}, x86_aes_block{})};
	return static_cast<::std::uint_least32_t>(transformed[0]);
}

/** @brief Loads a complete unaligned block without imposing a vector type or alignment on caller storage. */
inline x86_aes_block load_x86_aes_block(void const *p) noexcept
{
	x86_aes_block value;
	__builtin_memcpy(__builtin_addressof(value), p, sizeof(value));
	return value;
}

/**
 * @brief Transforms independent AES blocks using the canonical FIPS-197 key schedule.
 * @details Interleaving independent block states lets the core issue rounds before the preceding
 * block's round completes. The inverse cipher applies InvMixColumns only to the intermediate round keys.
 * All input lanes are loaded before storing results, including the exact in-place case.
 */
template <::std::size_t rounds, bool inverse, ::std::size_t lanes>
inline void x86_aes_group(::std::uint_least8_t const *keys, ::std::byte const *from, ::std::byte *to) noexcept
{
	x86_aes_block states[lanes];
	auto const first{load_x86_aes_block(keys + (inverse ? rounds : 0u) * 16u)};
	for (::std::size_t i{}; i != lanes; ++i)
	{
		states[i] = load_x86_aes_block(from + 16u * i) ^ first;
	}
	for (::std::size_t r{1u}; r != rounds; ++r)
	{
		auto key{load_x86_aes_block(keys + (inverse ? rounds - r : r) * 16u)};
		if constexpr (inverse)
		{
			key = __builtin_ia32_aesimc128(key);
		}
		for (auto &state : states)
		{
			if constexpr (inverse)
			{
				state = __builtin_ia32_aesdec128(state, key);
			}
			else
			{
				state = __builtin_ia32_aesenc128(state, key);
			}
		}
	}
	auto const last{load_x86_aes_block(keys + (inverse ? 0u : rounds) * 16u)};
	for (::std::size_t i{}; i != lanes; ++i)
	{
		if constexpr (inverse)
		{
			states[i] = __builtin_ia32_aesdeclast128(states[i], last);
		}
		else
		{
			states[i] = __builtin_ia32_aesenclast128(states[i], last);
		}
		__builtin_memcpy(to + 16u * i, __builtin_addressof(states[i]), sizeof(states[i]));
	}
}

#if defined(__VAES__) && defined(__AVX2__) && __has_builtin(__builtin_shufflevector) && __has_builtin(__builtin_bit_cast)
#if __has_builtin(__builtin_ia32_aesenc256) && __has_builtin(__builtin_ia32_aesenclast256) && \
	__has_builtin(__builtin_ia32_aesdec256) && __has_builtin(__builtin_ia32_aesdeclast256)
#define FAST_IO_AES_WIDE_CLANG
#elif __has_builtin(__builtin_ia32_vaesenc_v32qi) && __has_builtin(__builtin_ia32_vaesenclast_v32qi) && \
	__has_builtin(__builtin_ia32_vaesdec_v32qi) && __has_builtin(__builtin_ia32_vaesdeclast_v32qi)
#define FAST_IO_AES_WIDE_GNU
#endif
#endif

#if defined(FAST_IO_AES_WIDE_CLANG) || defined(FAST_IO_AES_WIDE_GNU)
using x86_aes_pair [[gnu::vector_size(32)]] = long long;

/** @brief Duplicates one round key for the two independent lanes of a VAES register. */
inline x86_aes_pair expand_x86_aes_key(x86_aes_block key) noexcept
{
	return __builtin_shufflevector(key, key, 0, 1, 0, 1);
}

/** @brief Applies the selected AES round to each 128-bit half of a 256-bit register. */
template <bool inverse, bool last>
inline x86_aes_pair x86_aes_pair_round(x86_aes_pair state, x86_aes_pair key) noexcept
{
#if defined(FAST_IO_AES_WIDE_CLANG)
	if constexpr (inverse && last) { return __builtin_ia32_aesdeclast256(state, key); }
	else if constexpr (inverse) { return __builtin_ia32_aesdec256(state, key); }
	else if constexpr (last) { return __builtin_ia32_aesenclast256(state, key); }
	else { return __builtin_ia32_aesenc256(state, key); }
#else
	using bytes [[gnu::vector_size(32)]] = char;
	auto s{__builtin_bit_cast(bytes, state)}, k{__builtin_bit_cast(bytes, key)};
	if constexpr (inverse && last) { return __builtin_bit_cast(x86_aes_pair, __builtin_ia32_vaesdeclast_v32qi(s, k)); }
	else if constexpr (inverse) { return __builtin_bit_cast(x86_aes_pair, __builtin_ia32_vaesdec_v32qi(s, k)); }
	else if constexpr (last) { return __builtin_bit_cast(x86_aes_pair, __builtin_ia32_vaesenclast_v32qi(s, k)); }
	else { return __builtin_bit_cast(x86_aes_pair, __builtin_ia32_vaesenc_v32qi(s, k)); }
#endif
}

/** @brief Interleaves eight independent vector dependencies, broadcasting each round key once per sixteen blocks. */
template <::std::size_t rounds, bool inverse>
inline void x86_aes_wide_group(::std::uint_least8_t const *keys, ::std::byte const *from, ::std::byte *to) noexcept
{
	x86_aes_pair states[8];
	auto initial{expand_x86_aes_key(load_x86_aes_block(keys + (inverse ? rounds : 0u) * 16u))};
	for (::std::size_t i{}; i != 8; ++i)
	{
		__builtin_memcpy(__builtin_addressof(states[i]), from + 32u * i, 32);
		states[i] ^= initial;
	}
	for (::std::size_t r{1}; r != rounds; ++r)
	{
		auto k{load_x86_aes_block(keys + (inverse ? rounds - r : r) * 16u)};
		if constexpr (inverse) { k = __builtin_ia32_aesimc128(k); }
		auto key{expand_x86_aes_key(k)};
		for (auto &state : states) { state = x86_aes_pair_round<inverse, false>(state, key); }
	}
	auto last{expand_x86_aes_key(load_x86_aes_block(keys + (inverse ? 0u : rounds) * 16u))};
	for (::std::size_t i{}; i != 8; ++i)
	{
		auto value{x86_aes_pair_round<inverse, true>(states[i], last)};
		__builtin_memcpy(to + 32u * i, __builtin_addressof(value), 32);
	}
}
#endif

/**
 * @brief Encrypts and XORs complete sixteen-block CTR groups without a keystream buffer.
 * @details The public counter's low 64 bits must hold the complete run without carrying.
 * A declined run keeps the source, destination and counter intact for the portable path.
 * The last AES instruction feeds the XOR directly. Secret bytes never select an address.
 */
template <::std::size_t rounds>
inline ::std::size_t x86_aes_ctr_bulk(::std::uint_least8_t const *keys, ::std::byte *counter,
	::std::byte const *source, ::std::byte *destination, ::std::size_t size) noexcept
{
#if defined(FAST_IO_AES_WIDE_CLANG) || defined(FAST_IO_AES_WIDE_GNU)
	using lanes [[gnu::vector_size(32)]] = unsigned long long;
	using bytes [[gnu::vector_size(32)]] = unsigned char;
	::std::uint_least64_t words[2];
	__builtin_memcpy(words, counter, 16);
	auto low{::fast_io::big_endian(words[1])};
	auto blocks{(size / 256u) * 16u};
	if (blocks > ::std::numeric_limits<::std::uint_least64_t>::max() - low)
	{
		return 0;
	}
	auto initial{expand_x86_aes_key(load_x86_aes_block(keys))};
	auto last{expand_x86_aes_key(load_x86_aes_block(keys + rounds * 16u))};
	for (auto end{low + blocks}; low != end; low += 16u, source += 256u, destination += 256u)
	{
		x86_aes_pair states[8];
		lanes origin{words[0], low, words[0], low};
		for (unsigned i{}; i != 8u; ++i)
		{
			// Form consecutive counters and serialize their low words in network order.
			auto raw{__builtin_bit_cast(bytes, origin + lanes{0u, 2u * i, 0u, 2u * i + 1u})};
			auto wire{__builtin_shufflevector(raw, raw, 0, 1, 2, 3, 4, 5, 6, 7,
				15, 14, 13, 12, 11, 10, 9, 8, 16, 17, 18, 19, 20, 21, 22, 23,
				31, 30, 29, 28, 27, 26, 25, 24)};
			states[i] = __builtin_bit_cast(x86_aes_pair, wire) ^ initial;
		}
		for (::std::size_t r{1}; r != rounds; ++r)
		{
			auto key{expand_x86_aes_key(load_x86_aes_block(keys + r * 16u))};
			for (auto &state : states) { state = x86_aes_pair_round<false, false>(state, key); }
		}
		for (unsigned i{}; i != 8u; ++i)
		{
			x86_aes_pair input;
			__builtin_memcpy(__builtin_addressof(input), source + 32u * i, 32);
			auto value{x86_aes_pair_round<false, true>(states[i], last) ^ input};
			__builtin_memcpy(destination + 32u * i, __builtin_addressof(value), 32);
		}
	}
	auto next{::fast_io::big_endian(low)};
	__builtin_memcpy(counter + 8, __builtin_addressof(next), 8);
	return blocks * 16u;
#else
	(void)keys;
	(void)counter;
	(void)source;
	(void)destination;
	(void)size;
	return 0;
#endif
}

/** @brief Handles arbitrary block counts, including zero and a final partial group of independent blocks. */
template <::std::size_t rounds, bool inverse>
inline void x86_aes_blocks(::std::uint_least8_t const *keys, ::std::byte const *from, ::std::size_t blocks, ::std::byte *to) noexcept
{
#if defined(FAST_IO_AES_WIDE_CLANG) || defined(FAST_IO_AES_WIDE_GNU)
	while (blocks >= 16u)
	{
		x86_aes_wide_group<rounds, inverse>(keys, from, to);
		from += 256u;
		to += 256u;
		blocks -= 16u;
	}
#endif
	while (blocks >= 4u)
	{
		x86_aes_group<rounds, inverse, 4u>(keys, from, to);
		from += 64u;
		to += 64u;
		blocks -= 4u;
	}
	while (blocks != 0u)
	{
		x86_aes_group<rounds, inverse, 1u>(keys, from, to);
		from += 16u;
		to += 16u;
		--blocks;
	}
}

} // namespace fast_io::details::aes

#undef FAST_IO_AES_WIDE_CLANG
#undef FAST_IO_AES_WIDE_GNU
