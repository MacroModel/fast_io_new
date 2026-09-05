#include <array>
#include <cassert>
#include <cstddef>
#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>

#include "scan_concept_support.h"

// This matrix audits scanner protocol composition, not conversion algorithms. Every successful scanner copies a
// fixed record, recognizes one literal separator, or accumulates uninterpreted characters up to '|'. Negative fixtures
// differ only in a concept-visible type, constant-expression, lifetime, or iterator/progress contract.
namespace scan_protocol_composition_matrix
{

template <::std::size_t extent>
struct parameter_fixed_target
{
	::std::array<char, extent> value{};
	::std::size_t calls{};
	bool received_object_pointer{};
};

// Ordinary public scan targets have no alias CPO. `io_scan_alias` therefore carries the target as `parameter<T&>`;
// defining the scanner only on that wrapper proves that parameter propagation is part of the supported scan protocol.
template <::std::size_t extent>
inline constexpr ::std::size_t scan_precise_reserve_size(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<parameter_fixed_target<extent> &>>) noexcept
{
	return extent;
}

template <::std::size_t extent>
inline constexpr void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<parameter_fixed_target<extent> &>>,
	char const *buffer, ::fast_io::parameter<parameter_fixed_target<extent> &> target) noexcept
{
	target.reference.received_object_pointer = buffer != nullptr;
	for (::std::size_t i{}; i != extent; ++i)
	{
		target.reference.value[i] = buffer[i];
	}
	++target.reference.calls;
}

struct parameter_contiguous_target
{
	::std::array<char, 8u> value{};
	::std::size_t size{};
};

inline constexpr ::fast_io::parse_result<char const *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<parameter_contiguous_target &>>,
	char const *first, char const *last,
	::fast_io::parameter<parameter_contiguous_target &> target) noexcept
{
	target.reference.size = 0u;
	for (auto current{first}; current != last; ++current)
	{
		if (*current == '|')
		{
			return {current + 1u, ::fast_io::parse_code::ok};
		}
		if (target.reference.size == target.reference.value.size())
		{
			return {current, ::fast_io::parse_code::invalid};
		}
		target.reference.value[target.reference.size++] = *current;
	}
	return {last, ::fast_io::parse_code::end_of_file};
}

struct parameter_context_target
{
	::std::array<char, 16u> value{};
	::std::size_t size{};
	::std::size_t commits{};
};

struct parameter_context_state
{
	bool started{};
};

inline constexpr ::fast_io::io_type_t<parameter_context_state> scan_context_type(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<parameter_context_target &>>) noexcept
{
	return {};
}

inline constexpr ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<parameter_context_target &>>,
	parameter_context_state &state, char const *first, char const *last,
	::fast_io::parameter<parameter_context_target &> target) noexcept
{
	if (!state.started)
	{
		state.started = true;
		target.reference.size = 0u;
	}
	for (auto current{first}; current != last; ++current)
	{
		if (*current == '|')
		{
			++target.reference.commits;
			return {current + 1u, ::fast_io::parse_code::ok};
		}
		if (target.reference.size == target.reference.value.size())
		{
			return {current, ::fast_io::parse_code::invalid};
		}
		target.reference.value[target.reference.size++] = *current;
	}
	return {last, ::fast_io::parse_code::partial};
}

inline constexpr ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<parameter_context_target &>>,
	parameter_context_state &state, ::fast_io::parameter<parameter_context_target &> target) noexcept
{
	if (!state.started)
	{
		return ::fast_io::parse_code::end_of_file;
	}
	++target.reference.commits;
	return ::fast_io::parse_code::ok;
}

struct lvalue_alias_target
{};

struct lvalue_alias_proxy
{};

inline constexpr lvalue_alias_proxy scan_alias_define(
	::fast_io::io_alias_t, lvalue_alias_target &) noexcept
{
	return {};
}

struct rvalue_alias_target
{};

struct rvalue_alias_proxy
{};

inline constexpr rvalue_alias_proxy scan_alias_define(
	::fast_io::io_alias_t, rvalue_alias_target &&) noexcept
{
	return {};
}

struct void_alias_target
{};

inline constexpr void scan_alias_define(::fast_io::io_alias_t, void_alias_target &) noexcept
{}

struct nonmovable_value_alias_target
{};

