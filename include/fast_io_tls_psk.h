#pragma once

/**
 * @file
 * @brief TLS 1.3 byte streams authenticated by an external pairwise PSK.
 * @details This profile uses X25519 and TLS_AES_128_GCM_SHA256. Give each pair of
 * peers a uniformly generated 32-byte key, a distinct identity and an application
 * context dedicated to TLS 1.3. Possession of that key authenticates either peer.
 * A group key would let every member impersonate every other member. The
 * transcript binder and both Finished MACs cover the negotiated handshake.
 * Application plaintext is released only after its record tag verifies.
 * @see RFC 9846, sections 4.3.11, 4.5.4, 4.7.3, 5 and 7. RFC 9257, sections 4 and 5.
 */
#include "fast_io_protocol.h"
#if defined(__cpp_impl_coroutine) && __has_include(<coroutine>)
#include "fast_io_protocol_async.h"
#endif
#include "fast_io_secure/tls13_psk_hello.h"
#include "fast_io_secure/deferred_protocol.h"
#include <memory>

#include "fast_io_dsal/impl/misc/push_macros.h"

namespace fast_io::secure
{
/** @brief Borrows a binary PSK identity and its secret until the client or server descriptor is constructed. */
struct tls13_external_psk
{
	::std::span<::std::byte const> identity;
	::std::span<::std::byte const,32> key;
};

namespace details
{
/** @brief Drives the TLS handshake and enforces directional record boundaries and traffic-key epochs. */
class tls_psk_connection
{
	enum class phase { initial, client_hello, server_hello, extensions, server_finished, client_finished, finished_flight, established };
	static constexpr ::std::size_t record_capacity{16384u + 256u}, header_size{5};
	phase next{phase::initial};
	bool server, received_close{}, sent_close{}, eof{}, peer_alert{}, checking_record_tag{};
	tls_buffer identity, outgoing, fragments;
	::std::size_t sent{}, filled{}, plaintext{}, plaintext_end{};
	::std::array<::std::byte,header_size + record_capacity> incoming{};
	::std::optional<tls_psk_handshake> hs;
	tls13_traffic rx,tx;
	tls13_send_updates updates;
	::fast_io::error failure{};

	/** @brief Returns the declared record length once its five-byte header is complete. */
	::std::size_t extent() const noexcept
	{
		if (filled < header_size) return header_size;
		unsigned length{static_cast<unsigned>(incoming[3]) * 256u + static_cast<unsigned>(incoming[4])};
		return header_size + length;
	}

	/** @brief Queues one record, authenticating its header and encrypting its inner content type. */
	void record(unsigned type, bytes_view content) FAST_IO_HERBCEPTIONS_THROWS
	{
		tls_wire_check(content.size() <= 16384u);
		bool encrypted{tx.cipher.has_value()};
		// Refuse an exhausted epoch before publishing even the length of an unencrypted staging record.
		if (encrypted) tls_wire_check(tx.sequence < tls13_traffic::record_limit,errc::usage_limit);
		auto start{outgoing.size()};
		auto length{content.size() + (encrypted ? 17u : 0u)};
		outgoing.resize(start + header_size + length);
		auto *p{outgoing.data() + start};
		p[0] = static_cast<::std::byte>(encrypted ? 23u : type);
		p[1] = p[2] = ::std::byte{3};
		p[3] = static_cast<::std::byte>(length >> 8u);
		p[4] = static_cast<::std::byte>(length & 255u);
		if (!content.empty()) ::fast_io::freestanding::my_memcpy(p + header_size,content.data(),content.size());
		if (encrypted)
		{
			p[header_size+content.size()] = static_cast<::std::byte>(type);
			tx.protect({p+header_size,content.size()+1u},{p,header_size},
				reinterpret_cast<unsigned char *>(p+header_size+content.size()+1u));
		}
	}

	/** @brief Hashes a complete outgoing handshake message, then fragments it into records of one epoch. */
	void send_message(bytes_view message) FAST_IO_HERBCEPTIONS_THROWS
	{
		hash_part(hs->transcript,message);
		while (!message.empty())
		{
			auto n{::std::min<::std::size_t>(message.size(),16384u)};
			record(22,message.first(n));
			message = message.subspan(n);
		}
	}

