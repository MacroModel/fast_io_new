#pragma once
/**
 * @file
 * @brief Authenticated TLS 1.3 streams over arbitrary ordered duplex byte transports.
 * @details OpenSSL owns transcript authentication, key exchange, AEAD, key
 * updates and alert processing. Peer verification is mandatory on clients.
 * Include explicitly and link libssl and libcrypto when using this provider.
 */
#include "fast_io_protocol.h"
#if defined(__cpp_impl_coroutine) && __has_include(<coroutine>)
#include "fast_io_protocol_async.h"
#endif
#include "fast_io_secure/openssl_common.h"
#include "fast_io_secure/deferred_protocol.h"
#include <string>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

namespace fast_io::secure
{

/** @brief Describes a PEM certificate chain and its matching private key. */
struct tls_credentials
{
	char const *chain_file;
	char const *private_key_file;
};

namespace details
{
using tls_context_owner = owner<SSL_CTX, SSL_CTX_free>;

/** @brief Configures a TLS 1.3 context without early data or unauthenticated EOF. */
inline tls_context_owner tls_context(bool server)
{
	tls_context_owner ctx{SSL_CTX_new(server ? TLS_server_method() : TLS_client_method())};
	check(ctx && SSL_CTX_set_min_proto_version(ctx.get(), TLS1_3_VERSION) == 1 &&
			  SSL_CTX_set_max_proto_version(ctx.get(), TLS1_3_VERSION) == 1 &&
			  SSL_CTX_set_max_early_data(ctx.get(), 0) == 1,
		  errc::configuration);
	if (SSL_CTX_get_security_level(ctx.get()) < 2)
	{
		SSL_CTX_set_security_level(ctx.get(), 2);
	}
	SSL_CTX_clear_options(ctx.get(), SSL_OP_IGNORE_UNEXPECTED_EOF);
	SSL_CTX_set_quiet_shutdown(ctx.get(), 0);
	return ctx;
}

/** @brief Accepts a single DNS reference identity in ASCII A-label form, never a suffix or wildcard pattern. */
inline bool valid_dns_name(char const *name) noexcept
{
	unsigned total{}, label{};
	bool hyphen{};
	for (auto p = name; *p; ++p)
	{
		if (++total > 253)
		{
			return false;
		}
		if (*p == '.')
		{
			if (!label || hyphen)
			{
				return false;
			}
			label = 0;
			continue;
		}
		hyphen = *p == '-';
		if ((hyphen && !label) || ++label > 63)
		{
			return false;
		}
		if (!hyphen && !('A' <= *p && *p <= 'Z') && !('a' <= *p && *p <= 'z') && !('0' <= *p && *p <= '9'))
		{
			return false;
		}
	}
	return label && !hyphen;
}

/** @brief Loads explicitly selected trust anchors or the provider's system trust store. */
inline void trust(SSL_CTX *ctx, char const *ca_file)
{
	check((ca_file ? SSL_CTX_load_verify_locations(ctx, ca_file, nullptr) : SSL_CTX_set_default_verify_paths(ctx)) == 1, errc::configuration);
}

/** @brief Loads and matches the full certificate chain and the locally held key. */
inline void identity(SSL_CTX *ctx, tls_credentials cred)
{
	check(cred.chain_file && cred.private_key_file &&
			  SSL_CTX_use_certificate_chain_file(ctx, cred.chain_file) == 1 &&
			  SSL_CTX_use_PrivateKey_file(ctx, cred.private_key_file, SSL_FILETYPE_PEM) == 1 &&
			  SSL_CTX_check_private_key(ctx) == 1,
		  errc::configuration);
}
} // namespace details

/** @brief Holds immutable client trust settings shared by independently keyed TLS connections. */
class tls_client_context
{
	details::tls_context_owner ctx{details::tls_context(false)};
	friend class tls_engine;

public:
	/** @brief Enables chain and peer name checking, with optional mutual TLS credentials. */
	explicit tls_client_context(char const *ca_file = nullptr, tls_credentials const *client = nullptr)
	{
		details::trust(ctx.get(), ca_file);
		SSL_CTX_set_verify(ctx.get(), SSL_VERIFY_PEER, nullptr);
		if (client)
		{
			details::identity(ctx.get(), *client);
		}
	}
};

/** @brief Holds a server identity and optionally a mandatory client-certificate trust store. */
class tls_server_context
{
	details::tls_context_owner ctx{details::tls_context(true)};
	friend class tls_engine;

public:
	/** @brief Presents the server chain. A client CA file requires a verified client certificate. */
	explicit tls_server_context(tls_credentials server, char const *client_ca_file = nullptr)
	{
		details::identity(ctx.get(), server);
		if (client_ca_file)
		{
			details::trust(ctx.get(), client_ca_file);
			SSL_CTX_set_verify(ctx.get(), SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
		}
	}
};

/**
 * @brief Drives exactly one OpenSSL connection through a bounded, zero-copy BIO pair.
 * @details The peer identity is verified during the handshake before any read
 * can publish plaintext. Physical EOF is delivered to OpenSSL and is accepted
 * as application EOF only following an authenticated close_notify.
 */
class tls_engine
{
	details::owner<BIO, BIO_free> network;
	details::owner<SSL, SSL_free> ssl;
	errc failure{};
	enum class initial_handshake : unsigned char { negotiating, flushing, delivered };
	initial_handshake handshake{initial_handshake::negotiating};
	bool sent_close{}, closed{};