struct nonmovable_value_alias_proxy
{
	nonmovable_value_alias_proxy() = default;
	nonmovable_value_alias_proxy(nonmovable_value_alias_proxy const &) = delete;
	nonmovable_value_alias_proxy(nonmovable_value_alias_proxy &&) = delete;
};

inline constexpr nonmovable_value_alias_proxy scan_alias_define(
	::fast_io::io_alias_t, nonmovable_value_alias_target &) noexcept
{
	return {};
}

struct nonmovable_xvalue_alias_target
{
	nonmovable_value_alias_proxy proxy{};
};

inline constexpr nonmovable_value_alias_proxy &&scan_alias_define(
	::fast_io::io_alias_t, nonmovable_xvalue_alias_target &target) noexcept
{
	return static_cast<nonmovable_value_alias_proxy &&>(target.proxy);
}

struct void_forward_target
{};

inline constexpr void status_io_scan_forward(
	::fast_io::io_alias_type_t<char>, void_forward_target &) noexcept
{}

struct const_category_target;

struct const_category_proxy
{
	const_category_target *target;
};

struct const_category_target
{
	char value{};
	const_category_proxy const proxy{this};
};

inline constexpr const_category_proxy const &scan_alias_define(
	::fast_io::io_alias_t, const_category_target &target) noexcept
{
	return target.proxy;
}

// This CPO intentionally admits only a const proxy lvalue. It distinguishes the argument category used for protocol
// selection from the unqualified representation carried by the reserve tag.
template <typename proxy_type>
	requires ::std::same_as<proxy_type, const_category_proxy const &>
inline constexpr ::fast_io::parse_result<char const *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<char, const_category_proxy>, char const *first,
	char const *last, proxy_type &&proxy) noexcept
{
	if (first == last)
	{
		return {first, ::fast_io::parse_code::end_of_file};
	}
	proxy.target->value = *first;
	return {first + 1u, ::fast_io::parse_code::ok};
}

struct status_value_target;

struct status_value_alias
{
	status_value_target *target;
};

struct status_value_proxy
{
	status_value_target *target;
};

struct status_value_target
{
	char value{};
};

inline constexpr status_value_alias scan_alias_define(
	::fast_io::io_alias_t, status_value_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

// The overload is deliberately rvalue-only. Recognition and invocation must use the same category after aliasing.
inline constexpr status_value_proxy status_io_scan_forward(
	::fast_io::io_alias_type_t<char>, status_value_alias &&alias) noexcept
{
	return {alias.target};
}

inline constexpr ::std::size_t scan_precise_reserve_size(
	::fast_io::io_reserve_type_t<char, status_value_proxy>) noexcept
{
	return 1u;
}

inline constexpr void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, status_value_proxy>, char const *buffer,
	status_value_proxy &proxy) noexcept
{
	proxy.target->value = *buffer;
}

struct status_reference_target;

struct status_reference_alias
{
	status_reference_target *target;

	explicit constexpr status_reference_alias(status_reference_target *address) noexcept
		: target(address)
	{}

	status_reference_alias(status_reference_alias const &) = delete;
	status_reference_alias(status_reference_alias &&) = delete;
};

struct status_reference_proxy
{
	status_reference_target *target;

	explicit constexpr status_reference_proxy(status_reference_target *address) noexcept
		: target(address)
	{}

	status_reference_proxy(status_reference_proxy const &) = delete;
	status_reference_proxy(status_reference_proxy &&) = delete;
};

struct status_reference_target
{
	char value{};
	status_reference_alias alias{this};
	status_reference_proxy proxy{this};
};

inline constexpr status_reference_alias &scan_alias_define(
	::fast_io::io_alias_t, status_reference_target &target) noexcept
{
	return target.alias;
}

inline constexpr status_reference_proxy &status_io_scan_forward(
	::fast_io::io_alias_type_t<char>, status_reference_alias &alias) noexcept
{
	return alias.target->proxy;
}

inline constexpr ::std::size_t scan_precise_reserve_size(
	::fast_io::io_reserve_type_t<char, status_reference_proxy>) noexcept
{
	return 1u;
}

inline constexpr void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, status_reference_proxy>, char const *buffer,
	status_reference_proxy &proxy) noexcept
{
	proxy.target->value = *buffer;
}

// The default-stdin adapter receives an already-normalized proxy. This zero-extent fixture lets the test exercise that
// transport boundary without consuming stdin; deleted copy and move operations make any intermediate by-value decay a
// compile-time error rather than an invisible benchmark cost.
struct default_stdin_zero_proxy
{
	::std::size_t calls{};

