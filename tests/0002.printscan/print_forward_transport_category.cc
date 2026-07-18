#include <cassert>
#include <concepts>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace
{

struct status_proxy
{
	char prefix;
	char value;
	::std::size_t calls{};

	inline explicit status_proxy(char prefix_value, char value_value) noexcept
		: prefix(prefix_value), value(value_value)
	{}

	status_proxy(status_proxy const &) = delete;
	status_proxy &operator=(status_proxy const &) = delete;
	status_proxy(status_proxy &&) = delete;
	status_proxy &operator=(status_proxy &&) = delete;
	inline ~status_proxy() {}
};

struct status_source
{
	status_proxy *proxy;
};

inline status_proxy &status_io_print_forward(
	::fast_io::io_alias_type_t<char>, status_source &source) noexcept
{
	return *source.proxy;
}

inline status_proxy &status_io_print_forward(
	::fast_io::io_alias_type_t<char>, status_source &&source) noexcept
{
	// Pack admission models an rvalue pack by forwarding its stored elements. The source contains only a pointer, so
	// both value categories legitimately name the same externally-owned proxy and exercise the same transport rule.
	return *source.proxy;
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, status_proxy>) noexcept
{
	return 2u;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, status_proxy>, char *iter, status_proxy &proxy) noexcept
{
	++proxy.calls;
	*iter++ = proxy.prefix;
	*iter++ = proxy.value;
	return iter;
}

inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<char, status_proxy>, status_proxy &) noexcept
{
	return 1u;
}

using status_transport =
	decltype(::fast_io::io_print_forward<char>(::std::declval<status_source &>()));
static_assert(::std::same_as<status_transport, ::fast_io::parameter<status_proxy &>>);
static_assert(::std::is_trivially_copyable_v<status_transport>);
static_assert(::fast_io::reserve_printable<char, status_transport>);
static_assert(::fast_io::printable_internal_shift<char, status_transport>);

struct large_trivial_transport_probe
{
	::std::size_t words[3];
};

static_assert(::std::is_trivially_copyable_v<large_trivial_transport_probe>);
static_assert(sizeof(large_trivial_transport_probe) >
			  ::fast_io::details::io_print_forward_transport_max_value_size);
static_assert(::std::same_as<
	decltype(::fast_io::details::io_print_forward_transport(
		::std::declval<large_trivial_transport_probe &>())),
	::fast_io::parameter<large_trivial_transport_probe &>>);
static_assert(::std::same_as<
	decltype(::fast_io::details::io_print_forward_transport(
		::std::declval<large_trivial_transport_probe>())),
	large_trivial_transport_probe>);

// These two formatter families expose the same protocol surface but deliberately accept opposite cv categories.
// The parameter adapters must recognize the expression used by their body, not an unqualified approximation of it.
template <bool const_only>
struct category_formatter
{
	char value{'C'};
};

template <bool const_only>
using category_reference = ::std::conditional_t<
	const_only, category_formatter<const_only> const &, category_formatter<const_only> &>;

template <bool const_only>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, category_formatter<const_only>>) noexcept
{
	return 1u;
}

template <bool const_only>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, category_formatter<const_only>>,
	category_reference<const_only>) noexcept
{
	return 1u;
}

template <bool const_only>
inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, category_formatter<const_only>>, char *iter,
	category_reference<const_only> value) noexcept
{
	*iter = value.value;
	return iter + 1;
}

template <bool const_only>
inline constexpr ::std::size_t print_reserve_static_stack_size(
	::fast_io::io_reserve_type_t<char, category_formatter<const_only>>) noexcept
{
	return 1u;
}

template <bool const_only>
inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<char, category_formatter<const_only>>,
	category_reference<const_only>) noexcept
{
	return 0u;
}

template <bool const_only>
inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, category_formatter<const_only>>,
	category_reference<const_only>) noexcept
{
	return 1u;
}

template <bool const_only>
inline constexpr ::std::size_t print_reserve_static_precise_size(
	::fast_io::io_reserve_type_t<char, category_formatter<const_only>>) noexcept
{
	return 1u;
}

template <bool const_only>
inline constexpr char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, category_formatter<const_only>>, char *iter,
	::std::size_t, category_reference<const_only> value) noexcept
{
	*iter = value.value;
	return iter + 1;
}

template <bool const_only>
inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, category_formatter<const_only>>,
	category_reference<const_only> value) noexcept
{
	return {__builtin_addressof(value.value), 1u};
}

template <bool const_only>
inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, category_formatter<const_only>>) noexcept
{
	return {1u, 0u};
}

template <bool const_only>
inline constexpr ::fast_io::basic_reserve_scatters_define_result<char>
print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, category_formatter<const_only>>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	category_reference<const_only> value) noexcept
{
	*scatters = {__builtin_addressof(value.value), 1u};
	return {scatters + 1, reserve};
}

template <bool const_only>
struct category_context
{
	inline constexpr ::fast_io::context_print_result<char *> print_context_define(
		category_reference<const_only> value, char *begin, char *end) noexcept
	{
		if (begin == end)
		{
			return {begin, false};
		}
		*begin = value.value;
		return {begin + 1, true};
	}
};