	enum class uninitialized { connection };
	explicit tls_engine(uninitialized) noexcept {}

	/** @brief Acquires both BIO endpoints before publishing them to SSL, returning setup failures in every exception mode. */
	[[nodiscard]] errc initialize(SSL_CTX *ctx, ::std::size_t capacity) noexcept
	{
		if (!ctx || !capacity || capacity>0x7fffffff) return errc::configuration;
		ssl.reset(SSL_new(ctx));
		if (!ssl) return errc::configuration;
		BIO *internal{}, *external{};
		if (BIO_new_bio_pair(&internal,capacity,&external,capacity)!=1)
		{
			// OpenSSL releases both buffers and returns two null endpoints on failure.
			return errc::provider;
		}
		network.reset(external);
		SSL_set_bio(ssl.get(),internal,internal);
		return {};
	}

	/** @brief Supplies the existing immediate-construction interface for callers using the raw SSL engine. */
	explicit tls_engine(SSL_CTX *ctx, ::std::size_t capacity)
	{
		auto error{initialize(ctx,capacity)};
		details::check(error==errc{},error);
	}

	/** @brief Installs the validated peer identity, using IP SAN or DNS SAN with SNI as appropriate. */
	[[nodiscard]] bool configure_client(char const *dns, ASN1_OCTET_STRING const *ip) noexcept
	{
		auto *params{SSL_get0_param(ssl.get())};
		X509_VERIFY_PARAM_set_hostflags(params,X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS | X509_CHECK_FLAG_NEVER_CHECK_SUBJECT);
		if (ip)
		{
			if (X509_VERIFY_PARAM_set1_ip(params,ASN1_STRING_get0_data(ip),
						static_cast<::std::size_t>(ASN1_STRING_length(ip)))!=1) return false;
		}
		else if (SSL_set1_host(ssl.get(),dns)!=1 || SSL_set_tlsext_host_name(ssl.get(),dns)!=1) return false;
		SSL_set_connect_state(ssl.get());
		return true;
	}

public:
	/**
	 * @brief Captures an immutable certificate configuration and a validated peer identity.
	 * @details The context reference and DNS name or binary IP remain live if the caller destroys
	 * the original objects. No SSL connection, BIO pair or TLS session key is created here.
	 */
	class parameters
	{
		details::tls_context_owner context;
		details::owner<ASN1_OCTET_STRING, ASN1_OCTET_STRING_free> ip;
		::std::string dns;
		bool server;

		/** @brief Acquires a provider reference before the caller may release the configured trust or server identity. */
		static details::tls_context_owner retain(SSL_CTX *ctx)
		{
			details::check(ctx && SSL_CTX_up_ref(ctx) == 1, errc::configuration);
			return details::tls_context_owner{ctx};
		}
	public:
		using engine_type = tls_engine;

		/** @brief Rejects an invalid DNS name at capture and owns the binary identity for IP certificate verification. */
		parameters(tls_client_context const &configured,char const *peer) : context(retain(configured.ctx.get())), server(false)
		{
			details::check(peer && *peer,errc::configuration);
			ip.reset(a2i_IPADDRESS(peer));
			if (!ip)
			{
				details::check(details::valid_dns_name(peer),errc::configuration);
				dns.assign(peer);
			}
		}
		/** @brief Retains the server role and its configured identity without allocating any wire buffers. */
		explicit parameters(tls_server_context const &configured) : context(retain(configured.ctx.get())), server(true) {}
		/** @brief Opens a fresh SSL/BIO pair at first use and transfers the captured role to the connection. */
		[[nodiscard]] details::protocol_open_result<engine_type> open() const noexcept
		{
			engine_type result{uninitialized::connection};
			if (auto error{result.initialize(context.get(),17408)}; error!=errc{}) return {{},error};
			if (server) SSL_set_accept_state(result.ssl.get());
			else if (!result.configure_client(dns.c_str(),ip.get())) return {{},errc::configuration};
			return {::std::move(result)};
		}
	};