	default_stdin_zero_proxy() = default;
	default_stdin_zero_proxy(default_stdin_zero_proxy const &) = delete;
	default_stdin_zero_proxy(default_stdin_zero_proxy &&) = delete;
};

inline constexpr ::std::size_t scan_precise_reserve_size(
	::fast_io::io_reserve_type_t<char, default_stdin_zero_proxy>) noexcept
{
	return 0u;
}

inline constexpr void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, default_stdin_zero_proxy>, char const *,
	default_stdin_zero_proxy &proxy) noexcept
{
	++proxy.calls;
}

struct empty_scatter_producer
{};

inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, empty_scatter_producer>, empty_scatter_producer) noexcept
{
	return {nullptr, 0u};
}

struct empty_contiguous_target
{
	bool called{};
	bool received_valid_empty_range{};
};

inline constexpr ::fast_io::parse_result<char const *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<empty_contiguous_target &>>,
	char const *first, char const *last,
	::fast_io::parameter<empty_contiguous_target &> target) noexcept
{
	target.reference.called = true;
	target.reference.received_valid_empty_range = first != nullptr && first == last;
	return {last, ::fast_io::parse_code::ok};
}

struct empty_context_target
{
	::std::size_t context_calls{};
	::std::size_t eof_calls{};
};

struct empty_context_state
{};

inline constexpr ::fast_io::io_type_t<empty_context_state> scan_context_type(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<empty_context_target &>>) noexcept
{
	return {};
}

inline constexpr ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<empty_context_target &>>,
	empty_context_state &, char const *first, char const *,
	::fast_io::parameter<empty_context_target &> target) noexcept
{
	++target.reference.context_calls;
	return {first, ::fast_io::parse_code::partial};
}

inline constexpr ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<empty_context_target &>>,
	empty_context_state &, ::fast_io::parameter<empty_context_target &> target) noexcept
{
	++target.reference.eof_calls;
	return ::fast_io::parse_code::ok;
}

struct convertible_contiguous_result
{
	inline constexpr operator ::fast_io::parse_result<char const *>() const noexcept
	{
		return {nullptr, ::fast_io::parse_code::invalid};
	}
};

struct wrong_convertible_contiguous
{};

inline constexpr convertible_contiguous_result scan_contiguous_define(
	::fast_io::io_reserve_type_t<char, wrong_convertible_contiguous>, char const *, char const *,
	wrong_convertible_contiguous &) noexcept
{
	return {};
}

struct wrong_mutable_contiguous
{};

inline constexpr ::fast_io::parse_result<char *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<char, wrong_mutable_contiguous>, char const *, char const *,
	wrong_mutable_contiguous &) noexcept
{
	return {nullptr, ::fast_io::parse_code::invalid};
}

struct escaped_contiguous_target
{
	char const *reported_iterator{};
};

inline constexpr ::fast_io::parse_result<char const *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<escaped_contiguous_target &>>,
	char const *, char const *, ::fast_io::parameter<escaped_contiguous_target &> target) noexcept
{
	return {target.reference.reported_iterator, ::fast_io::parse_code::ok};
}

struct valid_advertised_state
{};

struct nondefault_state
{
	nondefault_state() = delete;
};

template <typename state_type>
struct state_advertisement
{};

template <typename state_type>
inline constexpr ::fast_io::io_type_t<state_type> scan_context_type(
	::fast_io::io_reserve_type_t<char, state_advertisement<state_type>>) noexcept
{
	return {};
}

template <typename state_type>
inline constexpr ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, state_advertisement<state_type>>, state_type &,
	char const *first, char const *, state_advertisement<state_type> &) noexcept
{
	return {first, ::fast_io::parse_code::ok};
}

template <typename state_type>
inline constexpr ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, state_advertisement<state_type>>, state_type &,
	state_advertisement<state_type> &) noexcept
{
	return ::fast_io::parse_code::end_of_file;
}

struct missing_state_advertisement
{};

struct missing_state_descriptor
{};

inline constexpr missing_state_descriptor scan_context_type(
	::fast_io::io_reserve_type_t<char, missing_state_advertisement>) noexcept
{
	return {};
}

struct wrong_context_result
{};

