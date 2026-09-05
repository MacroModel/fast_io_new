/*
This test remains an empty ordinary-C++ smoke test, but exercises real
deterministic error propagation when compiled and linked with the experimental
Herbception runtime. Keeping it separate from the compile-time contract makes
the required runtime linkage explicit and prevents a standard toolchain from
acquiring a second test-only ABI.
*/

#include <fast_io_core.h>

#if defined(__HERBCEPTIONS__)

namespace herbceptions_effect_runtime
{

struct alias_value
{
	int payload{};
};

struct alias_source
{
	bool fail{};
};

struct alias_reference_source
{
	bool fail{};
	unsigned calls{};
};

struct alias_xvalue_reference_source
{
	bool fail{};
	unsigned calls{};
};

struct legacy_exception_alias_source
{
	bool fail{};
	unsigned calls{};
};

inline alias_value alias_reference_storage{43};
inline alias_value alias_xvalue_reference_storage{47};

inline alias_value print_alias_define(::fast_io::io_alias_t, alias_source &source) throws
{
	if (source.fail)
	{
		throw throws ::std::errc::invalid_argument;
	}
	return {31};
}

inline alias_value &print_alias_define(
	::fast_io::io_alias_t, alias_reference_source &source) throws
{
	++source.calls;
	if (source.fail)
	{
		throw throws ::std::errc::invalid_argument;
	}
	return alias_reference_storage;
}

inline alias_value &&print_alias_define(
	::fast_io::io_alias_t, alias_xvalue_reference_source &source) throws
{
	++source.calls;
	if (source.fail)
	{
		throw throws ::std::errc::invalid_argument;
	}
	return static_cast<alias_value &&>(alias_xvalue_reference_storage);
}

// This CPO has only the traditional exception channel.  The public alias wrapper nevertheless has active
// basic-throws because its ordinary nothrow proof is false; the language must convert an escaping legacy exception
// into std::error without invoking the customization again.
inline alias_value print_alias_define(
	::fast_io::io_alias_t, legacy_exception_alias_source &source)
{
	++source.calls;
	if (source.fail)
	{
		throw 71;
	}
	return {71};
}

struct output_observer
{
	using output_char_type = char;
	int state{};
};

struct output_handle
{
	bool fail{};
};

struct output_reference_handle
{
	output_observer observer{53};
	bool fail{};
	unsigned calls{};
};

inline output_observer output_stream_ref_define(output_handle &handle) throws
{
	if (handle.fail)
	{
		throw throws ::std::errc::io_error;
	}
	return {37};
}

inline output_observer &output_stream_ref_define(output_reference_handle &handle) throws
{
	++handle.calls;
	if (handle.fail)
	{
		throw throws ::std::errc::io_error;
	}
	return handle.observer;
}

inline bool alias_success() noexcept
{
	alias_source source{};
	try
	{
		auto value{::fast_io::io_print_alias(source)};
		return value.payload == 31;
	}
	catch throws(::std::error)
	{
		return false;
	}
}

inline bool alias_failure() noexcept
{
	alias_source source{true};
	try
	{
		(void)::fast_io::io_print_alias(source);
		return false;
	}
	catch throws(::std::error error)
	{
		return error == ::std::errc::invalid_argument;
	}
}

/// A successful deterministic result must preserve its reference identity and
/// value category; copying the global object would satisfy the payload check
/// while violating the CPO contract, so both address and mutation are tested.
inline bool alias_reference_success() noexcept
{
	alias_reference_source source{};
	try
	{
		auto &&value{::fast_io::io_print_alias(source)};
		value.payload = 59;
		return &value == &alias_reference_storage &&
			alias_reference_storage.payload == 59 && source.calls == 1u;
	}
	catch throws(::std::error)
	{
		return false;
	}
}

/// The failure edge must test the discriminator from the same invocation that
/// produced the payload. A second invocation can be masked by the same error
/// code, so the per-source call counter is part of the observable contract.
inline bool alias_reference_failure() noexcept
{
	alias_reference_source source{true};
	try
	{
		(void)::fast_io::io_print_alias(source);
		return false;
	}
	catch throws(::std::error error)
	{
		return error == ::std::errc::invalid_argument && source.calls == 1u;
	}
}

inline bool alias_xvalue_reference_success() noexcept
{
	alias_xvalue_reference_source source{};
	try
	{
		auto &&value{::fast_io::io_print_alias(source)};
		value.payload = 61;
		return &value == &alias_xvalue_reference_storage &&
			alias_xvalue_reference_storage.payload == 61 && source.calls == 1u;
	}
	catch throws(::std::error)
	{
		return false;
	}
}

inline bool alias_xvalue_reference_failure() noexcept
{
	alias_xvalue_reference_source source{true};
	try
	{
		(void)::fast_io::io_print_alias(source);
		return false;
	}
	catch throws(::std::error error)
	{
		return error == ::std::errc::invalid_argument && source.calls == 1u;
	}
}

inline bool legacy_exception_alias_success() noexcept
{
	legacy_exception_alias_source source{};
	try
	{
		auto value{::fast_io::io_print_alias(source)};
		return value.payload == 71 && source.calls == 1u;
	}
	catch throws(::std::error)
	{
		return false;
	}
}

inline bool legacy_exception_alias_failure() noexcept
{
	legacy_exception_alias_source source{true};
	try
	{
		(void)::fast_io::io_print_alias(source);
		return false;
	}
	catch throws(::std::error)
	{
		return source.calls == 1u;
	}
}

inline bool output_ref_success() noexcept
{
	output_handle handle{};
	try
	{
		auto observer{::fast_io::operations::output_stream_ref(handle)};
		return observer.state == 37;
	}
	catch throws(::std::error)
	{
		return false;
	}
}

inline bool output_ref_failure() noexcept
{
	output_handle handle{true};
	try
	{
		(void)::fast_io::operations::output_stream_ref(handle);
		return false;
	}
	catch throws(::std::error error)
	{
		return error == ::std::errc::io_error;
	}
}

inline bool output_reference_success() noexcept
{
	output_reference_handle handle{};
	try
	{
		auto &&observer{::fast_io::operations::output_stream_ref(handle)};
		observer.state = 67;
		return &observer == &handle.observer && handle.observer.state == 67 &&
			handle.calls == 1u;
	}
	catch throws(::std::error)
	{
		return false;
	}
}

inline bool output_reference_failure() noexcept
{
	output_reference_handle handle{{53}, true};
	try
	{
		(void)::fast_io::operations::output_stream_ref(handle);
		return false;
	}
	catch throws(::std::error error)
	{
		return error == ::std::errc::io_error && handle.calls == 1u;
	}
}

} // namespace herbceptions_effect_runtime

#endif

int main()
{
#if defined(__HERBCEPTIONS__)
	using namespace herbceptions_effect_runtime;
	return alias_success() && alias_failure() && alias_reference_success() &&
		alias_reference_failure() && alias_xvalue_reference_success() &&
		alias_xvalue_reference_failure() && legacy_exception_alias_success() &&
		legacy_exception_alias_failure() && output_ref_success() && output_ref_failure() &&
		output_reference_success() && output_reference_failure()
		? 0
		: 1;
#else
	return 0;
#endif
}