	/** @brief Binds freshly derived directional handshake keys to sequence number zero. */
	void install_handshake() FAST_IO_HERBCEPTIONS_THROWS
	{
		hs->derive_handshake();
		rx.install(server ? hs->client_handshake.value : hs->server_handshake.value);
		tx.install(server ? hs->server_handshake.value : hs->client_handshake.value);
	}

	/** @brief Computes Finished over the transcript preceding this message, including EncryptedExtensions. */
	void send_finished(bytes_view secret) FAST_IO_HERBCEPTIONS_THROWS
	{
		auto mac{tls_finished(secret,tls_digest(hs->transcript))};
		tls_buffer message{::std::byte{20},::std::byte{},::std::byte{},::std::byte{32}};
		tls_append(message,mac);
		send_message(message);
	}

	/** @brief Processes a complete handshake message, checking the record boundary at every change of keys. */
	void handshake_message(bytes_view message, bool boundary) FAST_IO_HERBCEPTIONS_THROWS
	{
		auto type{static_cast<unsigned>(message[0])};
		auto body{message.subspan(4)};
		switch (next)
		{
		case phase::client_hello:
			tls_wire_check(type==1 && boundary);
			hs->accept_client(message,identity);
			hash_part(hs->transcript,message);
			send_message(hs->server_hello());
			install_handshake();
			{
				// This external-PSK profile negotiates no application extensions.
				constexpr ::std::byte ext[]{::std::byte{8},{},{},::std::byte{2},{},{}};
				send_message(ext);
			}
			send_finished(hs->server_handshake.value);
			hs->derive_application();
			tx.install(hs->server_app.value);
			next=phase::client_finished;
			return;
		case phase::server_hello:
			tls_wire_check(type==2 && boundary);
			hs->accept_server(message);
			hash_part(hs->transcript,message);
			install_handshake();
			next=phase::extensions;
			return;
		case phase::extensions:
		{
			tls_wire_check(type==8);
			tls_cursor extensions{body};
			tls_extensions(extensions,[](unsigned extension,tls_cursor &item,bool) FAST_IO_HERBCEPTIONS_THROWS {
				tls_wire_check(extension==10);
				(void)tls_contains(item.vector(2),29,2);
			});
			hash_part(hs->transcript,message);
			next=phase::server_finished;
			return;
		}
		case phase::server_finished:
		case phase::client_finished:
		{
			tls_wire_check(type==20 && boundary);
			auto const &peer{server ? hs->client_handshake.value : hs->server_handshake.value};
			tls_wire_check(tls_auth_equal(tls_finished(peer,tls_digest(hs->transcript)),body),errc::authentication);
			hash_part(hs->transcript,message);
			if (server) rx.install(hs->client_app.value);
			else
			{
				hs->derive_application();
				rx.install(hs->server_app.value);
				send_finished(hs->client_handshake.value);
				tx.install(hs->client_app.value);
			}
			hs.reset();
			next=server ? phase::established : phase::finished_flight;
			return;
		}
		case phase::established:
			post_handshake(type,body,boundary);
			return;
		default: tls_wire_fail(errc::tls_protocol);
		}
	}

	/** @brief Advances receive keys in wire order and silently discards tickets for this external-PSK-only profile. */
	void post_handshake(unsigned type,bytes_view body,bool boundary) FAST_IO_HERBCEPTIONS_THROWS
	{
		if (type==24)
		{
			tls_wire_check(boundary);
			tls_wire_check(body.size()==1,errc::tls_decode);
			tls_wire_check(static_cast<unsigned>(body[0])<=1,errc::tls_illegal_parameter);
			rx.update();
			updates.requested |= body[0]==::std::byte{1};
			return;
		}
		tls_wire_check(!server && type==4);
		// RFC 9846 section 4.7.1 requires clients without resumption to ignore NewSessionTicket.
	}

	/** @brief Reassembles handshake headers across records and bounds memory before accepting message bodies. */
	void handshake_record(bytes_view part) FAST_IO_HERBCEPTIONS_THROWS
	{
		tls_wire_check(!part.empty());
		tls_append(fragments,part);
		bytes_view pending{fragments};
		while (pending.size()>=4)
		{
			auto n{static_cast<unsigned>(pending[1])*65536u+static_cast<unsigned>(pending[2])*256u+static_cast<unsigned>(pending[3])+4u};
			// A ClientHello may contain two full 16-bit vectors. Tickets carry a 16-bit opaque value and extensions.
			tls_wire_check(n<=196608u);
			if (pending.size()<n) break;
			handshake_message(pending.first(n),pending.size()==n);
			pending=pending.subspan(n);
		}
		auto consumed{fragments.size()-pending.size()};
		if (consumed) fragments.erase(fragments.begin(),fragments.begin()+static_cast<::std::ptrdiff_t>(consumed));
	}

