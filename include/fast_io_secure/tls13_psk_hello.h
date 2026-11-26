/** @file @brief TLS 1.3 external-PSK Hello encoding, transcript authentication and DHE key schedule. */
#pragma once
#include "../fast_io_public_key.h"
#include "tls13_traffic.h"
#include <vector>
#include <algorithm>

#include "../fast_io_dsal/impl/misc/push_macros.h"

namespace fast_io::secure::details
{
using tls_buffer = ::std::vector<::std::byte>;
using tls_digest_type = ::std::array<::std::byte,32>;

/** @brief Checks a wire-level TLS constraint and reports a terminal protocol failure. */
inline void tls_check(bool condition) { check(condition,errc::tls_protocol); }

/** @brief Consumes one length-delimited wire vector while preserving its enclosing record boundary. */
struct tls_cursor
{
	bytes_view bytes;
	/** @brief Borrows exactly the requested prefix and advances the parse cursor only after checking the bound. */
	bytes_view take(::std::size_t n) FAST_IO_HERBCEPTIONS_THROWS
	{
		tls_wire_check(n <= bytes.size());
		auto result{bytes.first(n)};
		bytes = bytes.subspan(n);
		return result;
	}
	/** @brief Reads a TLS network-order integer from the remaining vector. */
	unsigned integer(unsigned width) FAST_IO_HERBCEPTIONS_THROWS
	{
		unsigned result{};
		for (auto b : take(width)) result = (result << 8u) | static_cast<unsigned>(b);
		return result;
	}
	/** @brief Decodes an opaque length-prefixed vector and returns an independently bounded parse cursor. */
	tls_cursor vector(unsigned width) FAST_IO_HERBCEPTIONS_THROWS { return {take(integer(width))}; }
	/** @brief Requires the complete enclosing vector to have been consumed. */
	void done() const FAST_IO_HERBCEPTIONS_THROWS { tls_wire_check(bytes.empty()); }
};

/** @brief Appends a fixed-width unsigned TLS wire integer in network byte order. */
inline void tls_uint(tls_buffer &v,unsigned n,unsigned width)
{
	while (width) v.push_back(static_cast<::std::byte>((n >> (8u * --width)) & 255u));
}
/** @brief Appends the supplied wire bytes to the message under construction. */
inline void tls_append(tls_buffer &out,bytes_view data) { out.insert(out.end(),data.begin(),data.end()); }
/** @brief Reserves a wire length field and fills it only after validating the final child vector length. */
template <typename F>
inline void tls_vector(tls_buffer &out,unsigned width,F build)
{
	auto pos{out.size()};
	out.resize(pos+width);
	build(out);
	auto n{out.size()-pos-width};
	tls_check(n < (::std::size_t{1} << (width*8)));
	for (unsigned i{};i != width;++i,n >>= 8u) out[pos+width-1-i] = static_cast<::std::byte>(n & 255u);
}
/** @brief Encodes a bounded TLS opaque vector with its requested length prefix. */
inline void tls_string(tls_buffer &v,bytes_view data,unsigned width)
{ tls_vector(v,width,[&](auto &out){tls_append(out,data);}); }

/** @brief Encodes one named TLS extension and its checked 16-bit body length. */
template <typename F>
inline void tls_extension(tls_buffer &v,unsigned type,F build)
{ tls_uint(v,type,2); tls_vector(v,2,build); }

/** @brief Validates one framed extension and enters its handler exactly once.
 * @details The public 16-bit type chooses a bit. Checking its length and its uniqueness precedes
 * every callback. The handler must consume the complete body even for an unknown extension.
 */
template <typename Admit, typename F>
inline void tls_extension_body(tls_cursor &exts,unsigned type,Admit admit,F &parse) FAST_IO_HERBCEPTIONS_THROWS
{
	auto body{exts.vector(2)};
	tls_wire_check(admit());
	parse(type,body,exts.bytes.empty());
	body.done();
}

/** @brief Records membership in one aligned group of 64 public extension IDs and returns whether the ID is new. */
inline bool tls_mark_extension(::std::uint64_t &word,unsigned type) noexcept
{
	auto mask{::std::uint64_t{1} << (type % 64u)};
	bool fresh{!(word & mask)};
	word |= mask;
	return fresh;
}

/** @brief Continues over the full TLS ExtensionType space after the fixed list of high IDs fills.
 * @details The carried word and high IDs cover every earlier type. Every remaining type has exactly one bit in
 * this table, including unrecognized extensions. The work is linear in wire length and all table
 * addresses depend solely on public handshake metadata. The larger stack frame is needed only here.
 */
template <typename F>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline void tls_extended_extensions(tls_cursor &exts,unsigned type,::std::uint64_t low,
	::std::array<::std::uint16_t,8> const &high,F &parse) FAST_IO_HERBCEPTIONS_THROWS
{
	::std::array<::std::uint64_t,1024> seen{};
	seen[0] = low;
	for (auto key : high) (void)tls_mark_extension(seen[key/64u],key);
	for (;;)
	{
		tls_extension_body(exts,type,[&]() noexcept { return tls_mark_extension(seen[type/64u],type); },parse);
		if (exts.bytes.empty()) return;
		type = exts.integer(2);
	}
}

/** @brief Tracks a short list of nonstandard or higher-numbered public IDs without initializing the complete namespace.
 * @details Each lookup costs at most eight scalar comparisons. A ninth high ID switches once to a
 * direct membership table. This bounds processing even when the peer chooses all extension IDs.
 */
template <typename F>
inline void tls_sparse_extensions(tls_cursor &exts,unsigned type,::std::uint64_t low,F &parse) FAST_IO_HERBCEPTIONS_THROWS
{
	::std::array<::std::uint16_t,8> high;
	::std::size_t used{};
	for (;;)
	{
		if (type >= 64u && used == high.size())
		{
			tls_extended_extensions(exts,type,low,high,parse);
			return;
		}
		tls_extension_body(exts,type,[&]() noexcept {
			if (type < 64u) return tls_mark_extension(low,type);
			for (::std::size_t i{}; i != used; ++i) if (high[i] == type) return false;
			high[used++] = static_cast<::std::uint16_t>(type);
			return true;
		},parse);
		if (exts.bytes.empty()) return;
		type = exts.integer(2);
	}
}

/** @brief Visits extensions in wire order and rejects every repeated type as required by RFC 9846 §4.3.
 * @details A hello whose types are all below 64 needs one word of duplicate state. Higher IDs use
 * a bounded list or the complete namespace, always carrying the exact prefix state. No path publishes
 * an extension twice or changes the transcript, PSK binder checks or peer authentication.
 */
template <typename F>
inline void tls_extensions(tls_cursor &c,F parse) FAST_IO_HERBCEPTIONS_THROWS
{
	auto exts{c.vector(2)};
	c.done();
	::std::uint64_t low{};
	while (!exts.bytes.empty())
	{
		auto type{exts.integer(2)};
		if (type >= 64u)
		{
			tls_sparse_extensions(exts,type,low,parse);
			return;
		}
		tls_extension_body(exts,type,[&]() noexcept { return tls_mark_extension(low,type); },parse);
	}
}
/** @brief Checks membership in a nonempty, correctly framed peer algorithm or group list. */
inline bool tls_contains(tls_cursor list,unsigned value,unsigned width) FAST_IO_HERBCEPTIONS_THROWS
{
	tls_wire_check(!list.bytes.empty() && list.bytes.size()%width==0);
	bool found{};
	while (!list.bytes.empty()) found |= list.integer(width)==value;
	return found;
}
/** @brief Compares public PSK identity bytes before choosing the corresponding binder. */
inline bool same_bytes(bytes_view a,bytes_view b) noexcept
{ return a.size()==b.size() && ::std::equal(a.begin(),a.end(),b.begin()); }
/** @brief Verifies a fixed-size binder or Finished MAC using the constant-time 32-byte equality primitive. */
inline bool tls_auth_equal(bytes_view a,bytes_view b) noexcept
{
	return a.size()==32 && b.size()==32 &&
		curve25519::crypto_verify32(octets(a.data()),octets(b.data())) == 0;
}

/** @brief Hashes one complete transcript fragment without storing a second copy of its bytes. */
inline tls_digest_type tls_hash(bytes_view data) noexcept
{
	::fast_io::sha256_context hash;
	hash_part(hash,data);
	return tls_digest(hash);
}
/** @brief Computes the RFC 8446 Finished verify_data for the supplied traffic or binder secret and transcript digest. */
inline tls_digest_type tls_finished(bytes_view secret,bytes_view transcript) noexcept
{
	private_bytes<32> key;
	tls_expand(secret,label_bytes(u8"finished"),{},key.value);
	tls_digest_type result;
	sha256_hmac(key.value,result.data(),transcript);
	return result;
}

/** @brief Owns the unique X25519 key and secret schedule of one TLS 1.3 handshake. */
struct tls_psk_handshake
{
	x25519_private_key ephemeral{x25519_private_key::generate()};
	x25519_public_key peer{};
	private_bytes<32> early,master,client_handshake,server_handshake,client_app,server_app;
	::fast_io::sha256_context transcript;
	tls_digest_type empty_hash{tls_hash({})};
	tls_buffer session_id;
	unsigned selected{};

