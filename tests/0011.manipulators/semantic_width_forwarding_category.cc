#include <cassert>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace
{

struct forwarding_counts
{
	::std::size_t lvalue_calls{};
	::std::size_t rvalue_calls{};
};

// This leaf deliberately has a run-time scatter protocol. It lets the strategy trait distinguish the lvalue status
// route without involving an integer or floating conversion algorithm; both representations contain literal text.
struct dynamic_scatter_leaf
{
	char const *base;
	::std::size_t size;
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, dynamic_scatter_leaf>, dynamic_scatter_leaf leaf) noexcept
{
	return leaf.size;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, dynamic_scatter_leaf>, char *destination,
	dynamic_scatter_leaf leaf) noexcept
{
	for (::std::size_t index{}; index != leaf.size; ++index)
	{
		destination[index] = leaf.base[index];
	}
	return destination + leaf.size;
}

[[maybe_unused]] inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, dynamic_scatter_leaf>, dynamic_scatter_leaf) noexcept
{
	return {1u, 0u};
}

[[maybe_unused]] inline constexpr ::fast_io::basic_reserve_scatters_define_result<char> print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, dynamic_scatter_leaf>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	dynamic_scatter_leaf leaf) noexcept
{
	*scatters = {leaf.base, leaf.size};
	return {scatters + 1, reserve};
}

inline constexpr ::std::true_type print_borrowed_reserve_scatters_source(
	::fast_io::io_reserve_type_t<char, dynamic_scatter_leaf>) noexcept
{
	// The descriptor is exactly the caller-owned `leaf.base` interval; this producer has no mutable scratch and later
	// producer calls cannot invalidate it. The marker is required because descriptor shape alone cannot prove that fact.
	return {};
}

struct scatter_source
{
	forwarding_counts *counts;
};

// Ref-qualified status forwarding is intentional. A width emitter holds its normalized node in a named local and
// accesses `node.reference`; consequently this overload is the only route matching the real CPO expression.
inline dynamic_scatter_leaf status_io_print_forward(
	::fast_io::io_alias_type_t<char>, scatter_source &source) noexcept
{
	++source.counts->lvalue_calls;
	return {"S", 1u};
}

inline ::std::string_view status_io_print_forward(
	::fast_io::io_alias_type_t<char>, scatter_source &&source) noexcept
{
	++source.counts->rvalue_calls;
	return "wrong-rvalue-route";
}

struct semantic_source
{
	forwarding_counts *counts;
};

inline auto status_io_print_forward(
	::fast_io::io_alias_type_t<char>, semantic_source &source) noexcept
{
	++source.counts->lvalue_calls;
	return ::fast_io::mnp::pack(::std::string_view{"A"}, ::std::string_view{"B"});
}

inline ::std::string_view status_io_print_forward(
	::fast_io::io_alias_type_t<char>, semantic_source &&source) noexcept
{
	++source.counts->rvalue_calls;
	return "wrong-rvalue-route";
}

using lvalue_scatter_forward = decltype(
	::fast_io::io_print_forward<char>(::std::declval<scatter_source &>()));
using rvalue_scatter_forward = decltype(
	::fast_io::io_print_forward<char>(::std::declval<scatter_source &&>()));
static_assert(::fast_io::dynamic_reserve_scatters_printable<char, lvalue_scatter_forward>);
static_assert(!::fast_io::dynamic_reserve_scatters_printable<char, rvalue_scatter_forward>);

using lvalue_semantic_forward = decltype(
	::fast_io::io_print_forward<char>(::std::declval<semantic_source &>()));
using rvalue_semantic_forward = decltype(
	::fast_io::io_print_forward<char>(::std::declval<semantic_source &&>()));
static_assert(::fast_io::details::decay::print_semantic_node<lvalue_semantic_forward>);
static_assert(!::fast_io::details::decay::print_semantic_node<rvalue_semantic_forward>);

using scatter_width = decltype(::fast_io::mnp::right(::std::declval<scatter_source>(), 4u, '.'));
using semantic_width = decltype(::fast_io::mnp::left(::std::declval<semantic_source>(), 4u, '.'));

// Both lvalue and rvalue top-level nodes enter the emitter through a named forwarding-reference local. These four
// assertions prove that compile-time strategy selection uses that same lvalue member category, rather than selecting
// the negative rvalue status route from the member's declared type or the original node's value category.
static_assert(::fast_io::operations::decay::print_semantic_top_level_width_has_runtime_scatter<
	char, scatter_width &>());
static_assert(::fast_io::operations::decay::print_semantic_top_level_width_has_runtime_scatter<
	char, scatter_width>());
static_assert(::fast_io::operations::decay::print_semantic_top_level_width_has_semantic_child<
	char, semantic_width &>());
static_assert(::fast_io::operations::decay::print_semantic_top_level_width_has_semantic_child<
	char, semantic_width>());

} // namespace

int main()
{
	forwarding_counts scatter_counts;
	auto scatter_field{
		::fast_io::mnp::right(scatter_source{__builtin_addressof(scatter_counts)}, 4u, '.')};
	assert(::fast_io::concat_std(scatter_field) == "...S");
	assert(scatter_counts.lvalue_calls != 0u);
	assert(scatter_counts.rvalue_calls == 0u);

	forwarding_counts semantic_counts;
	// Passing a temporary width node is the regression case: its stored child is still an lvalue while the emitter
	// measures and emits it. Accidentally classifying it as an rvalue selects a different status CPO before output.
	assert(::fast_io::concat_std(
		::fast_io::mnp::left(semantic_source{__builtin_addressof(semantic_counts)}, 4u, '.')) == "AB..");
	assert(semantic_counts.lvalue_calls != 0u);
	assert(semantic_counts.rvalue_calls == 0u);
}
