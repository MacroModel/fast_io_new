#pragma once
#include "aes_sbox.h"

namespace fast_io::details::aes
{

/* rcon[0] is unused; index is the key-expansion round number */
inline constexpr ::std::uint_least8_t rcon[11]{
	0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36};

/** @brief Multiplies an AES byte by X modulo the reduction polynomial (x^8+x^4+x^3+x+1). */
inline constexpr ::std::uint_least8_t xtime(::std::uint_least8_t x) noexcept
{
	unsigned value{x};
	// Keep the reduction in the native unsigned word before narrowing to an octet.
	return static_cast<::std::uint_least8_t>((value << 1u) ^ (((value & 0x80u) * 0x1bu) >> 7u));
}

/* load/store a 32-bit AES word: byte 0 of the group is the MSB */
inline constexpr ::std::uint_least32_t load_be32(::std::byte const *p) noexcept
{
	FAST_IO_IF_CONSTEVAL
	{
		return (static_cast<::std::uint_least32_t>(::std::to_integer<::std::uint_least8_t>(p[0])) << 24) |
			   (static_cast<::std::uint_least32_t>(::std::to_integer<::std::uint_least8_t>(p[1])) << 16) |
			   (static_cast<::std::uint_least32_t>(::std::to_integer<::std::uint_least8_t>(p[2])) << 8) |
			   static_cast<::std::uint_least32_t>(::std::to_integer<::std::uint_least8_t>(p[3]));
	}
	else
	{
		::std::uint_least32_t v;
		::fast_io::details::my_memcpy(__builtin_addressof(v), p, 4);
		return ::fast_io::big_endian(v);
	}
}

inline constexpr void store_be32(::std::uint_least8_t *p, ::std::uint_least32_t v) noexcept
{
	FAST_IO_IF_CONSTEVAL
	{
		p[0] = static_cast<::std::uint_least8_t>(v >> 24);
		p[1] = static_cast<::std::uint_least8_t>(v >> 16);
		p[2] = static_cast<::std::uint_least8_t>(v >> 8);
		p[3] = static_cast<::std::uint_least8_t>(v);
	}
	else
	{
		v = ::fast_io::big_endian(v);
		::fast_io::details::my_memcpy(p, __builtin_addressof(v), 4);
	}
}

/* state <-> byte block transfer */
inline constexpr void load_block(::std::uint_least8_t *state, ::std::byte const *in) noexcept
{
	FAST_IO_IF_CONSTEVAL
	{
		state[0] = ::std::to_integer<::std::uint_least8_t>(in[0]);
		state[1] = ::std::to_integer<::std::uint_least8_t>(in[1]);
		state[2] = ::std::to_integer<::std::uint_least8_t>(in[2]);
		state[3] = ::std::to_integer<::std::uint_least8_t>(in[3]);
		state[4] = ::std::to_integer<::std::uint_least8_t>(in[4]);
		state[5] = ::std::to_integer<::std::uint_least8_t>(in[5]);
		state[6] = ::std::to_integer<::std::uint_least8_t>(in[6]);
		state[7] = ::std::to_integer<::std::uint_least8_t>(in[7]);
		state[8] = ::std::to_integer<::std::uint_least8_t>(in[8]);
		state[9] = ::std::to_integer<::std::uint_least8_t>(in[9]);
		state[10] = ::std::to_integer<::std::uint_least8_t>(in[10]);
		state[11] = ::std::to_integer<::std::uint_least8_t>(in[11]);
		state[12] = ::std::to_integer<::std::uint_least8_t>(in[12]);
		state[13] = ::std::to_integer<::std::uint_least8_t>(in[13]);
		state[14] = ::std::to_integer<::std::uint_least8_t>(in[14]);
		state[15] = ::std::to_integer<::std::uint_least8_t>(in[15]);
	}
	else
	{
		::fast_io::details::my_memcpy(state, in, 16);
	}
}

inline constexpr void store_block(::std::byte *out, ::std::uint_least8_t const *state) noexcept
{
	FAST_IO_IF_CONSTEVAL
	{
		out[0] = ::std::byte{state[0]};
		out[1] = ::std::byte{state[1]};
		out[2] = ::std::byte{state[2]};
		out[3] = ::std::byte{state[3]};
		out[4] = ::std::byte{state[4]};
		out[5] = ::std::byte{state[5]};
		out[6] = ::std::byte{state[6]};
		out[7] = ::std::byte{state[7]};
		out[8] = ::std::byte{state[8]};
		out[9] = ::std::byte{state[9]};
		out[10] = ::std::byte{state[10]};
		out[11] = ::std::byte{state[11]};
		out[12] = ::std::byte{state[12]};
		out[13] = ::std::byte{state[13]};
		out[14] = ::std::byte{state[14]};
		out[15] = ::std::byte{state[15]};
	}
	else
	{
		::fast_io::details::my_memcpy(out, state, 16);
	}
}

/** @brief Substitutes the four bytes of a key word through the same table-free S-box. */
inline constexpr ::std::uint_least32_t sub_word(::std::uint_least32_t x) noexcept
{
	::std::uint_least32_t planes[8]{x, x, x, x, x, x, x, x};
	transpose_substitution(planes);
	substitute_planes(planes);
	transpose_substitution(planes);
	return planes[0];
}

/*
Expand key into round_keys[(rounds + 1) * 16].
nk (key words) selects the algorithm: 4 = AES-128, 6 = AES-192, 8 = AES-256.
rounds = nk + 6 is a compile-time constant, not a parameter.
Round keys are stored as plain bytes in state order:
round_keys[16*r + c*4 + row] hits state column c, row r.

The expansion runs one group of nk words per iteration with the
per-word code manually unrolled, so there are no position checks
inside the loop. The final group may write a few words past
total_words (nk > 4); w has nk words of slack and the extra words
are never stored.
*/
template <::std::size_t nk, auto Substitute = sub_word>
inline constexpr void key_expansion(::std::byte const *key, ::std::uint_least8_t *round_keys) noexcept
{
	static_assert(nk == 4 || nk == 6 || nk == 8);
	constexpr ::std::size_t rounds{nk + 6};
	constexpr ::std::size_t total_words{4 * (rounds + 1)};
	::std::uint_least32_t w[total_words + nk]{};
	for (::std::size_t i{}; i != nk; ++i)
	{
		w[i] = load_be32(key + i * 4);
	}
	::std::size_t rcon_i{1};
	for (::std::size_t base{nk}; base < total_words; base += nk)
	{
		/* first word of each group: SubWord(RotWord(w)) ^ Rcon.
		canonical word: byte0 in bits 31..24 -> RotWord is rotl by 8 */
		w[base] = w[base - nk] ^
				  (Substitute(::std::rotl(w[base - 1], 8)) ^
				   (static_cast<::std::uint_least32_t>(rcon[rcon_i]) << 24));
		++rcon_i;
		w[base + 1] = w[base + 1 - nk] ^ w[base];
		w[base + 2] = w[base + 2 - nk] ^ w[base + 1];
		w[base + 3] = w[base + 3 - nk] ^ w[base + 2];
		if constexpr (nk == 6)
		{
			w[base + 4] = w[base - 2] ^ w[base + 3];
			w[base + 5] = w[base - 1] ^ w[base + 4];
		}
		else if constexpr (nk == 8)
		{
			w[base + 4] = w[base - 4] ^ Substitute(w[base + 3]);
			w[base + 5] = w[base - 3] ^ w[base + 4];
			w[base + 6] = w[base - 2] ^ w[base + 5];
			w[base + 7] = w[base - 1] ^ w[base + 6];
		}
	}
	for (::std::size_t i{}; i != total_words; ++i)
	{
		store_be32(round_keys + i * 4, w[i]);
	}
	::fast_io::secure_clear_array(w);
}

inline constexpr void add_round_key(::std::uint_least8_t *state, ::std::uint_least8_t const *rk) noexcept
{
	state[0] ^= rk[0];
	state[1] ^= rk[1];
	state[2] ^= rk[2];
	state[3] ^= rk[3];
	state[4] ^= rk[4];
	state[5] ^= rk[5];
	state[6] ^= rk[6];
	state[7] ^= rk[7];
	state[8] ^= rk[8];
	state[9] ^= rk[9];
	state[10] ^= rk[10];
	state[11] ^= rk[11];
	state[12] ^= rk[12];
	state[13] ^= rk[13];
	state[14] ^= rk[14];
	state[15] ^= rk[15];
}

/** @brief Performs SubBytes without indexing memory by a state byte. */
inline constexpr void sub_bytes(::std::uint_least8_t *state) noexcept
{
	substitute_block<false>(state);
}

/** @brief Performs the inverse SubBytes through the same fixed Boolean network. */
inline constexpr void inv_sub_bytes(::std::uint_least8_t *state) noexcept
{
	substitute_block<true>(state);
}

/* state[c*4 + r]: row r rotates left by r positions */
inline constexpr void shift_rows(::std::uint_least8_t *s) noexcept
{
	::std::uint_least8_t t;
	t = s[1];
	s[1] = s[5];
	s[5] = s[9];
	s[9] = s[13];
	s[13] = t;
	t = s[2];
	s[2] = s[10];
	s[10] = t;
	t = s[6];
	s[6] = s[14];
	s[14] = t;
	t = s[15];
	s[15] = s[11];
	s[11] = s[7];
	s[7] = s[3];
	s[3] = t;
}

inline constexpr void inv_shift_rows(::std::uint_least8_t *s) noexcept
{
	::std::uint_least8_t t;
	t = s[13];
	s[13] = s[9];
	s[9] = s[5];
	s[5] = s[1];
	s[1] = t;
	t = s[2];
	s[2] = s[10];
	s[10] = t;
	t = s[6];
	s[6] = s[14];
	s[14] = t;
	t = s[3];
	s[3] = s[7];
	s[7] = s[11];
	s[11] = s[15];
	s[15] = t;
}

inline constexpr void mix_columns(::std::uint_least8_t *s) noexcept
{
	::std::uint_least8_t a0, a1, a2, a3;
	a0 = s[0];
	a1 = s[1];
	a2 = s[2];
	a3 = s[3];
	s[0] = xtime(a0) ^ static_cast<::std::uint_least8_t>(xtime(a1) ^ a1) ^ a2 ^ a3;
	s[1] = a0 ^ xtime(a1) ^ static_cast<::std::uint_least8_t>(xtime(a2) ^ a2) ^ a3;
	s[2] = a0 ^ a1 ^ xtime(a2) ^ static_cast<::std::uint_least8_t>(xtime(a3) ^ a3);
	s[3] = static_cast<::std::uint_least8_t>(xtime(a0) ^ a0) ^ a1 ^ a2 ^ xtime(a3);
	a0 = s[4];
	a1 = s[5];
	a2 = s[6];
	a3 = s[7];
	s[4] = xtime(a0) ^ static_cast<::std::uint_least8_t>(xtime(a1) ^ a1) ^ a2 ^ a3;
	s[5] = a0 ^ xtime(a1) ^ static_cast<::std::uint_least8_t>(xtime(a2) ^ a2) ^ a3;
	s[6] = a0 ^ a1 ^ xtime(a2) ^ static_cast<::std::uint_least8_t>(xtime(a3) ^ a3);
	s[7] = static_cast<::std::uint_least8_t>(xtime(a0) ^ a0) ^ a1 ^ a2 ^ xtime(a3);
	a0 = s[8];
	a1 = s[9];
	a2 = s[10];
	a3 = s[11];
	s[8] = xtime(a0) ^ static_cast<::std::uint_least8_t>(xtime(a1) ^ a1) ^ a2 ^ a3;
	s[9] = a0 ^ xtime(a1) ^ static_cast<::std::uint_least8_t>(xtime(a2) ^ a2) ^ a3;
	s[10] = a0 ^ a1 ^ xtime(a2) ^ static_cast<::std::uint_least8_t>(xtime(a3) ^ a3);
	s[11] = static_cast<::std::uint_least8_t>(xtime(a0) ^ a0) ^ a1 ^ a2 ^ xtime(a3);
	a0 = s[12];
	a1 = s[13];
	a2 = s[14];
	a3 = s[15];
	s[12] = xtime(a0) ^ static_cast<::std::uint_least8_t>(xtime(a1) ^ a1) ^ a2 ^ a3;
	s[13] = a0 ^ xtime(a1) ^ static_cast<::std::uint_least8_t>(xtime(a2) ^ a2) ^ a3;
	s[14] = a0 ^ a1 ^ xtime(a2) ^ static_cast<::std::uint_least8_t>(xtime(a3) ^ a3);
	s[15] = static_cast<::std::uint_least8_t>(xtime(a0) ^ a0) ^ a1 ^ a2 ^ xtime(a3);
}

/*
InvMixColumns without per-byte gmul branches:
t0 ^= 4*(a0^a2), t2 ^= 4*(a0^a2), t1 ^= 4*(a1^a3), t3 ^= 4*(a1^a3)
turns the 14/11/13/9 matrix into the plain 2/3/1/1 MixColumns, since
2*(1^4)^4 == 14, 3*(1^4)^4 == 11, 2*4^5 == 13, 3*4^5 == 9.
*/
inline constexpr void inv_mix_columns(::std::uint_least8_t *s) noexcept
{
	::std::uint_least8_t a0, a1, a2, a3, t;
	a0 = s[0];
	a1 = s[1];
	a2 = s[2];
	a3 = s[3];
	t = xtime(xtime(static_cast<::std::uint_least8_t>(a0 ^ a2)));
	a0 ^= t;
	a2 ^= t;
	t = xtime(xtime(static_cast<::std::uint_least8_t>(a1 ^ a3)));
	a1 ^= t;
	a3 ^= t;
	s[0] = xtime(a0) ^ static_cast<::std::uint_least8_t>(xtime(a1) ^ a1) ^ a2 ^ a3;
	s[1] = a0 ^ xtime(a1) ^ static_cast<::std::uint_least8_t>(xtime(a2) ^ a2) ^ a3;
	s[2] = a0 ^ a1 ^ xtime(a2) ^ static_cast<::std::uint_least8_t>(xtime(a3) ^ a3);
	s[3] = static_cast<::std::uint_least8_t>(xtime(a0) ^ a0) ^ a1 ^ a2 ^ xtime(a3);
	a0 = s[4];
	a1 = s[5];
	a2 = s[6];
	a3 = s[7];
	t = xtime(xtime(static_cast<::std::uint_least8_t>(a0 ^ a2)));
	a0 ^= t;
	a2 ^= t;
	t = xtime(xtime(static_cast<::std::uint_least8_t>(a1 ^ a3)));
	a1 ^= t;
	a3 ^= t;
	s[4] = xtime(a0) ^ static_cast<::std::uint_least8_t>(xtime(a1) ^ a1) ^ a2 ^ a3;
	s[5] = a0 ^ xtime(a1) ^ static_cast<::std::uint_least8_t>(xtime(a2) ^ a2) ^ a3;
	s[6] = a0 ^ a1 ^ xtime(a2) ^ static_cast<::std::uint_least8_t>(xtime(a3) ^ a3);
	s[7] = static_cast<::std::uint_least8_t>(xtime(a0) ^ a0) ^ a1 ^ a2 ^ xtime(a3);
	a0 = s[8];
	a1 = s[9];
	a2 = s[10];
	a3 = s[11];
	t = xtime(xtime(static_cast<::std::uint_least8_t>(a0 ^ a2)));
	a0 ^= t;
	a2 ^= t;
	t = xtime(xtime(static_cast<::std::uint_least8_t>(a1 ^ a3)));
	a1 ^= t;
	a3 ^= t;
	s[8] = xtime(a0) ^ static_cast<::std::uint_least8_t>(xtime(a1) ^ a1) ^ a2 ^ a3;
	s[9] = a0 ^ xtime(a1) ^ static_cast<::std::uint_least8_t>(xtime(a2) ^ a2) ^ a3;
	s[10] = a0 ^ a1 ^ xtime(a2) ^ static_cast<::std::uint_least8_t>(xtime(a3) ^ a3);
	s[11] = static_cast<::std::uint_least8_t>(xtime(a0) ^ a0) ^ a1 ^ a2 ^ xtime(a3);
	a0 = s[12];
	a1 = s[13];
	a2 = s[14];
	a3 = s[15];
	t = xtime(xtime(static_cast<::std::uint_least8_t>(a0 ^ a2)));
	a0 ^= t;
	a2 ^= t;
	t = xtime(xtime(static_cast<::std::uint_least8_t>(a1 ^ a3)));
	a1 ^= t;
	a3 ^= t;
	s[12] = xtime(a0) ^ static_cast<::std::uint_least8_t>(xtime(a1) ^ a1) ^ a2 ^ a3;
	s[13] = a0 ^ xtime(a1) ^ static_cast<::std::uint_least8_t>(xtime(a2) ^ a2) ^ a3;
	s[14] = a0 ^ a1 ^ xtime(a2) ^ static_cast<::std::uint_least8_t>(xtime(a3) ^ a3);
	s[15] = static_cast<::std::uint_least8_t>(xtime(a0) ^ a0) ^ a1 ^ a2 ^ xtime(a3);
}

/* ECB encrypt: nblocks consecutive 16-byte blocks. from/to may alias. */
template <::std::size_t nk>
inline constexpr void encrypt(::std::uint_least8_t const *round_keys,
							  ::std::byte const *from, ::std::size_t nblocks, ::std::byte *to) noexcept
{
	static_assert(nk == 4 || nk == 6 || nk == 8);
	constexpr ::std::size_t rounds{nk + 6};
	::std::uint_least8_t state[16];
	for (; nblocks; --nblocks, from += 16, to += 16)
	{
		load_block(state, from);
		add_round_key(state, round_keys);
		for (::std::size_t r{1}; r != rounds; ++r)
		{
			sub_bytes(state);
			shift_rows(state);
			mix_columns(state);
			add_round_key(state, round_keys + r * 16);
		}
		sub_bytes(state);
		shift_rows(state);
		add_round_key(state, round_keys + rounds * 16);
		store_block(to, state);
	}
	::fast_io::secure_clear_array(state);
}

/* ECB decrypt: same round keys, used in reverse order */
template <::std::size_t nk>
inline constexpr void decrypt(::std::uint_least8_t const *round_keys,
							  ::std::byte const *from, ::std::size_t nblocks, ::std::byte *to) noexcept
{
	static_assert(nk == 4 || nk == 6 || nk == 8);
	constexpr ::std::size_t rounds{nk + 6};
	::std::uint_least8_t state[16];
	for (; nblocks; --nblocks, from += 16, to += 16)
	{
		load_block(state, from);
		add_round_key(state, round_keys + rounds * 16);
		for (::std::size_t r{rounds - 1}; r != 0; --r)
		{
			inv_shift_rows(state);
			inv_sub_bytes(state);
			add_round_key(state, round_keys + r * 16);
			inv_mix_columns(state);
		}
		inv_shift_rows(state);
		inv_sub_bytes(state);
		add_round_key(state, round_keys);
		store_block(to, state);
	}
	::fast_io::secure_clear_array(state);
}

} // namespace fast_io::details::aes
