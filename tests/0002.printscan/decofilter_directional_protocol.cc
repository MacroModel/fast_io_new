#include <cassert>
#include <concepts>
#include <type_traits>
#include <utility>

#include <fast_io_core.h>

namespace decofilter_directional_protocol
{

struct decorator
{};

struct filter_proxy
{
	unsigned *calls{};
	unsigned char padding[128]{};

	inline explicit constexpr filter_proxy(unsigned *value) noexcept : calls(value)
	{}
	filter_proxy(filter_proxy const &) = delete;
	filter_proxy &operator=(filter_proxy const &) = delete;
};

struct source
{
	filter_proxy proxy;
};

// This source intentionally exposes only the joint spelling. Directional public operations must select it as their
// fallback instead of re-testing the already-true "input-or-io" concept and trying to call a missing directional CPO.
inline constexpr filter_proxy &io_stream_deco_filter_ref_define(source &value) noexcept
{
	return value.proxy;
}

inline constexpr void io_stream_add_deco_filter_define(filter_proxy &proxy, decorator &) noexcept
{
	++*proxy.calls;
}

struct incomplete_proxy;

struct incomplete_source
{};

incomplete_proxy &input_stream_deco_filter_ref_define(incomplete_source &) noexcept;

struct void_source
{};

inline constexpr void output_stream_deco_filter_ref_define(void_source &) noexcept
{}

struct malformed_decorator_holder
{};

inline constexpr void input_decorators_ref_define(malformed_decorator_holder &) noexcept
{}

struct transcode_source
{
	filter_proxy proxy;
};

inline constexpr filter_proxy &io_stream_transcode_deco_filter_ref_define(
	transcode_source &&value) noexcept
{
	return value.proxy;
}

struct malformed_transcode_source
{};

inline constexpr void input_stream_transcode_deco_filter_ref_define(
	malformed_transcode_source &&) noexcept
{}

struct rvalue_transcode_decorator
{
	rvalue_transcode_decorator() = default;
	rvalue_transcode_decorator(rvalue_transcode_decorator const &) = delete;
	rvalue_transcode_decorator &operator=(rvalue_transcode_decorator const &) = delete;
};

struct lvalue_transcode_decorator
{};

struct transcode_result
{
	filter_proxy proxy;

	inline explicit constexpr transcode_result(unsigned *value) noexcept : proxy(value)
	{}
	transcode_result(transcode_result const &) = delete;
	transcode_result &operator=(transcode_result const &) = delete;
	inline constexpr transcode_result(transcode_result &&other) noexcept : proxy(other.proxy.calls)
	{}
};

inline constexpr filter_proxy &io_stream_deco_filter_ref_define(transcode_result &value) noexcept
{
	return value.proxy;
}

// The transcode operation consumes the decorator's original category, but borrows the already-normalized observer.
// A concept which probes a named `P` instead of `forward<P>(p)` rejects this valid rvalue-only customization.
inline constexpr transcode_result io_stream_transcode_deco_filter_define(
	filter_proxy &proxy, rvalue_transcode_decorator &&) noexcept
{
	++*proxy.calls;
	return transcode_result{proxy.calls};
}

inline constexpr transcode_result io_stream_transcode_deco_filter_define(
	filter_proxy &proxy, lvalue_transcode_decorator &) noexcept
{
	++*proxy.calls;
	return transcode_result{proxy.calls};
}

struct void_transcode_decorator
{};

inline constexpr void io_stream_transcode_deco_filter_define(
	filter_proxy &, void_transcode_decorator &&) noexcept
{}

struct incomplete_transcode_result;

struct incomplete_transcode_decorator
{};

incomplete_transcode_result io_stream_transcode_deco_filter_define(
	filter_proxy &, incomplete_transcode_decorator &&) noexcept;

struct no_tail_transcode_decorator
{};

struct no_tail_transcode_result
{};

inline constexpr no_tail_transcode_result io_stream_transcode_deco_filter_define(
	filter_proxy &, no_tail_transcode_decorator &&) noexcept
{
	return {};
}

struct immovable_transcode_decorator
{};

struct immovable_transcode_result
{
	filter_proxy proxy;