	/** @brief Extracts the early secret from the provisioned external PSK using SHA-256 HKDF-Extract. */
	explicit tls_psk_handshake(::std::span<::std::byte const,32> psk) { sha256_hmac({},early.value.data(),psk); }
	tls_psk_handshake(tls_psk_handshake const &) = delete;
	/** @brief Erases the stored transcript context in addition to the independently erased private key material. */
	~tls_psk_handshake() { ::fast_io::secure_clear(&transcript,sizeof(transcript)); }

	/** @brief Authenticates the truncated ClientHello with the external-PSK binder label. */
	tls_digest_type binder(bytes_view truncated_hello) const noexcept
	{
		private_bytes<32> key;
		tls_expand(early.value,label_bytes(u8"ext binder"),empty_hash,key.value);
		return tls_finished(key.value,tls_hash(truncated_hello));
	}
	/** @brief Emits the required legacy version followed by 32 fresh bytes from the operating system RNG. */
	static void random_hello(tls_buffer &out)
	{
		tls_uint(out,0x0303,2);
		::std::byte random[32];
		::fast_io::native_white_hole rng;
		::fast_io::operations::read_all_bytes(rng,random,random+32);
		tls_append(out,random);
	}
	/** @brief Builds a TLS 1.3 ClientHello offering X25519, AES-128-GCM, a single external PSK and its transcript binder. */
	tls_buffer client_hello(bytes_view identity)
	{
		tls_buffer hello{::std::byte{1}};
		auto share{ephemeral.public_key()};
		tls_vector(hello,3,[&](auto &v){
			random_hello(v);
			tls_uint(v,0,1); // Empty legacy session ID.
			tls_uint(v,2,2); tls_uint(v,0x1301,2);
			tls_uint(v,1,1); tls_uint(v,0,1);
			tls_vector(v,2,[&](auto &ext){
				tls_extension(ext,43,[](auto &e){tls_uint(e,2,1);tls_uint(e,0x0304,2);});
				tls_extension(ext,10,[](auto &e){tls_uint(e,2,2);tls_uint(e,29,2);});
				tls_extension(ext,51,[&](auto &e){tls_uint(e,36,2);tls_uint(e,29,2);tls_string(e,share,2);});
				tls_extension(ext,45,[](auto &e){tls_uint(e,1,1);tls_uint(e,1,1);});
				tls_extension(ext,41,[&](auto &e){
					tls_vector(e,2,[&](auto &ids){tls_string(ids,identity,2);tls_uint(ids,0,4);});
					tls_uint(e,33,2);tls_uint(e,32,1);e.resize(e.size()+32);
				});
			});
		});
		auto value{binder(bytes_view{hello}.first(hello.size()-35))};
		::std::copy(value.begin(),value.end(),hello.end()-32);
		return hello;
	}
	/** @brief Validates the client proposal and the matching identity binder before accepting its public key share. */
	void accept_client(bytes_view message,bytes_view expected) FAST_IO_HERBCEPTIONS_THROWS
	{
		tls_cursor c{message.subspan(4)};
		tls_wire_check(c.integer(2)==0x0303); c.take(32);
		auto id{c.vector(1).bytes}; tls_wire_check(id.size()<=32);
		session_id.assign(id.begin(),id.end());
		tls_wire_check(tls_contains(c.vector(2),0x1301,2));
		auto compression{c.vector(1)};
		tls_wire_check(compression.integer(1)==0);compression.done();
		bool version{},group{},share{},mode{},psk{};
		tls_extensions(c,[&](unsigned type,tls_cursor &e,bool last) FAST_IO_HERBCEPTIONS_THROWS {
			switch (type)
			{
			case 43: version=tls_contains(e.vector(1),0x0304,2); break;
			case 10: group=tls_contains(e.vector(2),29,2); break;
			case 45: mode=tls_contains(e.vector(1),1,1); break;
			case 51:
			{
				auto list{e.vector(2)};
				while (!list.bytes.empty())
				{
					auto named{list.integer(2)};auto value{list.vector(2).bytes};
					tls_wire_check(!value.empty());
					if (named==29) { tls_wire_check(!share && value.size()==32);::std::copy(value.begin(),value.end(),peer.begin());share=true; }
				}
				break;
			}
			case 41:
			{
				tls_wire_check(last);auto ids{e.vector(2)};
				unsigned count{},matched{~0u};
				while (!ids.bytes.empty())
				{
					auto value{ids.vector(2).bytes};tls_wire_check(!value.empty());
					ids.integer(4);
					if (same_bytes(value,expected) && matched==~0u) matched=count;
					++count;
				}
				auto truncated{message.first(static_cast<::std::size_t>(e.bytes.data()-message.data()))};
				auto binders{e.vector(2)};
				for (unsigned i{};i!=count;++i)
				{
					auto value{binders.vector(1).bytes};tls_wire_check(value.size()>=32);
					if (i==matched) { tls_wire_check(tls_auth_equal(binder(truncated),value),errc::authentication);selected=i;psk=true; }
				}
				binders.done();break;
			}
			default:e.take(e.bytes.size());
			}
		});
		tls_wire_check(psk,errc::peer_identity);tls_wire_check(version&&group&&share&&mode);
	}
	/** @brief Encodes the selected suite, TLS version, X25519 share and bound PSK index. */
	tls_buffer server_hello()
	{
		tls_buffer hello{::std::byte{2}};
		auto share{ephemeral.public_key()};
		tls_vector(hello,3,[&](auto &v){
			random_hello(v);tls_string(v,session_id,1);tls_uint(v,0x1301,2);tls_uint(v,0,1);
			tls_vector(v,2,[&](auto &ext){
				tls_extension(ext,43,[](auto &e){tls_uint(e,0x0304,2);});
				tls_extension(ext,51,[&](auto &e){tls_uint(e,29,2);tls_string(e,share,2);});
				tls_extension(ext,41,[&](auto &e){tls_uint(e,selected,2);});
			});
		});
		return hello;
	}
	/** @brief Validates that every server-selected parameter was offered and captures its exact X25519 share. */
	void accept_server(bytes_view message) FAST_IO_HERBCEPTIONS_THROWS
	{
		tls_cursor c{message.subspan(4)};
		tls_wire_check(c.integer(2)==0x0303);
		c.take(32);
		tls_wire_check(c.vector(1).bytes.empty() && c.integer(2)==0x1301 && c.integer(1)==0);
		bool version{},share{},psk{};
		tls_extensions(c,[&](unsigned type,tls_cursor &e,bool) FAST_IO_HERBCEPTIONS_THROWS {
			switch(type)
			{
			case 43: version=e.integer(2)==0x0304;break;
			case 51:
			{
				tls_wire_check(e.integer(2)==29);
				auto key{e.vector(2).bytes};tls_wire_check(key.size()==32);::std::copy(key.begin(),key.end(),peer.begin());share=true;break;
			}
			case 41: psk=e.integer(2)==0;break;
			default:tls_wire_fail(errc::tls_protocol);
			}
		});
		tls_wire_check(version&&share&&psk);
	}
	/** @brief Mixes the authenticated external PSK and fresh X25519 secret into the handshake traffic keys. */
	void derive_handshake() FAST_IO_HERBCEPTIONS_THROWS
	{
		private_bytes<32> dh,derived,handshake;
		tls_wire_check(ephemeral.try_agree(peer,dh.value),errc::peer_identity);
		tls_expand(early.value,label_bytes(u8"derived"),empty_hash,derived.value);
		sha256_hmac(derived.value,handshake.value.data(),dh.value);
		auto digest{tls_digest(transcript)};
		tls_expand(handshake.value,label_bytes(u8"c hs traffic"),digest,client_handshake.value);
		tls_expand(handshake.value,label_bytes(u8"s hs traffic"),digest,server_handshake.value);
		tls_expand(handshake.value,label_bytes(u8"derived"),empty_hash,derived.value);
		constexpr ::std::byte zero[32]{};
		sha256_hmac(derived.value,master.value.data(),zero);
		early.clear();
	}
	/** @brief Derives both application traffic secrets from the transcript through the server Finished and erases the master secret. */
	void derive_application()
	{
		auto digest{tls_digest(transcript)};
		tls_expand(master.value,label_bytes(u8"c ap traffic"),digest,client_app.value);
		tls_expand(master.value,label_bytes(u8"s ap traffic"),digest,server_app.value);
		master.clear();
	}
};
} // namespace fast_io::secure::details

#include "../fast_io_dsal/impl/misc/pop_macros.h"
