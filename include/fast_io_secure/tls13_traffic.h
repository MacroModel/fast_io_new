#pragma once
#include "error.h"
#include "rfc8188_crypto.h"
#include <optional>

#include "../fast_io_dsal/impl/misc/push_macros.h"

namespace fast_io::secure::details
{
/** @brief Unwinds through the selected exception channel while keeping the exact peer failure code. */
[[noreturn]] inline void tls_wire_fail(errc code) FAST_IO_HERBCEPTIONS_THROWS
{
#if defined(__HERBCEPTIONS__)
	throw throws code;
#else
	fail(code);
#endif
}

/** @brief Rejects malformed wire input before accessing the affected buffer or publishing any application bytes. */
inline void tls_wire_check(bool condition,errc code=errc::tls_protocol) FAST_IO_HERBCEPTIONS_THROWS
{
	if (!condition) [[unlikely]] tls_wire_fail(code);
}

/** @brief Finalizes a copy of the running transcript and erases its temporary hash state. */
inline ::std::array<::std::byte,32> tls_digest(::fast_io::sha256_context hash) noexcept
{
	::std::array<::std::byte,32> digest;
	hash.do_final();
	hash.digest_to_byte_ptr(digest.data());
	::fast_io::secure_clear(&hash,sizeof(hash));
	return digest;
}

/** @brief TLS 1.3 HKDF-Expand-Label for the one-block SHA-256 traffic, Finished and IV values. */
template <::std::size_t N>
requires (N>0 && N<=32)
inline void tls_expand(bytes_view secret,bytes_view label,bytes_view context,::std::array<::std::byte,N> &output)
{
	// The six-byte protocol prefix and context length both occupy one-byte vectors.
	check(label.size()<=249u && context.size()<=255u,errc::configuration);
	::std::byte sizes[]{::std::byte{},static_cast<::std::byte>(output.size()),static_cast<::std::byte>(6+label.size())};
	::std::byte context_size{static_cast<::std::byte>(context.size())},counter{1};
	private_bytes<32> block;
	sha256_hmac(secret,block.value.data(),sizes,label_bytes(u8"tls13 "),label,bytes_view{&context_size,1},context,bytes_view{&counter,1});
	::fast_io::freestanding::my_memcpy(output.data(),block.value.data(),output.size());
}

/** @brief Tracks outgoing application epochs independently of the peer's unconstrained receive-side epoch count. */
struct tls13_send_updates
{
	static constexpr ::std::uint_least64_t maximum{(::std::uint_least64_t{1}<<48u)-1u};
	::std::uint_least64_t count{};
	bool requested{};

	/** @brief Coalesces peer requests at the next application write and enforces the RFC 9846 section 4.7.3 sending limit. */
	bool take(bool due) FAST_IO_HERBCEPTIONS_THROWS
	{
		if (!due && !requested) return false;
		requested=false;
		if (count==maximum)
		{
			// At the last epoch, ignore a peer request but stop before the local AES-GCM limit is reached.
			tls_wire_check(!due,errc::usage_limit);
			return false;
		}
		++count;
		return true;
	}
};

/** @brief Holds one direction's AES-128-GCM traffic epoch, IV and monotonically advancing sequence number. */
struct tls13_traffic
{
	/** @brief RFC 9846 section 5.5 bounds AES-GCM use per key. Reserve one record for an epoch change. */
	static constexpr ::std::uint_least64_t record_limit{1u << 24u};
	private_bytes<32> secret;
	private_bytes<12> iv;
	::std::optional<aes128gcm_key> cipher;
	::std::uint_least64_t sequence{};

	/** @brief Discards the expanded cipher and erases both the traffic secret and static IV. */
	void clear() noexcept
	{
		cipher.reset();
		secret.clear();
		iv.clear();
	}
	/** @brief Derives a new epoch's key and IV from exactly one SHA-256 traffic secret, then resets its nonce sequence. */
	void install(::std::span<::std::byte const,32> next)
	{
		::fast_io::freestanding::my_memcpy(secret.value.data(),next.data(),32);
		private_bytes<16> key;
		tls_expand(secret.value,label_bytes(u8"key"),{},key.value);
		tls_expand(secret.value,label_bytes(u8"iv"),{},iv.value);
		cipher.emplace(key.value.data());
		sequence = 0;
	}
	/** @brief Derives the next application traffic secret as specified by RFC 8446 section 7.2. */
	void update()
	{
		private_bytes<32> next;
		tls_expand(secret.value,label_bytes(u8"traffic upd"),{},next.value);
		install(next.value);
	}
	/** @brief Computes the unique TLS nonce for one record and enforces the conservative AES-GCM per-epoch bound. */
	void next_nonce(unsigned char (&nonce)[12]) FAST_IO_HERBCEPTIONS_THROWS
	{
		tls_wire_check(sequence < record_limit,errc::usage_limit);
		::fast_io::freestanding::my_memcpy(nonce,iv.value.data(),12);
		auto n{sequence++};
		for (unsigned i{}; i != 8; ++i,n >>= 8u) nonce[11-i] ^= static_cast<unsigned char>(n);
	}
	/** @brief Encrypts a record in place and authenticates its associated five-byte TLSCiphertext header. */
	void protect(::std::span<::std::byte> data,bytes_view header,unsigned char *tag) FAST_IO_HERBCEPTIONS_THROWS
	{
		unsigned char nonce[12];
		next_nonce(nonce);
		auto p{reinterpret_cast<unsigned char *>(data.data())};
		cipher->seal(nonce,p,data.size(),p,tag,header);
	}
	/** @brief Verifies the entire ciphertext and associated header before exposing any plaintext bytes. */
	void unprotect(::std::span<::std::byte> data,bytes_view header,unsigned char const *tag) FAST_IO_HERBCEPTIONS_THROWS
	{
		unsigned char nonce[12];
		next_nonce(nonce);
		auto p{reinterpret_cast<unsigned char *>(data.data())};
		tls_wire_check(cipher->try_open(nonce,p,data.size(),tag,p,header),errc::authentication);
	}
};
} // namespace fast_io::secure::details

#include "../fast_io_dsal/impl/misc/pop_macros.h"
