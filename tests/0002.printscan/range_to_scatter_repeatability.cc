#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace
{

/// A valid one-shot scatter producer which is intentionally neither borrowed nor repeatable.
/// The large padding keeps print forwarding from copying the source into a register-sized value: both observations in
/// an unsafe two-pass strategy therefore reach the same counter and expose the length disagreement deterministically.
struct changing_scatter
{
	::std::size_t observations{};
	char first[1u]{'A'};
	char later[4u]{'B', 'B', 'B', 'B'};
	char transport_padding[32u]{};
};

inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, changing_scatter>, changing_scatter &source) noexcept
{
	if (source.observations++ == 0u)
	{
		return {source.first, 1u};
	}
	return {source.later, 4u};
}

struct captured_text
{
	::std::string value;
};

inline ::fast_io::parse_result<char const *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<captured_text &>>, char const *first,
	char const *last, ::fast_io::parameter<captured_text &> target)
{
	target.reference.value.assign(first, last);
	return {last, ::fast_io::parse_code::ok};
}

struct context_captured_text
{
	::std::string value;
};

struct context_capture_state
{};

inline constexpr ::fast_io::io_type_t<context_capture_state> scan_context_type(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<context_captured_text &>>) noexcept
{
	return {};
}

inline ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<context_captured_text &>>, context_capture_state &,
	char const *first, char const *last, ::fast_io::parameter<context_captured_text &> target)
{
	target.reference.value.append(first, last);
	return {last, ::fast_io::parse_code::partial};
}

inline constexpr ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<context_captured_text &>>, context_capture_state &,
	::fast_io::parameter<context_captured_text &>) noexcept
{
	return ::fast_io::parse_code::ok;
}

/// Public scatter compatibility is deliberately rvalue-only, while the reserve protocol accepts the named object used
/// by `basic_inplace_to_decay`. Strategy selection must therefore choose reserve materialization rather than instantiate
/// an lvalue call to the scatter CPO.
struct rvalue_scatter_with_reserve
{};

inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, rvalue_scatter_with_reserve>,
	rvalue_scatter_with_reserve &&) noexcept
{
	return {"X", 1u};
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, rvalue_scatter_with_reserve>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, rvalue_scatter_with_reserve>, char *out,
	rvalue_scatter_with_reserve) noexcept
{
	*out = 'R';
	return out + 1u;
}

struct large_static_reserve
{};

inline constexpr ::std::size_t
print_reserve_size(::fast_io::io_reserve_type_t<char, large_static_reserve>) noexcept
{
	return 32u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, large_static_reserve>, char *out,
	large_static_reserve) noexcept
{
	for (::std::size_t i{}; i != 32u; ++i)
	{
		*out++ = 'S';
	}
	return out;
}

struct small_dynamic_reserve
{
	::std::string_view value;
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, small_dynamic_reserve>, small_dynamic_reserve source) noexcept
{
	return source.value.size();
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, small_dynamic_reserve>, char *out,
	small_dynamic_reserve source) noexcept
{
	for (char ch : source.value)
	{
		*out++ = ch;
	}
	return out;
}

using changing_array = ::std::array<changing_scatter, 2u>;
using changing_iterator = ::std::ranges::iterator_t<changing_array &>;
using changing_sized_view = ::fast_io::sized_range_view_t<char, changing_iterator>;
using changing_range = decltype(::fast_io::mnp::rgvw(
	::std::declval<changing_array &>(), ::std::declval<char const (&)[2u]>()));

// Both scatter calls are syntactically valid. Provenance, not shape or forward iteration, is what rejects two passes.
static_assert(::fast_io::range_two_pass_scatter_printable_v<
	char, typename changing_sized_view::forwarded_expression_type>);
static_assert(!::fast_io::sized_range_view_borrowed_scatter_source_v<char, changing_iterator>);
static_assert(!::fast_io::sized_range_view_two_pass_scatter_element_v<char, changing_iterator>);
static_assert(::std::same_as<
	changing_range, ::fast_io::range_view_t<char, changing_iterator>>);

static_assert(::fast_io::scatter_printable<char, rvalue_scatter_with_reserve>);
static_assert(!::fast_io::scatter_printable_for<char, rvalue_scatter_with_reserve &>);
static_assert(!::fast_io::details::to_named_scatter_printable_v<
	char, rvalue_scatter_with_reserve>);
static_assert(::fast_io::details::to_two_pass_fragment_available_v<
	char, rvalue_scatter_with_reserve>);

inline void test_range_uses_one_pass()
{
	changing_array sources{};
	auto range{::fast_io::mnp::rgvw(sources, ",")};
	assert(::fast_io::concat_std(range) == "A,A");
	assert(sources[0].observations == 1u);
	assert(sources[1].observations == 1u);
}

inline void test_to_uses_one_pass_for_unmarked_scatter()
{
	changing_scatter source;
	captured_text target;
	::fast_io::inplace_to(target, source, "|tail");
	assert(target.value == "A|tail");
	assert(source.observations == 1u);
}

inline void test_to_uses_the_exact_named_category()
{
	captured_text target;
	::fast_io::inplace_to(target, rvalue_scatter_with_reserve{});
	assert(target.value == "R");
}

inline void test_mixed_reserve_capacity_proofs()
{
	large_static_reserve large;
	small_dynamic_reserve small{"D"};
	::std::string const expected(32u, 'S');

	captured_text contiguous;
	::fast_io::inplace_to(contiguous, large, small);
	assert(contiguous.value == expected + "D");

	context_captured_text context;
	::fast_io::inplace_to(context, large, small);
	assert(context.value == expected + "D");
}

} // namespace

int main()
{
	test_range_uses_one_pass();
	test_to_uses_one_pass_for_unmarked_scatter();
	test_to_uses_the_exact_named_category();
	test_mixed_reserve_capacity_proofs();
}