	/** @brief Verifies the ciphertext before inspecting padding, the inner type, or any application bytes. */
	void consume_record() FAST_IO_HERBCEPTIONS_THROWS
	{
		unsigned type{static_cast<unsigned>(incoming[0])};
		::std::span<::std::byte> payload{incoming.data()+header_size,filled-header_size};
		if (type==20)
		{
			// Compatibility CCS is confined to the interval between the first ClientHello and the peer Finished.
			tls_wire_check(next!=phase::initial && next!=phase::client_hello && next!=phase::established &&
				payload.size()==1 && payload[0]==::std::byte{1});
			return;
		}
		if (rx.cipher)
		{
			tls_wire_check(type==23 && incoming[1]==::std::byte{3} && incoming[2]==::std::byte{3} && payload.size()>16);
			auto data{payload.first(payload.size()-16u)};
			checking_record_tag=true;
			rx.unprotect(data,{incoming.data(),header_size},reinterpret_cast<unsigned char const *>(data.data()+data.size()));
			checking_record_tag=false;
			tls_wire_check(data.size()<=16385u,errc::tls_record_overflow);
			while (!data.empty() && data.back()==::std::byte{}) data=data.first(data.size()-1u);
			tls_wire_check(!data.empty());
			type=static_cast<unsigned>(data.back());
			payload=data.first(data.size()-1u);
		}
		else tls_wire_check(type==22 || type==21);
		if (type==22) handshake_record(payload);
		else if (type==23)
		{
			tls_wire_check(next==phase::established && fragments.empty());
			plaintext=header_size;
			plaintext_end=header_size+payload.size();
		}
		else
		{
			tls_wire_check(type==21 && fragments.empty() && payload.size()==2u);
			auto code{static_cast<unsigned>(payload[1])};
			if (code!=0 && code!=90) { peer_alert=true; tls_wire_fail(errc::tls_protocol); }
			if (code==0)
			{
				tls_wire_check(next==phase::established);
				received_close=true;
			}
		}
	}

	/** @brief Requests only the remaining header or body prefix without reading into an outstanding plaintext view. */
	protocol_result receive() FAST_IO_HERBCEPTIONS_THROWS
	{
		auto size{extent()};
		// Validate the peer length in the step, where an error can be turned into a permanent protocol result.
		tls_wire_check(size <= header_size + (incoming[0]==::std::byte{23} ? record_capacity : 16384u),errc::tls_record_overflow);
		if (filled!=size)
		{
			tls_wire_check(!eof,errc::invalid_message);
			return {protocol_status::need_input,0};
		}
		consume_record();
		filled=0;
		return {protocol_status::retry,0};
	}

public:
	/** @brief Captures a TLS 1.3 PSK identity that fits the identities and extensions vectors. */
	static tls_buffer capture_identity(bytes_view id)
	{
		check(!id.empty() && id.size()<=65400u,errc::configuration);
		return {id.begin(),id.end()};
	}
	/** @brief Takes the captured identity without copying and extracts the early key when the connection actually opens. */
	tls_psk_connection(bool role,tls_buffer captured,::std::span<::std::byte const,32> key) :
		server(role), identity(::std::move(captured)), hs(::std::in_place,key) {}
	/** @brief Erases the last decrypted record when the session is released. */
	~tls_psk_connection() { ::fast_io::secure_clear(incoming.data(),incoming.size()); }

	/** @brief Includes the client's last Finished flight until even a partial wire write has drained it. */
	[[nodiscard]] bool handshake_pending() const noexcept { return next!=phase::established; }

