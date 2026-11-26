#pragma once
/** @file Ed25519 signatures (RFC 8032) and X25519 key agreement (RFC 7748). */
#include "fast_io_hosted.h"
#include "fast_io_secure/error.h"
#include "fast_io_secure/key_material.h"
#include "fast_io_secure/curve25519.h"

namespace fast_io::secure
{
using ed25519_public_key = ::std::array<::std::byte,32>;
using ed25519_signature = ::std::array<::std::byte,64>;
using x25519_public_key = ::std::array<::std::byte,32>;

namespace details
{
inline unsigned char *octets(::std::byte *p) noexcept { return reinterpret_cast<unsigned char *>(p); }
inline unsigned char const *octets(::std::byte const *p) noexcept { return reinterpret_cast<unsigned char const *>(p); }

inline private_bytes<32> random_private_key()
{
	private_bytes<32> result;
	::fast_io::native_white_hole random;
	::fast_io::operations::read_all_bytes(random,result.value.data(),result.value.data()+32);
	return result;
}

template <typename... Parts>
inline void ed_hash(::std::byte *result, Parts const &...parts) noexcept
{
	::fast_io::sha512_context hash;
	::fast_io::secure_clear_guard erase{&hash,1};
	(hash_part(hash,parts),...);
	hash.do_final();
	hash.digest_to_byte_ptr(result);
}

template <typename... Parts>
inline void ed_reduce_hash(::std::byte *result, Parts const &...parts) noexcept
{
	private_bytes<64> wide;
	ed_hash(wide.value.data(),parts...);
	curve25519::crypto_eddsa_reduce(octets(result),octets(wide.value.data()));
}

inline bool canonical_ed_point(::std::byte const *p) noexcept
{
	if ((static_cast<unsigned>(p[31]) & 127u) < 127u) return true;
	for (unsigned i{1}; i != 31; ++i)
		if (p[i] != ::std::byte{255}) return true;
	return static_cast<unsigned>(p[0]) < 237u;
}

inline bytes_view ed_ph_domain() noexcept
{
	// RFC 8032 dom2(F=1, C=""), including the empty-context length byte.
	static constexpr char8_t domain[]{u8"SigEd25519 no Ed25519 collisions\x01\x00"};
	return label_bytes(domain);
}

inline bool ed_verify(ed25519_public_key const &pk,bytes_view message,ed25519_signature const &sig,bytes_view domain) noexcept
{
	if (!canonical_ed_point(pk.data()) || !canonical_ed_point(sig.data())) return false;
	::std::byte challenge[32];
	ed_reduce_hash(challenge,domain,bytes_view{sig.data(),32},pk,message);
	return curve25519::crypto_eddsa_check_equation(octets(sig.data()),octets(pk.data()),octets(challenge)) == 0;
}
} // namespace details

/** @brief Retains the expanded secret and its derived public key. Moving erases the source secret. */
class ed25519_signing_key
{
	details::private_bytes<64> expanded;
	ed25519_public_key pk;
	bool active{true};