inline constexpr ::fast_io::io_type_t<valid_advertised_state> scan_context_type(
	::fast_io::io_reserve_type_t<char, wrong_context_result>) noexcept
{
	return {};
}

inline constexpr int scan_context_define(
	::fast_io::io_reserve_type_t<char, wrong_context_result>, valid_advertised_state &,
	char const *, char const *, wrong_context_result &) noexcept
{
	return 0;
}

inline constexpr ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, wrong_context_result>, valid_advertised_state &,
	wrong_context_result &) noexcept
{
	return ::fast_io::parse_code::end_of_file;
}

struct wrong_eof_result
{};

inline constexpr ::fast_io::io_type_t<valid_advertised_state> scan_context_type(
	::fast_io::io_reserve_type_t<char, wrong_eof_result>) noexcept
{
	return {};
}

inline constexpr ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, wrong_eof_result>, valid_advertised_state &,
	char const *first, char const *, wrong_eof_result &) noexcept
{
	return {first, ::fast_io::parse_code::ok};
}

inline constexpr int scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, wrong_eof_result>, valid_advertised_state &,
	wrong_eof_result &) noexcept
{
	return 0;
}

struct runtime_precise_extent
{};

inline ::std::size_t scan_precise_reserve_size(
	::fast_io::io_reserve_type_t<char, runtime_precise_extent>) noexcept
{
	return 1u;
}

inline constexpr void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, runtime_precise_extent>, char const *,
	runtime_precise_extent &) noexcept
{}

struct wrong_precise_size_type
{};

inline constexpr unsigned scan_precise_reserve_size(
	::fast_io::io_reserve_type_t<char, wrong_precise_size_type>) noexcept
{
	return 1u;
}

inline constexpr void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, wrong_precise_size_type>, char const *,
	wrong_precise_size_type &) noexcept
{}

struct oversized_precise_extent
{};

inline constexpr ::std::size_t scan_precise_reserve_size(
	::fast_io::io_reserve_type_t<char, oversized_precise_extent>) noexcept
{
	return static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)());
}

inline constexpr void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, oversized_precise_extent>, char const *,
	oversized_precise_extent &) noexcept
{}

struct byte_domain_oversized_wide_extent
{};

inline constexpr ::std::size_t scan_precise_reserve_size(
	::fast_io::io_reserve_type_t<char32_t, byte_domain_oversized_wide_extent>) noexcept
{
	return (::std::numeric_limits<::std::size_t>::max)() / sizeof(char32_t) + 1u;
}

inline constexpr void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char32_t, byte_domain_oversized_wide_extent>, char32_t const *,
	byte_domain_oversized_wide_extent &) noexcept
{}

struct wrong_precise_define_result
{};

inline constexpr ::std::size_t scan_precise_reserve_size(
	::fast_io::io_reserve_type_t<char, wrong_precise_define_result>) noexcept
{
	return 1u;
}

inline constexpr int scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, wrong_precise_define_result>, char const *,
	wrong_precise_define_result &) noexcept
{
	return 0;
}

struct fallible_separator_target
{
	bool matched{};
};

inline constexpr ::std::size_t scan_precise_reserve_size(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<fallible_separator_target &>>) noexcept
{
	return 1u;
}

inline constexpr ::fast_io::parse_code scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<fallible_separator_target &>>,
	char const *buffer, ::fast_io::parameter<fallible_separator_target &> target) noexcept
{
	target.reference.matched = *buffer == '|';
	return target.reference.matched ? ::fast_io::parse_code::ok : ::fast_io::parse_code::invalid;
}

struct exact_status_argument
{};

struct exact_status_source
{};

inline constexpr bool status_scan_define(exact_status_source, exact_status_argument &) noexcept
{
	return true;
}

struct wrong_status_source
{};

inline constexpr int status_scan_define(wrong_status_source, exact_status_argument &) noexcept
{
	return 1;
}

struct iterative_only
{};

inline constexpr void scan_iterative_init_define(
	::fast_io::io_reserve_type_t<char, iterative_only>, iterative_only &) noexcept
{}

inline constexpr ::fast_io::parse_result<char const *> scan_iterative_next_define(
	::fast_io::io_reserve_type_t<char, iterative_only>, iterative_only &, char const *first,
	char const *) noexcept
{
	return {first, ::fast_io::parse_code::end_of_file};
}