	/** @brief Provides the smallest exact receive window for the current record boundary. */
	basic_protocol_window<::std::byte> input_window() noexcept
	{
		if (failure.domain) return {};
		return {incoming.data()+filled,(::std::min)(extent(),incoming.size())-filled};
	}
	/** @brief Commits a short read or records physical EOF until authentication decides its disposition. */
	void input_commit(::std::size_t n) { tls_check(n<=(::std::min)(extent(),incoming.size())-filled); filled+=n; eof|=!n; }
	/** @brief Exposes the contiguous unsent ciphertext prefix, including complete handshake flights. */
	auto output_window() const noexcept
	{ return basic_protocol_window<::std::byte const>{outgoing.empty()?nullptr:outgoing.data()+sent,outgoing.size()-sent}; }
	/** @brief Releases the reported wire prefix only after the transport has accepted it. */
	void output_consume(::std::size_t n)
	{
		tls_check(n<=outgoing.size()-sent);
		sent+=n;
		if (sent==outgoing.size())
		{
			sent=0;
			outgoing.clear();
			if (next==phase::finished_flight) next=phase::established;
		}
	}
	/** @brief Queues a fatal alert at the active write epoch and delivers the original failure to the caller. */
	protocol_result reject(::fast_io::error error)
	{
		if (failure.domain) return {protocol_status::failed,0,failure};
		failure=error;
		if (!peer_alert && !eof && !sent_close && tx.sequence < tls13_traffic::record_limit)
		{
			unsigned number{10};
			if (error==errc::authentication) number=checking_record_tag ? 20u : 51u;
			else if (error==errc::peer_identity) number=115;
			else if (error==errc::tls_record_overflow) number=22;
			else if (error==errc::tls_illegal_parameter) number=47;
			else if (error==errc::tls_decode) number=50;
			::std::byte fatal[]{::std::byte{2},static_cast<::std::byte>(number)};
#if defined(__HERBCEPTIONS__)
			// Preserve the original peer error when an alert itself cannot be produced.
			try { record(21,fatal); }
			catch throws(::std::error) {}
#else
			record(21,fatal);
#endif
		}
		return {protocol_status::failed,0,error};
	}
	/** @brief Processes one operation, preserving key and cursor state across partial transport completions. */
	protocol_result step(protocol_request request) FAST_IO_HERBCEPTIONS_THROWS
	{
		if (failure.domain) return {protocol_status::failed,0,failure};
		if (next==phase::initial)
		{
			if (server) next=phase::client_hello;
			else { send_message(hs->client_hello(identity)); next=phase::server_hello; }
		}
		if (next==phase::finished_flight) return {protocol_status::need_output,0};
		if (next!=phase::established) return receive();
		switch (request.action)
		{
		case protocol_action::handshake: return {protocol_status::complete,0};
		case protocol_action::write:
		{
			tls_wire_check(!sent_close);
			auto n{::std::min<::std::size_t>(request.size,16384u)};
			if (!n) return {protocol_status::complete,0};
			if (updates.take(tx.sequence+1u==tls13_traffic::record_limit))
			{
				// Both a local rotation and the coalesced reply use the old key and request no response.
				constexpr ::std::byte update[]{::std::byte{24},{},{},::std::byte{1},{}};
				record(22,update);
				tx.update();
			}
			record(23,{request.source,n});
			return {protocol_status::complete,n};
		}
		case protocol_action::read:
			if (auto n{::std::min(request.size,plaintext_end-plaintext)})
			{
				::fast_io::freestanding::my_memcpy(request.destination,incoming.data()+plaintext,n);
				plaintext+=n;
				return {protocol_status::complete,n};
			}
			if (received_close || !request.size) return {protocol_status::complete,0};
			return receive();
		case protocol_action::close_write:
		case protocol_action::shutdown:
			if (!sent_close)
			{
				constexpr ::std::byte close[]{::std::byte{1},{}};
				record(21,close);
				sent_close=true;
			}
			if (request.action==protocol_action::close_write || received_close) return {protocol_status::complete,0};
			// Full shutdown consumes any final application records before waiting for the peer alert.
			plaintext=plaintext_end;
			return receive();
		}
		tls_wire_fail(errc::tls_protocol);
	}
};
} // namespace details

/** @brief Adapts independent TLS record and handshake steps to the generic duplex protocol concept. */
class tls13_psk_engine
{
	::std::unique_ptr<details::tls_psk_connection> state;
public:
	/** @brief Allocates a stable record workspace once for this connection. */
	tls13_psk_engine(bool server,tls13_external_psk config) :
		state{::std::make_unique<details::tls_psk_connection>(server,details::tls_psk_connection::capture_identity(config.identity),config.key)} {}

