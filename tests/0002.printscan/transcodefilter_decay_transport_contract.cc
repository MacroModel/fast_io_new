#include <concepts>
#include <cstdlib>
#include <type_traits>
#include <utility>

#include <fast_io_core.h>

namespace transcodefilter_decay_transport_contract
{

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

struct counters
{
	unsigned normalizations{};
	unsigned constructions{};
	unsigned source_destructions{};
};

struct decorator
{};

struct result
{
	unsigned normalization_snapshot{};
	unsigned construction_snapshot{};
};

/** A one-pointer projection whose copies all address the same external state. */
struct substitutable_proxy
{
	counters *state{};
};

inline constexpr ::std::true_type stream_ref_value_transport_safe_define(
	::fast_io::io_type_t<substitutable_proxy>) noexcept
{
	return {};
}

struct substitutable_source
{
	counters *state{};

	inline ~substitutable_source()
	{
		++state->source_destructions;
	}
};

inline substitutable_proxy io_stream_transcode_deco_filter_ref_define(
	substitutable_source &&source) noexcept
{
	++source.state->normalizations;
	return {source.state};
}

/** A normalized observer whose cursor is part of the observer object's identity. */
struct identity_proxy
{
	counters *state{};
	unsigned cursor{};

	inline explicit identity_proxy(counters &value) noexcept
		: state(__builtin_addressof(value))
	{}

	identity_proxy(identity_proxy const &) = delete;
	identity_proxy &operator=(identity_proxy const &) = delete;
	identity_proxy(identity_proxy &&) = delete;
	identity_proxy &operator=(identity_proxy &&) = delete;
};

struct identity_source
{
	identity_proxy proxy;

	inline explicit identity_source(counters &value) noexcept : proxy(value)
	{}

	inline ~identity_source()
	{
		++proxy.state->source_destructions;
	}
};

inline identity_proxy &io_stream_transcode_deco_filter_ref_define(
	identity_source &&source) noexcept
{
	++source.proxy.state->normalizations;
	return source.proxy;
}

inline result io_stream_transcode_deco_filter_define(
	substitutable_proxy &proxy, decorator &&) noexcept
{
	++proxy.state->constructions;
	return {proxy.state->normalizations, proxy.state->constructions};
}

inline result io_stream_transcode_deco_filter_define(
	identity_proxy &proxy, decorator &&) noexcept
{
	++proxy.cursor;
	++proxy.state->constructions;
	return {proxy.state->normalizations, proxy.state->constructions};
}

template <typename T>
concept exposes_input_char_type = requires { typename T::input_char_type; };

using owner_entry = result (*)(substitutable_proxy, decorator &&);
using borrowed_entry = result (*)(substitutable_proxy &, decorator &&);

/*
 * The generic ABI predicate is intentional: a transcode-filter projection is
 * valid without exposing a primitive input/output character typedef. Every
 * unsuffixed decay entry owns its observer, while both the explicitly borrowed
 * implementation and mandatory-inline selector retain a reference parameter.
 */
static_assert(!exposes_input_char_type<substitutable_proxy>);
static_assert(
	::fast_io::operations::defines::abi_value_stream_ref_result_object<
		substitutable_proxy &>());
static_assert(!::fast_io::operations::defines::
				  abi_value_stream_ref_result_object<identity_proxy &>());

static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transcode_input_decos_decay<
					   substitutable_proxy, decorator>),
			  owner_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transcode_input_decos_decay_borrowed<
					   substitutable_proxy, decorator>),
			  borrowed_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transcode_input_decos_decay_dispatch<
					   substitutable_proxy, decorator>),
			  borrowed_entry>);

static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transcode_output_decos_decay<
					   substitutable_proxy, decorator>),
			  owner_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transcode_output_decos_decay_borrowed<
					   substitutable_proxy, decorator>),
			  borrowed_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transcode_output_decos_decay_dispatch<
					   substitutable_proxy, decorator>),
			  borrowed_entry>);

static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transcode_io_decos_decay<
					   substitutable_proxy, decorator>),
			  owner_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transcode_io_decos_decay_borrowed<
					   substitutable_proxy, decorator>),
			  borrowed_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transcode_io_decos_decay_dispatch<
					   substitutable_proxy, decorator>),
			  borrowed_entry>);

/*
 * The public wrappers must finish the construction before their rvalue source
 * dies and return the resulting owner by value. These expressions instantiate
 * the complete normalization/dispatch path, including the io-only fallback.
 */
static_assert(::std::same_as<
			  decltype(::fast_io::operations::transcode_input_decos(
				  ::std::declval<substitutable_source>(),
				  ::std::declval<decorator>())),
			  result>);
static_assert(::std::same_as<
			  decltype(::fast_io::operations::transcode_output_decos(
				  ::std::declval<substitutable_source>(),
				  ::std::declval<decorator>())),
			  result>);
static_assert(::std::same_as<
			  decltype(::fast_io::operations::transcode_io_decos(
				  ::std::declval<substitutable_source>(),
				  ::std::declval<decorator>())),
			  result>);

template <typename result_type>
inline void verify_result(result_type const &value, counters const &state) noexcept
{
	// The source temporary has already been destroyed. Only copied scalar state
	// remains in the returned owner, so observing it cannot dereference a helper.
	require(state.normalizations == 1u);
	require(state.constructions == 1u);
	require(state.source_destructions == 1u);
	require(value.normalization_snapshot == 1u);
	require(value.construction_snapshot == 1u);
}

inline void verify_public_normalization_and_result_lifetime() noexcept
{
	{
		counters state{};
		auto value{::fast_io::operations::transcode_input_decos(
			substitutable_source{__builtin_addressof(state)}, decorator{})};
		verify_result(value, state);
	}
	{
		counters state{};
		auto value{::fast_io::operations::transcode_output_decos(
			substitutable_source{__builtin_addressof(state)}, decorator{})};
		verify_result(value, state);
	}
	{
		counters state{};
		auto value{::fast_io::operations::transcode_io_decos(
			substitutable_source{__builtin_addressof(state)}, decorator{})};
		verify_result(value, state);
	}
}

inline void verify_identity_preserving_public_path() noexcept
{
	{
		counters state{};
		auto value{::fast_io::operations::transcode_input_decos(
			identity_source{state}, decorator{})};
		verify_result(value, state);
	}
	{
		counters state{};
		auto value{::fast_io::operations::transcode_output_decos(
			identity_source{state}, decorator{})};
		verify_result(value, state);
	}
	{
		counters state{};
		auto value{::fast_io::operations::transcode_io_decos(
			identity_source{state}, decorator{})};
		verify_result(value, state);
	}
}

} // namespace transcodefilter_decay_transport_contract

int main()
{
	::transcodefilter_decay_transport_contract::
		verify_public_normalization_and_result_lifetime();
	::transcodefilter_decay_transport_contract::
		verify_identity_preserving_public_path();
}