inline constexpr ::fast_io::parse_code scan_iterative_eof_define(
	::fast_io::io_reserve_type_t<char, iterative_only>, iterative_only &) noexcept
{
	return ::fast_io::parse_code::end_of_file;
}

inline constexpr ::fast_io::parse_result<char const *> scan_iterative_contiguous_define(
	::fast_io::io_reserve_type_t<char, iterative_only>, iterative_only &, char const *first,
	char const *) noexcept
{
	return {first, ::fast_io::parse_code::end_of_file};
}

struct rewind_target
{};

struct rewind_state
{
	::std::size_t phases{};
};

inline constexpr ::fast_io::io_type_t<rewind_state> scan_context_type(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<rewind_target &>>) noexcept
{
	return {};
}

inline constexpr ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<rewind_target &>>, rewind_state &state,
	char const *first, char const *, ::fast_io::parameter<rewind_target &>) noexcept
{
	++state.phases;
	// Consume one character per phase so the final call begins strictly inside the same chunk. EOF rewind must still
	// use the chunk's original lower bound rather than clamping against this last suffix.
	return {first + 1u, ::fast_io::parse_code::partial};
}

inline constexpr ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<rewind_target &>>, rewind_state &,
	::fast_io::parameter<rewind_target &>) noexcept
{
	return ::fast_io::parse_code::invalid;
}

inline constexpr ::std::size_t scan_context_eof_rewind_size(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<rewind_target &>>, rewind_state &,
	::fast_io::parameter<rewind_target &>) noexcept
{
	return 2u;
}

struct eof_partial_target
{};

struct eof_partial_state
{};

inline constexpr ::fast_io::io_type_t<eof_partial_state> scan_context_type(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<eof_partial_target &>>) noexcept
{
	return {};
}

inline constexpr ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<eof_partial_target &>>,
	eof_partial_state &, char const *first, char const *,
	::fast_io::parameter<eof_partial_target &>) noexcept
{
	return {first, ::fast_io::parse_code::partial};
}

inline constexpr ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<eof_partial_target &>>,
	eof_partial_state &, ::fast_io::parameter<eof_partial_target &>) noexcept
{
	return ::fast_io::parse_code::partial;
}

using precise_parameter = ::fast_io::parameter<parameter_fixed_target<4u> &>;
using contiguous_parameter = ::fast_io::parameter<parameter_contiguous_target &>;
using context_parameter = ::fast_io::parameter<parameter_context_target &>;
using separator_parameter = ::fast_io::parameter<fallible_separator_target &>;
using rewind_parameter = ::fast_io::parameter<rewind_target &>;

static_assert(::std::same_as<
			  decltype(::fast_io::io_scan_alias(::std::declval<parameter_fixed_target<4u> &>())),
			  precise_parameter>);
static_assert(::std::same_as<
			  decltype(::fast_io::io_scan_forward<char>(::fast_io::io_scan_alias(
				  ::std::declval<parameter_fixed_target<4u> &>()))),
			  precise_parameter>);
static_assert(::std::same_as<
			  decltype(::fast_io::io_scan_alias(::std::declval<parameter_fixed_target<4u> const &>())),
			  ::fast_io::parameter<parameter_fixed_target<4u> const &>>);
static_assert(::std::same_as<
			  decltype(::fast_io::io_scan_alias(::std::declval<parameter_fixed_target<4u> volatile &>())),
			  ::fast_io::parameter<parameter_fixed_target<4u> volatile &>>);
static_assert(::std::same_as<
			  decltype(::fast_io::io_scan_alias(::std::declval<parameter_fixed_target<4u> &&>())),
			  ::fast_io::parameter<parameter_fixed_target<4u>>>);
static_assert(::std::same_as<
			  decltype(::fast_io::io_scan_alias(::std::declval<precise_parameter &>())),
			  precise_parameter &>);
static_assert(::std::same_as<
			  decltype(::fast_io::io_scan_alias(::std::declval<precise_parameter &&>())),
			  precise_parameter>);
static_assert(::std::same_as<
			  decltype(::fast_io::io_scan_forward<char>(::std::declval<precise_parameter &>())),
			  precise_parameter &>);
static_assert(::std::same_as<
			  decltype(::fast_io::io_scan_forward<char>(::std::declval<precise_parameter &&>())),
			  precise_parameter>);
