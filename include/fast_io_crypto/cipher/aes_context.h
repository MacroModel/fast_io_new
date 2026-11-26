#pragma once
/*
fast_io AES (FIPS-197).

Public API:
	::fast_io::aes_ctx<key_size>  key_size is 16, 24 or 32 bytes
	aliases: aes128 / aes192 / aes256

	ctx.encrypt(from, nblocks, to) / ctx.decrypt(from, nblocks, to)
	operate on nblocks consecutive 16-byte blocks (ECB); from and to
	may be identical (in-place); otherwise the ranges must not overlap. The block loop lives inside the backend functions so
	the optimizer sees the whole buffer.

One context serves both directions: decryption reuses the encryption
round keys in reverse order (straightforward inverse cipher), so the
object stores a single byte-oriented key schedule.

Backend layout:
	details::aes::* in aes_scalar.h is the portable constexpr software
implementation. A target advertising x86 AES instructions can process
independent runtime blocks with the same stored encryption key schedule.
*/

#include "aes_scalar.h"

#if defined(__AES__) && (defined(__x86_64__) || defined(__i386__)) && defined(__has_builtin)
#if __has_builtin(__builtin_ia32_aesenc128) && __has_builtin(__builtin_ia32_aesenclast128) && \
	__has_builtin(__builtin_ia32_aesdec128) && __has_builtin(__builtin_ia32_aesdeclast128) && __has_builtin(__builtin_ia32_aesimc128)
#define FAST_IO_AES_CONTEXT_X86_ROUNDS
#include "aes_x86.h"
#endif
#endif

namespace fast_io
{

namespace secure::details { class aes128gcm_key; }

namespace transcoders
{
template <::std::size_t KeyBytes>
class aes_ctr;
}

/**
 * @brief Owns the canonical FIPS 197 round keys for both directions of one AES block cipher.
 * @details Key expansion and block processing use fixed-index memory operations and
 * Boolean substitution or AES hardware rounds. Their source-level control flow depends
 * on the public key size, message length, and selected backend. Security against
 * timing and cache observation requires that the generated code and the target's
 * instructions preserve these properties. Use this primitive inside an appropriate mode.
 */
template <::std::size_t key_size_bytes>
	requires(key_size_bytes == 16 || key_size_bytes == 24 || key_size_bytes == 32)
class aes_ctx
{
public:
	inline static constexpr ::std::size_t block_size{16};
	inline static constexpr ::std::size_t key_size{key_size_bytes};
	inline static constexpr ::std::size_t nk{key_size_bytes / 4u};
	inline static constexpr ::std::size_t rounds{nk + 6u};
	inline static constexpr ::std::size_t key_schedule_size{block_size * (rounds + 1u)};

private:
	friend class ::fast_io::transcoders::aes_ctr<key_size_bytes>;
	friend class ::fast_io::secure::details::aes128gcm_key;
	::std::uint_least8_t round_keys[key_schedule_size]{};

	/** @brief Offers a complete register-resident CTR run when the selected backend can supply one. */
	inline constexpr ::std::size_t transform_ctr_bulk(::std::byte *counter, ::std::byte const *source,
		::std::byte *destination, ::std::size_t size) const noexcept
	{
#ifdef FAST_IO_AES_CONTEXT_X86_ROUNDS
		if (!::std::is_constant_evaluated())
		{
			return ::fast_io::details::aes::x86_aes_ctr_bulk<rounds>(round_keys, counter, source, destination, size);
		}
#endif
		(void)counter;
		(void)source;
		(void)destination;
		(void)size;
		return 0;
	}

public:
	/** @brief Expands exactly key_size input bytes into an encryption schedule shared by both directions. */
	inline explicit constexpr aes_ctx(::std::byte const *key) noexcept
	{
#ifdef FAST_IO_AES_CONTEXT_X86_ROUNDS
		if (!::std::is_constant_evaluated())
		{
			::fast_io::details::aes::key_expansion<nk, ::fast_io::details::aes::x86_aes_sub_word>(key, round_keys);
			return;
		}
#endif
		::fast_io::details::aes::key_expansion<nk>(key, round_keys);
	}

	/** @brief A copied block cipher owns its own schedule. Moving clears the source schedule. */
	constexpr aes_ctx(aes_ctx const &) noexcept = default;
	constexpr aes_ctx &operator=(aes_ctx const &) noexcept = default;

	constexpr aes_ctx(aes_ctx &&other) noexcept : aes_ctx(static_cast<aes_ctx const &>(other))
	{
		::fast_io::secure_clear_array(other.round_keys);
	}

	constexpr aes_ctx &operator=(aes_ctx &&other) noexcept
	{
		if (this != __builtin_addressof(other))
		{
			*this = static_cast<aes_ctx const &>(other);
			::fast_io::secure_clear_array(other.round_keys);
		}
		return *this;
	}

	constexpr ~aes_ctx() { ::fast_io::secure_clear_array(round_keys); }

	/** @brief Encrypts complete 16-byte blocks, allowing identical source and destination addresses. */
	inline constexpr void encrypt(::std::byte const *from, ::std::size_t nblocks,
								  ::std::byte *to) const noexcept
	{
#ifdef FAST_IO_AES_CONTEXT_X86_ROUNDS
		if (!::std::is_constant_evaluated())
		{
			return ::fast_io::details::aes::x86_aes_blocks<rounds, false>(round_keys, from, nblocks, to);
		}
#endif
		::fast_io::details::aes::encrypt<nk>(round_keys, from, nblocks, to);
	}

	/** @brief Decrypts complete 16-byte blocks using the inverse round order and preserves in-place operation. */
	inline constexpr void decrypt(::std::byte const *from, ::std::size_t nblocks,
								  ::std::byte *to) const noexcept
	{
#ifdef FAST_IO_AES_CONTEXT_X86_ROUNDS
		if (!::std::is_constant_evaluated())
		{
			return ::fast_io::details::aes::x86_aes_blocks<rounds, true>(round_keys, from, nblocks, to);
		}
#endif
		::fast_io::details::aes::decrypt<nk>(round_keys, from, nblocks, to);
	}
};

using aes128 = aes_ctx<16>;
using aes192 = aes_ctx<24>;
using aes256 = aes_ctx<32>;

} // namespace fast_io

#undef FAST_IO_AES_CONTEXT_X86_ROUNDS
