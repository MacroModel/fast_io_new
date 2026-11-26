#pragma once
/** @file RFC 9180 base-mode HPKE with DHKEM(X25519, HKDF-SHA256), HKDF-SHA256 and AES-128-GCM. */
#include "fast_io_public_key.h"
#include "fast_io_secure/rfc8188_crypto.h"

namespace fast_io::secure
{
namespace details
{
inline constexpr char8_t hpke_kem_suite[]{u8"KEM\x00\x20"};
inline constexpr char8_t hpke_suite[]{u8"HPKE\x00\x20\x00\x01\x00\x01"};

template <typename... Parts>
inline void hpke_extract(::std::byte *out,bytes_view salt,bytes_view suite,bytes_view label,Parts const &...parts) noexcept
{ sha256_hmac(salt,out,label_bytes(u8"HPKE-v1"),suite,label,parts...); }

template <typename... Parts>
inline void hpke_expand(::std::span<::std::byte> out,bytes_view prk,bytes_view suite,bytes_view label,Parts const &...parts)
{
	check(out.size() <= 255u*32u,errc::configuration);
	::std::byte length[]{static_cast<::std::byte>(out.size() >> 8u),static_cast<::std::byte>(out.size() & 255u)};
	private_bytes<32> block;
	bytes_view previous{};
	for (unsigned i{1}; !out.empty(); ++i)
	{
		::std::byte counter{static_cast<::std::byte>(i)};
		sha256_hmac(prk,block.value.data(),previous,length,label_bytes(u8"HPKE-v1"),suite,label,parts...,bytes_view{&counter,1});
		previous = block.value;
		auto size{out.size() < 32 ? out.size() : 32u};
		::fast_io::freestanding::my_memcpy(out.data(),block.value.data(),size);
		out = out.subspan(size);
	}
}

struct hpke_material
{
	x25519_public_key encapsulation;
	private_bytes<16> key;
	private_bytes<12> nonce;
	private_bytes<32> exporter;
};

inline hpke_material hpke_schedule(x25519_private_key const &own,x25519_public_key const &peer,
	x25519_public_key const &enc,x25519_public_key const &recipient,bytes_view info)
{
	private_bytes<32> dh,prk,shared,secret;
	auto kem{label_bytes(hpke_kem_suite)}, suite{label_bytes(hpke_suite)};
	own.agree(peer,dh.value);
	hpke_extract(prk.value.data(),{},kem,label_bytes(u8"eae_prk"),dh.value);
	hpke_expand(shared.value,prk.value,kem,label_bytes(u8"shared_secret"),enc,recipient);
	::std::byte context[65]{};
	hpke_extract(context+1,{},suite,label_bytes(u8"psk_id_hash"));
	hpke_extract(context+33,{},suite,label_bytes(u8"info_hash"),info);
	hpke_extract(secret.value.data(),shared.value,suite,label_bytes(u8"secret"));
	hpke_material material{enc,{},{},{}};
	hpke_expand(material.key.value,secret.value,suite,label_bytes(u8"key"),context);
	hpke_expand(material.nonce.value,secret.value,suite,label_bytes(u8"base_nonce"),context);
	hpke_expand(material.exporter.value,secret.value,suite,label_bytes(u8"exp"),context);
	return material;
}

inline bool hpke_overlap(bytes_view a,bytes_view b) noexcept
{
	if (a.empty() || b.empty()) return false;
	::std::less<::std::byte const *> less;
	return less(a.data(),b.data()+b.size()) && less(b.data(),a.data()+a.size());
}
} // namespace details

/** @brief Ordered HPKE context. The sender transmits encapsulated_key() once, and each application supplies its message boundaries. */
template <bool Sender>
class basic_hpke_context
{
	x25519_public_key enc;
	details::aes128gcm_key cipher;
	details::private_bytes<12> nonce;
	details::private_bytes<32> exporter;
	::std::uint_least64_t sequence{}, blocks{};

	explicit basic_hpke_context(details::hpke_material material)
		: enc(material.encapsulation), cipher(material.key.value.data()), nonce(::std::move(material.nonce)),
		  exporter(::std::move(material.exporter)) {}

	void begin_message(::std::size_t size, details::bytes_view aad, unsigned char (&iv)[12])
	{
		constexpr ::std::uint_least64_t block_budget{1ULL << 32u};
		details::check(size <= (1ULL << 36u)-32u && aad.size() <= (1ULL << 36u),errc::usage_limit);
		auto cost{2u + (static_cast<::std::uint_least64_t>(size)+15u)/16u + (static_cast<::std::uint_least64_t>(aad.size())+15u)/16u};
		details::check(cost <= block_budget - blocks,errc::usage_limit);
		blocks += cost;
		::fast_io::freestanding::my_memcpy(iv,nonce.value.data(),12);
		auto seq{sequence};
		for (unsigned i{}; i != 8; ++i, seq >>= 8u) iv[11-i] ^= static_cast<unsigned char>(seq);
	}

public:
	basic_hpke_context(basic_hpke_context const &) = delete;
	basic_hpke_context &operator=(basic_hpke_context const &) = delete;

	static basic_hpke_context setup(x25519_public_key const &recipient,details::bytes_view info = {}) requires Sender
	{
		auto ephemeral{x25519_private_key::generate()};
		return basic_hpke_context{details::hpke_schedule(ephemeral,recipient,ephemeral.public_key(),recipient,info)};
	}
	static basic_hpke_context setup(x25519_private_key const &recipient,x25519_public_key const &encapsulation,
		details::bytes_view info = {}) requires (!Sender)
	{
		return basic_hpke_context{details::hpke_schedule(recipient,encapsulation,encapsulation,recipient.public_key(),info)};
	}
	x25519_public_key const &encapsulated_key() const noexcept { return enc; }

	/** @brief Seals into a destination of exactly plaintext.size()+16 bytes. Exact in-place encryption is supported. */
	void seal(details::bytes_view aad,details::bytes_view plain,::std::span<::std::byte> out) requires Sender
	{
		details::check(out.size() >= 16 && out.size()-16 == plain.size() &&
			(out.data()==plain.data() || !details::hpke_overlap(out,plain)) && !details::hpke_overlap(out,aad),errc::configuration);
		unsigned char iv[12];
		begin_message(plain.size(),aad,iv);
		auto dest{details::octets(out.data())};
		cipher.seal(iv,details::octets(plain.data()),plain.size(),dest,dest+plain.size(),aad);
		++sequence;
	}

	/** @brief Verifies the full tag before writing. A failed authentication leaves the output and sequence unchanged. */
	void open(details::bytes_view aad,details::bytes_view ciphertext,::std::span<::std::byte> out) requires (!Sender)
	{
		details::check(ciphertext.size() >= 16,errc::invalid_message);
		details::check(ciphertext.size()-16 == out.size() &&
			(out.data()==ciphertext.data() || !details::hpke_overlap(out,ciphertext)),errc::configuration);
		unsigned char iv[12];
		begin_message(out.size(),aad,iv);
		auto data{details::octets(ciphertext.data())};
		details::check(cipher.try_open(iv,data,out.size(),data+out.size(),details::octets(out.data()),aad),errc::authentication);
		++sequence;
	}

	void export_secret(details::bytes_view context,::std::span<::std::byte> out) const
	{
		details::check(!details::hpke_overlap(context,out),errc::configuration);
		details::hpke_expand(out,exporter.value,details::label_bytes(details::hpke_suite),details::label_bytes(u8"sec"),context);
	}
};
using hpke_sender = basic_hpke_context<true>;
using hpke_recipient = basic_hpke_context<false>;
} // namespace fast_io::secure