static_assert(::fast_io::precise_reserve_scannable<char, precise_parameter>);
static_assert(::fast_io::precise_reserve_scannable_no_error<char, precise_parameter>);
static_assert(::fast_io::precise_reserve_scannable<char,
												   ::fast_io::parameter<parameter_fixed_target<0u> &>>);
static_assert(::fast_io::contiguous_scannable<char, contiguous_parameter>);
static_assert(::fast_io::context_scannable<char, context_parameter>);

static_assert(::fast_io::alias_scannable<lvalue_alias_target &>);
static_assert(!::fast_io::alias_scannable<lvalue_alias_target>);
static_assert(!::fast_io::alias_scannable<rvalue_alias_target &>);
static_assert(::fast_io::alias_scannable<rvalue_alias_target>);
static_assert(!::fast_io::alias_scannable<void_alias_target &>);
static_assert(!::fast_io::alias_scannable<nonmovable_value_alias_target &>);
static_assert(!::fast_io::alias_scannable<nonmovable_xvalue_alias_target &>);
static_assert(::fast_io::alias_scannable<const_category_target &>);
static_assert(!::fast_io::contiguous_scannable<char, const_category_proxy>);
static_assert(::fast_io::contiguous_scannable<char, const_category_proxy const>);
static_assert(::std::same_as<
			  decltype(::fast_io::io_scan_alias(::std::declval<rvalue_alias_target &&>())),
			  rvalue_alias_proxy>);
static_assert(::std::same_as<
			  decltype(::fast_io::io_scan_alias(
				  ::std::declval<::scan_concept_harness::reference_alias_target &>())),
			  ::scan_concept_harness::noncopyable_reference_proxy &>);
static_assert(::std::same_as<
			  decltype(::fast_io::io_scan_forward<char>(::fast_io::io_scan_alias(
				  ::std::declval<::scan_concept_harness::reference_alias_target &>()))),
			  ::scan_concept_harness::noncopyable_reference_proxy &>);

static_assert(::fast_io::status_io_scan_forwardable<char, status_value_alias>);
static_assert(!::fast_io::status_io_scan_forwardable<char, status_value_alias &>);
static_assert(!::fast_io::status_io_scan_forwardable<char, void_forward_target &>);
static_assert(::std::same_as<
			  decltype(::fast_io::io_scan_forward<char>(::std::declval<status_value_alias &&>())),
			  status_value_proxy>);
static_assert(!::fast_io::precise_reserve_scannable<char, status_value_alias>);
static_assert(!::fast_io::contiguous_scannable<char, status_value_alias>);
static_assert(!::fast_io::context_scannable<char, status_value_alias>);
static_assert(::fast_io::precise_reserve_scannable<char, status_value_proxy>);
static_assert(::fast_io::status_io_scan_forwardable<char, status_reference_alias &>);
static_assert(!::fast_io::status_io_scan_forwardable<char, status_reference_alias>);
static_assert(::std::same_as<
			  decltype(::fast_io::io_scan_forward<char>(::std::declval<status_reference_alias &>())),
			  status_reference_proxy &>);

static_assert(!::fast_io::contiguous_scannable<char, wrong_convertible_contiguous>);
static_assert(!::fast_io::contiguous_scannable<char, wrong_mutable_contiguous>);

static_assert(::fast_io::context_scannable<char, state_advertisement<valid_advertised_state>>);
static_assert(!::fast_io::context_scannable<char, missing_state_advertisement>);
static_assert(!::fast_io::context_scannable<char, state_advertisement<valid_advertised_state const>>);
static_assert(!::fast_io::context_scannable<char, state_advertisement<valid_advertised_state &>>);
static_assert(!::fast_io::context_scannable<char, state_advertisement<valid_advertised_state[2]>>);
static_assert(!::fast_io::context_scannable<char, state_advertisement<nondefault_state>>);
static_assert(!::fast_io::context_scannable<char, wrong_context_result>);
static_assert(!::fast_io::context_scannable<char, wrong_eof_result>);

static_assert(!::fast_io::precise_reserve_scannable<char, runtime_precise_extent>);
static_assert(!::fast_io::precise_reserve_scannable<char, wrong_precise_size_type>);
static_assert(!::fast_io::precise_reserve_scannable<char, oversized_precise_extent>);
static_assert(!::fast_io::precise_reserve_scannable<char32_t, byte_domain_oversized_wide_extent>);
static_assert(!::fast_io::precise_reserve_scannable<char, wrong_precise_define_result>);
static_assert(::fast_io::precise_reserve_scannable<char, separator_parameter>);
static_assert(!::fast_io::precise_reserve_scannable_no_error<char, separator_parameter>);
static_assert(::fast_io::scatter_printable<char, empty_scatter_producer>);
static_assert(::fast_io::contiguous_scannable<
	char, ::fast_io::parameter<empty_contiguous_target &>>);