	/** @brief Captures a public identity and an independently erasable key without starting X25519, HKDF or record allocation. */
	class parameters
	{
		bool server;
		details::tls_buffer identity;
		details::private_bytes<32> secret;
	public:
		using engine_type = tls13_psk_engine;
		/** @brief Validates identity bounds immediately and copies the caller's key before its lifetime can end. */
		parameters(bool role,tls13_external_psk config)
			: server(role), identity(details::tls_psk_connection::capture_identity(config.identity))
		{
			::fast_io::freestanding::my_memcpy(secret.value.data(),config.key.data(),secret.value.size());
		}
		/** @brief Opens the role once with a fresh ephemeral private key and transfers ownership of the identity. */
		[[nodiscard]] details::protocol_open_result<engine_type> open()
		{
			return {engine_type{::std::make_unique<details::tls_psk_connection>(server,::std::move(identity),secret.value)}};
		}
	};
private:
	/** @brief Adopts the connection prepared from a captured descriptor without reinitializing any traffic secrets. */
	explicit tls13_psk_engine(::std::unique_ptr<details::tls_psk_connection> connection) noexcept : state(::std::move(connection)) {}
public:
	/** @brief Advertises the TLS plaintext fragment limit as the transform batch size. */
	friend constexpr transcode_buffer_sizes transcode_preferred_buffer_sizes_define(transcode_reserve_t<tls13_psk_engine>) noexcept
	{ return {16384u,16384u}; }
	/** @brief Carries the TLS plaintext-erasure policy through either direction of nested transform storage. */
	friend constexpr ::std::true_type transcode_sensitive_define(io_type_t<tls13_psk_engine>) noexcept { return {}; }
	/** @brief Reports the initial handshake's local transport obligation without decrypting or consuming anything. */
	friend bool protocol_handshake_pending_define(tls13_psk_engine const &p) noexcept { return p.state->handshake_pending(); }
	/** @brief Lends space for the next piece of an incoming record. */
	friend auto protocol_input_window_define(tls13_psk_engine &p) { return p.state->input_window(); }
	/** @brief Records the number of ciphertext bytes actually read. */
	friend void protocol_input_commit_define(tls13_psk_engine &p,::std::size_t n) { p.state->input_commit(n); }
	/** @brief Lends pending wire bytes through their physical send completion. */
	friend auto protocol_output_window_define(tls13_psk_engine &p) { return p.state->output_window(); }
	/** @brief Advances the ciphertext send cursor through the accepted prefix. */
	friend void protocol_output_consume_define(tls13_psk_engine &p,::std::size_t n) { p.state->output_consume(n); }
	/** @brief Advances the selected TLS operation by one provider step. */
	friend protocol_result protocol_step_define(tls13_psk_engine &p,protocol_request r)
	{
#if defined(__HERBCEPTIONS__)
		try { return p.state->step(r); }
		catch throws(::std::error reason)
		{
			auto code{reason.is_code_of<errc>() ? static_cast<errc>(reason.code()) : errc::provider};
			return p.state->reject({domain_define(::fast_io::error_type<errc>),static_cast<::std::size_t>(code)});
		}
#elif defined(__cpp_exceptions) || defined(_CPPUNWIND)
		try { return p.state->step(r); }
		catch (::fast_io::error error) { return p.state->reject(error); }
#else
		return p.state->step(r);
#endif
	}
};

/** @brief Authenticates a server by its separately provisioned PSK and an ephemeral X25519 exchange. */
template <duplex_protocol_transport Handle>
inline auto tls_client(Handle &&wire,tls13_external_psk config)
{
	return ::fast_io::make_iotranscoder(static_cast<Handle &&>(wire),details::deferred_protocol_engine{tls13_psk_engine::parameters{false,config}});
}
/** @brief Authenticates a client's external PSK identity and binder before producing the server flight. */
template <duplex_protocol_transport Handle>
inline auto tls_server(Handle &&wire,tls13_external_psk config)
{
	return ::fast_io::make_iotranscoder(static_cast<Handle &&>(wire),details::deferred_protocol_engine{tls13_psk_engine::parameters{true,config}});
}
} // namespace fast_io::secure

#include "fast_io_dsal/impl/misc/pop_macros.h"
