#include <cassert>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace
{

struct forwarding_counts
{
	::std::size_t lvalue_calls{};
};

struct lvalue_source
{
	forwarding_counts *counts;
};

// The normalized print entry owns the complete semantic node before dispatch. A by-value pack member is consequently
// a named lvalue when status forwarding runs, irrespective of the public pack's original value category.
inline constexpr ::fast_io::basic_io_scatter_t<char> status_io_print_forward(
	::fast_io::io_alias_type_t<char>, lvalue_source &source) noexcept
{
	++source.counts->lvalue_calls;
	return {"L", 1u};
}

struct rvalue_source
{};

// This opposite-category customization is a negative admission probe. It is callable while constructing an rvalue
// expression, but not from the stored member expression used by the semantic emitter.
[[maybe_unused]] inline constexpr ::fast_io::basic_io_scatter_t<char> status_io_print_forward(
	::fast_io::io_alias_type_t<char>, rvalue_source &&) noexcept
{
	return {"wrong-rvalue-route", 18u};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, lvalue_source>) noexcept
{
	// The final descriptor refers only to a string literal, so it remains valid for the complete print operation.
	return {};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, rvalue_source>) noexcept
{
	return {};
}

using lvalue_pack = decltype(::fast_io::mnp::pack(::std::declval<lvalue_source>()));
using rvalue_pack = decltype(::fast_io::mnp::pack(::std::declval<rvalue_source>()));

static_assert(::fast_io::details::decay::print_semantic_params_okay<char, lvalue_pack>::value);
static_assert(!::fast_io::details::decay::print_semantic_params_okay<char, rvalue_pack>::value);

template <typename T>
inline ::std::string render(T &&value)
{
	::std::string result;
	::fast_io::ostring_ref_std output{__builtin_addressof(result)};
	::fast_io::print(output, ::std::forward<T>(value));
	return result;
}

} // namespace

int main()
{
	forwarding_counts counts;

	auto stored{::fast_io::mnp::pack(lvalue_source{__builtin_addressof(counts)})};
	assert(render(stored) == "L");
	assert(counts.lvalue_calls == 1u);

	// A temporary top-level pack is still owned and named before expansion; its element must not regain rvalueness.
	assert(render(::fast_io::mnp::pack(lvalue_source{__builtin_addressof(counts)})) == "L");
	assert(counts.lvalue_calls == 2u);

	// Width retains the complete pack as another named member. This nested case exercises the same category proof
	// through both structural traits before the pack is flattened into its active leaf.
	assert(render(::fast_io::mnp::right(
		::fast_io::mnp::pack(lvalue_source{__builtin_addressof(counts)}), 3u, '.')) == "..L");
	assert(counts.lvalue_calls != 0u);
}