	inline explicit constexpr immovable_transcode_result(unsigned *value) noexcept : proxy(value)
	{}
	immovable_transcode_result(immovable_transcode_result const &) = delete;
	immovable_transcode_result(immovable_transcode_result &&) = delete;
};

inline constexpr filter_proxy &io_stream_deco_filter_ref_define(immovable_transcode_result &value) noexcept
{
	return value.proxy;
}

inline constexpr immovable_transcode_result io_stream_transcode_deco_filter_define(
	filter_proxy &proxy, immovable_transcode_decorator &&) noexcept
{
	return immovable_transcode_result{proxy.calls};
}

template <typename Source, typename Deco, typename... Args>
concept can_transcode_input = requires(Source &&source_value, Deco &&deco, Args &&...args) {
	::fast_io::operations::transcode_input_decos(
		::std::forward<Source>(source_value), ::std::forward<Deco>(deco), ::std::forward<Args>(args)...);
};

template <typename Source, typename Deco, typename... Args>
concept can_transcode_output = requires(Source &&source_value, Deco &&deco, Args &&...args) {
	::fast_io::operations::transcode_output_decos(
		::std::forward<Source>(source_value), ::std::forward<Deco>(deco), ::std::forward<Args>(args)...);
};

template <typename Source, typename Deco, typename... Args>
concept can_transcode_io = requires(Source &&source_value, Deco &&deco, Args &&...args) {
	::fast_io::operations::transcode_io_decos(
		::std::forward<Source>(source_value), ::std::forward<Deco>(deco), ::std::forward<Args>(args)...);
};

static_assert(!::std::copy_constructible<filter_proxy>);
static_assert(!::fast_io::operations::defines::has_input_stream_deco_filter_ref_define<source &>);
static_assert(!::fast_io::operations::defines::has_output_stream_deco_filter_ref_define<source &>);
static_assert(::fast_io::operations::defines::has_io_stream_deco_filter_ref_define<source &>);
static_assert(::fast_io::operations::defines::has_input_or_io_stream_deco_filter_ref_define<source &>);
static_assert(::fast_io::operations::defines::has_output_or_io_stream_deco_filter_ref_define<source &>);

// Invalid result storage must fail during constraint substitution, before a public wrapper tries to bind a local.
static_assert(!::fast_io::operations::defines::has_input_stream_deco_filter_ref_define<incomplete_source &>);
static_assert(!::fast_io::operations::defines::has_output_stream_deco_filter_ref_define<void_source &>);
static_assert(!::fast_io::operations::defines::has_input_decorators_ref_define<malformed_decorator_holder &>);
static_assert(!::fast_io::operations::defines::has_input_stream_transcode_deco_filter_ref_define<transcode_source>);
static_assert(::fast_io::operations::defines::has_io_stream_transcode_deco_filter_ref_define<transcode_source>);
static_assert(::fast_io::operations::defines::has_input_or_io_stream_transcode_deco_filter_ref_define<
			  transcode_source>);
static_assert(::fast_io::operations::defines::has_output_or_io_stream_transcode_deco_filter_ref_define<
			  transcode_source>);
static_assert(!::fast_io::operations::defines::has_input_stream_transcode_deco_filter_ref_define<
			  malformed_transcode_source>);

static_assert(::fast_io::operations::decay::defines::has_io_stream_transcode_deco_filter_define<
			  filter_proxy, rvalue_transcode_decorator>);
static_assert(!::fast_io::operations::decay::defines::has_io_stream_transcode_deco_filter_define<
			  filter_proxy, rvalue_transcode_decorator &>);
static_assert(::fast_io::operations::decay::defines::has_io_stream_transcode_deco_filter_define<
			  filter_proxy, lvalue_transcode_decorator &>);
static_assert(!::fast_io::operations::decay::defines::has_io_stream_transcode_deco_filter_define<
			  filter_proxy, lvalue_transcode_decorator>);

// All three directional public wrappers must accept the io-only fallback without copying the 128-byte observer.
static_assert(can_transcode_input<transcode_source, rvalue_transcode_decorator, decorator>);
static_assert(can_transcode_output<transcode_source, rvalue_transcode_decorator, decorator>);
static_assert(can_transcode_io<transcode_source, rvalue_transcode_decorator, decorator>);
static_assert(can_transcode_input<transcode_source, lvalue_transcode_decorator &, decorator>);

// Invalid CPO results and invalid trailing composition must disappear during constraint substitution.
static_assert(!can_transcode_input<malformed_transcode_source, rvalue_transcode_decorator>);
static_assert(!can_transcode_input<transcode_source, void_transcode_decorator>);
static_assert(!can_transcode_input<transcode_source, incomplete_transcode_decorator>);
static_assert(can_transcode_input<transcode_source, no_tail_transcode_decorator>);
static_assert(!can_transcode_input<transcode_source, no_tail_transcode_decorator, decorator>);
static_assert(can_transcode_input<transcode_source, immovable_transcode_decorator>);
static_assert(!can_transcode_input<transcode_source, immovable_transcode_decorator, decorator>);

} // namespace decofilter_directional_protocol

int main()
{
	using namespace ::decofilter_directional_protocol;
	unsigned calls{};
	source value{filter_proxy{__builtin_addressof(calls)}};

	::fast_io::operations::add_input_decos(value, decorator{});
	::fast_io::operations::add_output_decos(value, decorator{});
	::fast_io::operations::add_io_decos(value, decorator{});
	assert(calls == 3u);

	transcode_source transcode{filter_proxy{__builtin_addressof(calls)}};
	decltype(auto) input_fallback = ::fast_io::operations::input_stream_transcode_deco_filter_ref(
		::std::move(transcode));
	static_assert(::std::same_as<decltype(input_fallback), filter_proxy &>);
	assert(__builtin_addressof(input_fallback) == __builtin_addressof(transcode.proxy));

	// The entry reference is a large noncopyable lvalue. Successful execution therefore proves that the public
	// boundary preserved it and that every deeper transcode strategy borrowed the same object.
	auto input_result = ::fast_io::operations::transcode_input_decos(
		::std::move(transcode), rvalue_transcode_decorator{}, decorator{});
	assert(input_result.proxy.calls == __builtin_addressof(calls));
	auto output_result = ::fast_io::operations::transcode_output_decos(
		::std::move(transcode), rvalue_transcode_decorator{}, decorator{});
	assert(output_result.proxy.calls == __builtin_addressof(calls));
	auto io_result = ::fast_io::operations::transcode_io_decos(
		::std::move(transcode), rvalue_transcode_decorator{}, decorator{});
	assert(io_result.proxy.calls == __builtin_addressof(calls));
	assert(calls == 9u);
}