	/**
	 * @brief Starts a client for an expected DNS name or IP address in the peer's SAN.
	 * @details Primitive engine construction immediately provides its wire windows. Public stream
	 * factories capture the same configuration and initialize those windows at the first I/O step.
	 */
	tls_engine(tls_client_context const &context, char const *peer, ::std::size_t capacity = 17408)
		: tls_engine(context.ctx.get(), capacity)
	{
		details::check(peer && *peer, errc::configuration);
		details::owner<ASN1_OCTET_STRING, ASN1_OCTET_STRING_free> ip{a2i_IPADDRESS(peer)};
		details::check(ip || details::valid_dns_name(peer),errc::configuration);
		details::check(configure_client(peer,ip.get()),errc::configuration);
	}

	/** @brief Starts a server over a bounded transport buffer. SSL retains the context. */
	explicit tls_engine(tls_server_context const &context, ::std::size_t capacity = 17408)
		: tls_engine(context.ctx.get(), capacity)
	{
		SSL_set_accept_state(ssl.get());
	}

	/** @brief Advertises the maximum plaintext TLS record for both application directions. */
	friend constexpr transcode_buffer_sizes transcode_preferred_buffer_sizes_define(transcode_reserve_t<tls_engine>) noexcept
	{
		return {SSL3_RT_MAX_PLAIN_LENGTH, SSL3_RT_MAX_PLAIN_LENGTH};
	}
	/** @brief Requests erasure of outer transform buffers holding application data at the TLS boundary. */
	friend constexpr ::std::true_type transcode_sensitive_define(io_type_t<tls_engine>) noexcept { return {}; }

	/** @brief Reports whether initial negotiation or delivery of its final wire bytes is still outstanding. */
	friend bool protocol_handshake_pending_define(tls_engine const &e) noexcept
	{
		return e.handshake != initial_handshake::delivered;
	}

	/** @brief Exposes the next contiguous ciphertext prefix, including handshake messages and alerts. */
	friend basic_protocol_window<::std::byte const> protocol_output_window_define(tls_engine &e)
	{
		char *data{};
		int n = BIO_nread0(e.network.get(), &data);
		details::check(n >= -1);
		return {reinterpret_cast<::std::byte const *>(data), n > 0 ? static_cast<::std::size_t>(n) : 0u};
	}

	/** @brief Releases only the ciphertext prefix that actually reached the wire. */
	friend void protocol_output_consume_define(tls_engine &e, ::std::size_t n)
	{
		char *data{};
		details::check(n <= 0x7fffffff && BIO_nread(e.network.get(), &data, static_cast<int>(n)) == static_cast<int>(n));
		if (e.handshake == initial_handshake::flushing && BIO_ctrl_pending(e.network.get()) == 0)
		{
			e.handshake = initial_handshake::delivered;
		}
	}

	/** @brief Borrows uncommitted storage into which the transport can read directly. */
	friend basic_protocol_window<::std::byte> protocol_input_window_define(tls_engine &e)
	{
		char *data{};
		int n = BIO_nwrite0(e.network.get(), &data);
		details::check(n > 0);
		return {reinterpret_cast<::std::byte *>(data), static_cast<::std::size_t>(n)};
	}

	/** @brief Commits received ciphertext, or signals physical EOF without synthesizing a TLS alert. */
	friend void protocol_input_commit_define(tls_engine &e, ::std::size_t n)
	{
		if (!n)
		{
			details::check(BIO_shutdown_wr(e.network.get()) == 1);
			return;
		}
		char *data{};
		details::check(n <= 0x7fffffff && BIO_nwrite(e.network.get(), &data, static_cast<int>(n)) == static_cast<int>(n));
	}

