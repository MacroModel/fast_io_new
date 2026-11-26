#pragma once
#include "../fast_io_core.h"

namespace fast_io::secure
{

/** @brief Identifies peer authentication, message validity and local provider failures. */
enum class errc : unsigned char
{
	configuration = 1,
	peer_identity,
	tls_protocol,
	authentication,
	invalid_message,
	usage_limit,
	provider,
	tls_record_overflow,
	tls_illegal_parameter,
	tls_decode
};

/** @brief Assigns this provider a stable fast_io error domain. */
inline constexpr ::std::size_t domain_define(::fast_io::error_type_t<errc>) noexcept
{
	return static_cast<::std::size_t>(0x66696f7365637572ULL);
}

/** @brief Compares a propagated error against the secure stream domain and code. */
inline constexpr bool equivalent_define(::fast_io::error_type_t<errc> t, ::fast_io::error e, errc code) noexcept
{
	return e.domain == domain_define(t) && e.code == static_cast<::std::size_t>(code);
}

namespace details
{
/** @brief Propagates a terminal secure-stream failure following the library's exception configuration. */
[[noreturn]] inline void fail(errc code)
{
#if defined(__cpp_exceptions) && !(defined(_MSC_VER) && !defined(_CPPUNWIND))
	throw ::fast_io::error{domain_define(::fast_io::error_type<errc>), static_cast<::std::size_t>(code)};
#else
	(void)code;
	::fast_io::fast_terminate();
#endif
}

/** @brief Rejects a provider failure before any partially initialized state is published. */
inline void check(bool ok, errc code = errc::provider)
{
	if (!ok) [[unlikely]]
	{
		fail(code);
	}
}
} // namespace details
} // namespace fast_io::secure

#if defined(__HERBCEPTIONS__)
// A TLS provider step catches this exact domain again before publishing its stable protocol_result.
namespace std
{
/** @brief Preserves the exact secure-stream code across a deterministic error channel. */
template <>
class error_domain<::fast_io::secure::errc>
{
private:
	/** @brief Maps related secure-stream failures to a common generic condition for callers that compare domains. */
	static constexpr ::std::errc generic(::std::size_t value) noexcept
	{
		using secure_error = ::fast_io::secure::errc;
		switch (static_cast<secure_error>(value))
		{
		case secure_error::configuration: return ::std::errc::invalid_argument;
		case secure_error::peer_identity:
		case secure_error::authentication: return ::std::errc::permission_denied;
		case secure_error::usage_limit:
		case secure_error::tls_record_overflow: return ::std::errc::value_too_large;
		case secure_error::provider: return ::std::errc::io_error;
		default: return ::std::errc::protocol_error;
		}
	}
	/** @brief Compares precise secure codes within this domain and generic conditions across different domains. */
	static constexpr bool equivalent(::std::size_t value, ::std::error_domain_singleton const *other, ::std::size_t code) noexcept
	{
		return other == &singleton ? value == code : generic(value) == other->do_to_errc(code);
	}
	/** @brief Supplies a stable UTF-8 domain name for generic error reporters without allocating storage. */
	static void describe(::std::size_t, ::std::error_query_information query, ::std::error_reporter_encoding encoding,
	                     void *cookie, ::std::error_reporter_io_cookie_function emit) noexcept
	{
		if (encoding==::std::error_reporter_encoding::utf8 &&
		    (static_cast<unsigned>(query)&static_cast<unsigned>(::std::error_query_information::name)))
		{
			constexpr char name[]{"fast_io.secure"};
			::std::io_scatter_t const part{name,sizeof(name)-1u};
			emit(cookie,&part,1);
		}
	}
	inline static constexpr ::std::error_domain_singleton singleton{
		.do_equivalent=equivalent, .do_query_information=describe, .do_to_errc=generic};
public:
	/** @brief Returns the one process-wide domain shared by all secure-stream Herbceptions. */
	static constexpr ::std::error_domain_singleton const *domain() noexcept { return &singleton; }
	/** @brief Preserves every protocol and authentication failure code without translation. */
	static constexpr ::std::size_t code(::fast_io::secure::errc value) noexcept { return static_cast<::std::size_t>(value); }
};
} // namespace std
#endif