template <bool const_only>
inline constexpr auto print_context_type(
	::fast_io::io_reserve_type_t<char, category_formatter<const_only>>) noexcept
{
	return ::fast_io::io_type_t<category_context<const_only>>{};
}

template <bool const_only>
inline constexpr ::std::size_t print_context_static_buffer_size(
	::fast_io::io_reserve_type_t<char, category_formatter<const_only>>) noexcept
{
	return 1u;
}

struct category_staged_state
{
	char value{};
};

template <bool const_only>
inline constexpr auto print_staged_type(
	::fast_io::io_reserve_type_t<char, category_formatter<const_only>>) noexcept
{
	return ::fast_io::io_type_t<category_staged_state>{};
}

template <bool const_only>
inline constexpr ::std::size_t print_staged_width(
	::fast_io::io_reserve_type_t<char, category_formatter<const_only>>) noexcept
{
	return 2u;
}

template <bool const_only>
inline constexpr bool print_staged_eligible(
	::fast_io::io_reserve_type_t<char, category_formatter<const_only>>,
	category_reference<const_only>) noexcept
{
	return true;
}

template <bool const_only>
inline constexpr category_staged_state print_staged_prepare(
	::fast_io::io_reserve_type_t<char, category_formatter<const_only>>,
	category_reference<const_only> value) noexcept
{
	return {value.value};
}

template <bool const_only>
inline constexpr char *print_staged_define(
	::fast_io::io_reserve_type_t<char, category_formatter<const_only>>, char *iter,
	category_reference<const_only>, category_staged_state const &state) noexcept
{
	*iter = state.value;
	return iter + 1;
}

template <typename output, bool const_only>
	requires ::std::is_trivially_copyable_v<output>
inline constexpr void print_define(
	::fast_io::io_reserve_type_t<char, category_formatter<const_only>>, output,
	category_reference<const_only>) noexcept
{}

template <bool const_only>
inline constexpr ::std::true_type print_buffered_preferred(
	::fast_io::io_reserve_type_t<char, category_formatter<const_only>>) noexcept
{
	return {};
}

template <bool const_only>
inline constexpr ::std::true_type print_put_area_preferred(
	::fast_io::io_reserve_type_t<char, category_formatter<const_only>>) noexcept
{
	return {};
}

template <typename T>
concept complete_category_protocol =
	::fast_io::reserve_printable<char, T> &&
	::fast_io::dynamic_reserve_printable<char, T> &&
	::fast_io::dynamic_reserve_with_possible_static_stack_size<char, T> &&
	::fast_io::printable_internal_shift<char, T> &&
	::fast_io::precise_reserve_printable<char, T> &&
	::fast_io::static_precise_reserve_printable<char, T> &&
	::fast_io::scatter_printable<char, T> &&
	::fast_io::reserve_scatters_printable<char, T> &&
	::fast_io::context_printable<char, T> &&
	::fast_io::context_printable_with_static_buffer_size<char, T> &&
	::fast_io::staged_printable<char, T> &&
	::fast_io::printable<char, T> &&
	::fast_io::buffered_printable_preferred<char, T> &&
	::fast_io::put_area_printable_preferred<char, T>;

template <typename T>
concept no_category_protocol =
	(!::fast_io::reserve_printable<char, T>) &&
	(!::fast_io::dynamic_reserve_printable<char, T>) &&
	(!::fast_io::dynamic_reserve_with_possible_static_stack_size<char, T>) &&
	(!::fast_io::printable_internal_shift<char, T>) &&
	(!::fast_io::precise_reserve_printable<char, T>) &&
	(!::fast_io::static_precise_reserve_printable<char, T>) &&
	(!::fast_io::scatter_printable<char, T>) &&
	(!::fast_io::reserve_scatters_printable<char, T>) &&
	(!::fast_io::context_printable<char, T>) &&
	(!::fast_io::context_printable_with_static_buffer_size<char, T>) &&
	(!::fast_io::staged_printable<char, T>) &&
	(!::fast_io::printable<char, T>) &&
	(!::fast_io::buffered_printable_preferred<char, T>) &&
	(!::fast_io::put_area_printable_preferred<char, T>);

using mutable_formatter = category_formatter<false>;
using const_formatter = category_formatter<true>;
static_assert(complete_category_protocol<::fast_io::parameter<mutable_formatter &>>);
static_assert(no_category_protocol<::fast_io::parameter<mutable_formatter const &>>);
static_assert(complete_category_protocol<::fast_io::parameter<const_formatter &>>);
static_assert(complete_category_protocol<::fast_io::parameter<const_formatter const &>>);

template <bool const_only>
struct dynamic_scatter_formatter
{
	char value{'D'};
};

template <bool const_only>
using dynamic_scatter_reference = ::std::conditional_t<
	const_only, dynamic_scatter_formatter<const_only> const &,
	dynamic_scatter_formatter<const_only> &>;