static_assert(::fast_io::context_scannable<
	char, ::fast_io::parameter<empty_context_target &>>);

static_assert(::fast_io::operations::decay::defines::has_status_scan_define<
			  exact_status_source, exact_status_argument &>);
static_assert(!::fast_io::operations::decay::defines::has_status_scan_define<
			  wrong_status_source, exact_status_argument &>);

static_assert(::fast_io::iterative_scannable<char, iterative_only>);
static_assert(::fast_io::iterative_contiguous_scannable<char, iterative_only>);
static_assert(!::fast_io::precise_reserve_scannable<char, iterative_only>);
static_assert(!::fast_io::contiguous_scannable<char, iterative_only>);
static_assert(!::fast_io::context_scannable<char, iterative_only>);

static_assert(::fast_io::details::scan_context_eof_rewindable<
			  char, rewind_parameter, rewind_state>);

inline bool bytes_equal(char const *first, ::std::string_view expected) noexcept
{
	for (::std::size_t i{}; i != expected.size(); ++i)
	{
		if (first[i] != expected[i])
		{
			return false;
		}
	}
	return true;
}

inline void test_parameter_protocols()
{
	{
		::std::string_view input_text{"HEAD"};
		::fast_io::basic_ibuffer_view<char> input(input_text);
		parameter_fixed_target<4u> target;
		assert(::fast_io::io::scan<true>(input, target));
		assert(target.calls == 1u && target.received_object_pointer);
		assert(bytes_equal(target.value.data(), "HEAD"));
	}
	{
		::std::string_view empty;
		::fast_io::basic_ibuffer_view<char> input(empty);
		parameter_fixed_target<0u> target;
		assert(::fast_io::io::scan<true>(input, target));
		assert(target.calls == 1u && target.received_object_pointer);
	}
	{
		::std::string_view input_text{"text|tail"};
		::fast_io::basic_ibuffer_view<char> input(input_text);
		parameter_contiguous_target target;
		assert(::fast_io::io::scan<true>(input, target));
		assert(target.size == 4u && bytes_equal(target.value.data(), "text"));
		assert(::std::string_view(
				   input.curr_ptr, static_cast<::std::size_t>(input.end_ptr - input.curr_ptr)) == "tail");
	}
	{
		::scan_concept_harness::bounded_refill_source input;
		input.reset("cross-boundary|tail", 3u);
		parameter_context_target target;
		assert(::fast_io::io::scan<true>(input, target));
		assert(target.size == 14u && bytes_equal(target.value.data(), "cross-boundary"));
		assert(target.commits == 1u);
	}
}

inline void test_alias_and_forward_categories()
{
	{
		::std::string_view input_text{"V"};
		::fast_io::basic_ibuffer_view<char> input(input_text);
		status_value_target target;
		assert(::fast_io::io::scan<true>(input, target));
		assert(target.value == 'V');
	}
	{
		::std::string_view input_text{"R"};
		::fast_io::basic_ibuffer_view<char> input(input_text);
		status_reference_target target;
		assert(::fast_io::io::scan<true>(input, target));
		assert(target.value == 'R');
	}
	{
		::std::string_view input_text{"A"};
		::fast_io::basic_ibuffer_view<char> input(input_text);
		::scan_concept_harness::reference_alias_target target;
		assert(::fast_io::io::scan<true>(input, target));
		assert(target.value == 'A');
	}
	{
		::std::string_view input_text{"C"};
		::fast_io::basic_ibuffer_view<char> input(input_text);
		const_category_target target;
		assert(::fast_io::io::scan<true>(input, target));
		assert(target.value == 'C');
	}
	{
		char input{'P'};
		const_category_target target;
		auto const result{::fast_io::parse_by_scan(
			__builtin_addressof(input), __builtin_addressof(input) + 1u, target)};
		assert(result.code == ::fast_io::parse_code::ok);
		assert(result.iter == __builtin_addressof(input) + 1u);
		assert(target.value == 'P');
	}
	{
		const_category_target target;
		::fast_io::inplace_to(target, "T");
		assert(target.value == 'T');
	}
	{
		::scan_concept_harness::reference_alias_target target;
		// The target alias is an immovable lvalue proxy. `inplace_to` must select the dedicated borrowed-target entry;
		// the value-decay entry remains reserved for prvalue proxies whose ABI transport must not become a reference.
		::fast_io::inplace_to(target, "B");
		assert(target.value == 'B');
	}
	#if __has_include(<stdio.h>)
	{
		default_stdin_zero_proxy proxy;
		assert(::fast_io::details::scan_after_io_scan_forward<true>(proxy));
		assert(proxy.calls == 1u);
	}
	#endif
}