	/** @brief Advances one TLS operation while preserving OpenSSL retry arguments across wire waits. */
	friend protocol_result protocol_step_define(tls_engine &e, protocol_request r)
	{
		auto rejected = [&](errc code) {
			e.failure = code;
			return protocol_result{protocol_status::failed, 0, {domain_define(::fast_io::error_type<errc>), static_cast<::std::size_t>(code)}};
		};
		if (e.failure != errc{})
		{
			return rejected(e.failure);
		}
		auto *s = e.ssl.get();
		auto n = r.size < 16384u ? r.size : 16384u;
		::std::size_t count{};
		int rc{};
		bool closing{};
		if (e.handshake == initial_handshake::flushing) return {protocol_status::need_output, 0};
		if (e.handshake == initial_handshake::negotiating && !n &&
			(r.action == protocol_action::read || r.action == protocol_action::write)) return {};
		// SSL_get_error interprets this thread's error queue and the immediately preceding call.
		ERR_clear_error();
		switch (e.handshake == initial_handshake::negotiating ? protocol_action::handshake : r.action)
		{
		case protocol_action::handshake:
			if (e.handshake == initial_handshake::delivered) return {};
			rc = SSL_do_handshake(s);
			if (rc == 1)
			{
				// Separate the last handshake flight from later application or post-handshake records.
				e.handshake = BIO_ctrl_pending(e.network.get()) ? initial_handshake::flushing : initial_handshake::delivered;
				return {r.action == protocol_action::handshake ? protocol_status::complete : protocol_status::retry, 0};
			}
			break;
		case protocol_action::read:
			if (e.closed || !n)
			{
				return {};
			}
			rc = SSL_read_ex(s, r.destination, n, &count);
			break;
		case protocol_action::write:
			if (e.sent_close || e.closed)
			{
				return rejected(errc::tls_protocol);
			}
			if (!n)
			{
				return {};
			}
			rc = SSL_write_ex(s, r.source, n, &count);
			break;
		case protocol_action::close_write:
		case protocol_action::shutdown:
			if (e.closed || (r.action == protocol_action::close_write && e.sent_close))
			{
				return {};
			}
			closing = true;
			rc = SSL_shutdown(s);
			if (rc == 1)
			{
				e.sent_close = e.closed = true;
				return {};
			}
			break;
		}
		if (rc == 1)
		{
			return {protocol_status::complete, count};
		}
		// Zero from SSL_shutdown means progress. Inspect its retry direction without
		// interpreting zero as a fatal SSL_get_error result.
		int error = closing && rc == 0 ? (SSL_want_write(s) ? SSL_ERROR_WANT_WRITE : SSL_ERROR_WANT_READ)
									   : SSL_get_error(s, rc);
		if (closing && error == SSL_ERROR_WANT_READ &&
			(SSL_get_shutdown(s) & SSL_SENT_SHUTDOWN))
		{
			bool first = !e.sent_close;
			e.sent_close = true;
			if (r.action == protocol_action::close_write)
			{
				return {};
			}
			// The peer alert may already be buffered in SSL. Re-enter once before waiting on the wire.
			if (first)
			{
				return {protocol_status::retry, 0};
			}
		}
		if (error == SSL_ERROR_WANT_READ)
		{
			return {protocol_status::need_input, 0};
		}
		if (error == SSL_ERROR_WANT_WRITE)
		{
			return {protocol_status::need_output, 0};
		}
		if (error == SSL_ERROR_ZERO_RETURN && r.action == protocol_action::read)
		{
			return {};
		}
		// Give the transport a chance to flush the fatal alert before reporting the terminal error.
		return rejected(SSL_get_verify_result(s) == X509_V_OK ? errc::tls_protocol : errc::peer_identity);
	}
};

/** @brief Wraps a connected byte transport as a verifying TLS client. */
template <duplex_protocol_transport handle>
inline auto tls_client(handle &&wire, tls_client_context const &context, char const *peer)
{
	return ::fast_io::make_iotranscoder(::std::forward<handle>(wire), details::deferred_protocol_engine{tls_engine::parameters{context,peer}});
}

/** @brief Wraps a connected byte transport as a TLS server using the configured identity. */
template <duplex_protocol_transport handle>
inline auto tls_server(handle &&wire, tls_server_context const &context)
{
	return ::fast_io::make_iotranscoder(::std::forward<handle>(wire), details::deferred_protocol_engine{tls_engine::parameters{context}});
}

/** @brief Constructs a verifying client over the supplied transport. */
template <duplex_protocol_transport Handle>
inline auto make_tls_client(Handle &&wire, tls_client_context const &context, char const *peer)
{
	return tls_client(::std::forward<Handle>(wire), context, peer);
}

/** @brief Constructs the server role with its configured certificate chain and private key. */
template <duplex_protocol_transport Handle>
inline auto make_tls_server(Handle &&wire, tls_server_context const &context)
{
	return tls_server(::std::forward<Handle>(wire), context);
}

} // namespace fast_io::secure