template <bool const_only>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, dynamic_scatter_formatter<const_only>>,
	dynamic_scatter_reference<const_only>) noexcept
{
	return 1u;
}

template <bool const_only>
inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, dynamic_scatter_formatter<const_only>>, char *iter,
	dynamic_scatter_reference<const_only> value) noexcept
{
	*iter = value.value;
	return iter + 1;
}

template <bool const_only>
inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, dynamic_scatter_formatter<const_only>>,
	dynamic_scatter_reference<const_only>) noexcept
{
	return {1u, 0u};
}

template <bool const_only>
inline constexpr ::fast_io::basic_reserve_scatters_define_result<char>
print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, dynamic_scatter_formatter<const_only>>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	dynamic_scatter_reference<const_only> value) noexcept
{
	*scatters = {__builtin_addressof(value.value), 1u};
	return {scatters + 1, reserve};
}

using mutable_dynamic = dynamic_scatter_formatter<false>;
using const_dynamic = dynamic_scatter_formatter<true>;
static_assert(::fast_io::dynamic_reserve_scatters_printable<
	char, ::fast_io::parameter<mutable_dynamic &>>);
static_assert(!::fast_io::dynamic_reserve_scatters_printable<
	char, ::fast_io::parameter<mutable_dynamic const &>>);
static_assert(::fast_io::dynamic_reserve_scatters_printable<
	char, ::fast_io::parameter<const_dynamic &>>);
static_assert(::fast_io::dynamic_reserve_scatters_printable<
	char, ::fast_io::parameter<const_dynamic const &>>);

struct unsafe_alias_scatter_source
{
	char const *value;
};

[[maybe_unused]] inline constexpr ::fast_io::basic_io_scatter_t<char> print_alias_define(
	::fast_io::io_alias_t, unsafe_alias_scatter_source source) noexcept
{
	return {source.value, 1u};
}

struct safe_alias_scatter_source
{
	char const *value;
};

[[maybe_unused]] inline constexpr ::fast_io::basic_io_scatter_t<char> print_alias_define(
	::fast_io::io_alias_t, safe_alias_scatter_source source) noexcept
{
	return {source.value, 1u};
}

[[maybe_unused]] inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, safe_alias_scatter_source>) noexcept
{
	return {};
}

struct unsafe_status_scatter_source
{
	char const *value;
};

[[maybe_unused]] inline constexpr ::fast_io::basic_io_scatter_t<char> status_io_print_forward(
	::fast_io::io_alias_type_t<char>, unsafe_status_scatter_source source) noexcept
{
	return {source.value, 1u};
}

struct safe_status_scatter_source
{
	char const *value;
};

[[maybe_unused]] inline constexpr ::fast_io::basic_io_scatter_t<char> status_io_print_forward(
	::fast_io::io_alias_type_t<char>, safe_status_scatter_source source) noexcept
{
	return {source.value, 1u};
}

[[maybe_unused]] inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, safe_status_scatter_source>) noexcept
{
	return {};
}

struct value_alias_result
{};

struct value_alias_source
{};

[[maybe_unused]] inline constexpr value_alias_result print_alias_define(
	::fast_io::io_alias_t, value_alias_source) noexcept
{
	return {};
}

static_assert(!::fast_io::alias_printable<unsafe_alias_scatter_source>);
static_assert(::fast_io::alias_printable<safe_alias_scatter_source>);
static_assert(!::fast_io::status_io_print_forwardable<char, unsafe_status_scatter_source>);
static_assert(::fast_io::status_io_print_forwardable<char, safe_status_scatter_source>);
static_assert(::fast_io::alias_printable<value_alias_source>);

template <typename T>
inline ::std::string render(T &&value)
{
	::std::string result;
	::fast_io::ostring_ref_std output{__builtin_addressof(result)};
	::fast_io::print(output, ::std::forward<T>(value));
	return result;
}

template <typename T>
inline ::std::string render_line(T &&value)
{
	::std::string result;
	::fast_io::ostring_ref_std output{__builtin_addressof(result)};
	::fast_io::println(output, ::std::forward<T>(value));
	return result;
}

} // namespace

int main()
{
	status_proxy proxy{'-', 'x'};
	status_source source{__builtin_addressof(proxy)};

	assert(render(source) == "-x");
	assert(render_line(source) == "-x\n");
	assert(::fast_io::concat_std(source) == "-x");
	assert(::fast_io::concatln_std(source) == "-x\n");

	auto packed{::fast_io::mnp::pack("[", source, "]")};
	assert(render(packed) == "[-x]");
	assert(::fast_io::concat_std(packed) == "[-x]");

	auto selected{::fast_io::mnp::cond(true, source, status_source{__builtin_addressof(proxy)})};
	assert(render(selected) == "-x");
	assert(::fast_io::concat_std(selected) == "-x");

	auto internal{::fast_io::mnp::internal(source, 5u, '_')};
	assert(render(internal) == "-___x");
	assert(::fast_io::concat_std(internal) == "-___x");
	assert(proxy.calls != 0u);
}
