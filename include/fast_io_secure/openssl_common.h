#pragma once
/**
 * @file
 * @brief Shared ownership helpers of the opt-in OpenSSL 3 providers.
 */
#include "error.h"
#include <openssl/opensslv.h>
#include <openssl/crypto.h>
#include <memory>

#if OPENSSL_VERSION_MAJOR < 3
#error "fast_io secure providers require OpenSSL 3 or later"
#endif

namespace fast_io::secure::details
{
/** @brief Releases one owned provider object with its matching OpenSSL destructor. */
template <auto release>
struct deleter
{
	template <typename T>
	void operator()(T *p) const noexcept
	{
		release(p);
	}
};

template <typename T, auto release>
using owner = ::std::unique_ptr<T, deleter<release>>;

} // namespace fast_io::secure::details