	/** @brief Signs with an initialized expansion, including the RFC 8032 domain supplied by the caller. */
	ed25519_signature sign_active(details::bytes_view message,details::bytes_view domain) const noexcept
	{
		using namespace details;
		private_bytes<32> nonce;
		ed_reduce_hash(nonce.value.data(),domain,bytes_view{expanded.value.data()+32,32},message);
		ed25519_signature sig;
		curve25519::crypto_eddsa_scalarbase(octets(sig.data()),octets(nonce.value.data()));
		::std::byte challenge[32];
		ed_reduce_hash(challenge,domain,bytes_view{sig.data(),32},pk,message);
		curve25519::crypto_eddsa_mul_add(octets(sig.data()+32),octets(challenge),octets(expanded.value.data()),octets(nonce.value.data()));
		return sig;
	}
public:
	explicit ed25519_signing_key(::std::span<::std::byte const,32> seed) noexcept
	{
		details::ed_hash(expanded.value.data(),seed);
		auto *scalar{details::octets(expanded.value.data())};
		details::curve25519::crypto_eddsa_trim_scalar(scalar,scalar);
		details::curve25519::crypto_eddsa_scalarbase(details::octets(pk.data()),scalar);
	}
	static ed25519_signing_key generate() { return ed25519_signing_key{details::random_private_key().value}; }
	ed25519_signing_key(ed25519_signing_key &&other) noexcept
		: expanded(::std::move(other.expanded)), pk(other.pk), active(::std::exchange(other.active,false)) {}
	ed25519_public_key const &public_key() const noexcept { return pk; }
	/** @brief Signs using a fresh expansion of a fixed-length seed. All 32-byte seed values are admissible. */
	static ed25519_signature sign_from_seed(::std::span<::std::byte const,32> seed, details::bytes_view message) noexcept
	{
		return ed25519_signing_key{seed}.sign_active(message,{});
	}
	ed25519_signature sign(details::bytes_view message) const
	{ details::check(active,errc::configuration); return sign_active(message,{}); }
	ed25519_signature sign_prehashed(::std::span<::std::byte const,64> sha512_digest) const
	{ details::check(active,errc::configuration); return sign_active(sha512_digest,details::ed_ph_domain()); }
};

inline bool ed25519_verify(ed25519_public_key const &pk,details::bytes_view message,ed25519_signature const &sig) noexcept
{ return details::ed_verify(pk,message,sig,{}); }

inline bool ed25519ph_verify(ed25519_public_key const &pk,::std::span<::std::byte const,64> digest,ed25519_signature const &sig) noexcept
{ return details::ed_verify(pk,digest,sig,details::ed_ph_domain()); }

/** @brief An X25519 private key with all-zero shared-secret rejection. */
class x25519_private_key
{
	details::private_bytes<32> scalar;
	bool active{true};
public:
	explicit x25519_private_key(::std::span<::std::byte const,32> secret) noexcept
	{ ::fast_io::freestanding::my_memcpy(scalar.value.data(),secret.data(),32); }
	static x25519_private_key generate() { return x25519_private_key{details::random_private_key().value}; }
	x25519_private_key(x25519_private_key &&other) noexcept
		: scalar(::std::move(other.scalar)), active(::std::exchange(other.active,false)) {}
	/** @brief Derives the RFC 7748 base-point product for any fixed-length scalar without a peer-validation step. */
	static x25519_public_key public_from_scalar(::std::span<::std::byte const,32> secret) noexcept
	{
		x25519_public_key pk;
		details::curve25519::crypto_x25519_public_key(details::octets(pk.data()),details::octets(secret.data()));
		return pk;
	}
	x25519_public_key public_key() const
	{ details::check(active,errc::configuration); return public_from_scalar(scalar.value); }
	/** @brief Computes X25519 and leaves the destination untouched when the peer gives an all-zero shared secret. */
	[[nodiscard]] bool try_agree(x25519_public_key const &peer,::std::span<::std::byte,32> output) const noexcept
	{
		if (!active) return false;
		details::private_bytes<32> result;
		auto *shared{details::octets(result.value.data())};
		details::curve25519::crypto_x25519(shared,details::octets(scalar.value.data()),details::octets(peer.data()));
		constexpr unsigned char zeros[32]{};
		if (!details::curve25519::crypto_verify32(shared,zeros)) return false;
		::fast_io::freestanding::my_memcpy(output.data(),shared,32);
		return true;
	}
	/** @brief Rejects a moved-from private key or low-order peer point before exposing a secret. */
	void agree(x25519_public_key const &peer,::std::span<::std::byte,32> output) const
	{
		details::check(active,errc::configuration);
		details::check(try_agree(peer,output),errc::peer_identity);
	}
};
} // namespace fast_io::secure