inline void test_result_and_runtime_contracts()
{
	{
		char separator{'|'};
		fallible_separator_target target;
		auto const result{::fast_io::parse_by_scan(
			__builtin_addressof(separator), __builtin_addressof(separator) + 1u, target)};
		assert(result.iter == __builtin_addressof(separator) + 1u);
		assert(result.code == ::fast_io::parse_code::ok && target.matched);
	}
	{
		char nonseparator{'x'};
		fallible_separator_target target;
		auto const result{::fast_io::parse_by_scan(
			__builtin_addressof(nonseparator), __builtin_addressof(nonseparator) + 1u, target)};
		// A precise scanner owns one complete record once it is available, even when validation rejects that record.
		assert(result.iter == __builtin_addressof(nonseparator) + 1u);
		assert(result.code == ::fast_io::parse_code::invalid && !target.matched);
	}
	{
		char storage[3]{'L', 'x', 'R'};
		escaped_contiguous_target target;
		target.reported_iterator = storage + 3u;
		auto const result{::fast_io::parse_by_scan(storage + 1u, storage + 2u, target)};
		assert(result.iter == storage + 1u && result.code == ::fast_io::parse_code::invalid);
	}
	{
		char storage[3]{'L', 'x', 'R'};
		::scan_concept_harness::escaped_context_target target;
		target.reported_iterator = storage + 3u;
		auto const result{::fast_io::parse_by_scan(storage + 1u, storage + 2u, target)};
		assert(result.iter == storage + 1u && result.code == ::fast_io::parse_code::invalid);
	}
	{
		char storage[3]{'L', 'x', 'R'};
		::scan_concept_harness::stalled_context_target target;
		auto const result{::fast_io::parse_by_scan(storage + 1u, storage + 2u, target)};
		assert(result.iter == storage + 1u && result.code == ::fast_io::parse_code::invalid);
	}
	{
		char storage{'x'};
		eof_partial_target target;
		auto const result{::fast_io::parse_by_scan(
			__builtin_addressof(storage), __builtin_addressof(storage), target)};
		assert(result.iter == __builtin_addressof(storage));
		assert(result.code == ::fast_io::parse_code::invalid);
	}
	{
		::scan_concept_harness::status_source input;
		::scan_concept_harness::status_target target;
		assert(::fast_io::io::scan<true>(input, target));
		assert(input.calls == 1u && target.value);
	}
}

inline void test_eof_rewind()
{
#if defined(__cpp_exceptions)
	char storage[2]{'a', 'b'};
	::fast_io::basic_ibuffer_view<char> input(storage, storage + 2u);
	rewind_target target;
	bool rejected{};
	try
	{
		(void)::fast_io::io::scan<true>(input, target);
	}
	catch (::fast_io::error const &error)
	{
		rejected = error == ::fast_io::parse_code::invalid;
	}
	assert(rejected);
	assert(input.curr_ptr == storage);
#endif
}

inline void test_empty_scatter_conversion_bridge()
{
	empty_contiguous_target contiguous;
	::fast_io::inplace_to(contiguous, empty_scatter_producer{});
	assert(contiguous.called && contiguous.received_valid_empty_range);

	empty_context_target context;
	::fast_io::inplace_to(context, empty_scatter_producer{});
	assert(context.context_calls == 0u);
	assert(context.eof_calls == 1u);
}

} // namespace scan_protocol_composition_matrix

int main()
{
	using namespace ::scan_protocol_composition_matrix;
	test_parameter_protocols();
	test_alias_and_forward_categories();
	test_result_and_runtime_contracts();
	test_eof_rewind();
	test_empty_scatter_conversion_bridge();
}
