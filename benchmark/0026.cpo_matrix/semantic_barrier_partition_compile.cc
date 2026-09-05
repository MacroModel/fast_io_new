#include <fast_io.h>
#include <utility>

#ifndef FAST_IO_COMPILE_PACK
#define FAST_IO_COMPILE_PACK 128
#endif

namespace semantic_partition_compile
{
struct sink
{
	using output_char_type = char;
};

void scatter_write_all_overflow_define(sink, ::fast_io::basic_io_scatter_t<char> const *, ::std::size_t);
void write_all_overflow_define(sink, char const *, char const *);

// This compile-only observer has no status owner. Its protocol declarations model the same synchronous native
// scatter boundaries as the runtime contract fixtures; no formatter or destination implementation is needed here.
::std::true_type print_semantic_optional_scatter_plan_stream(::fast_io::io_reserve_type_t<char, sink>);
::std::true_type print_semantic_optional_scatter_barrier_plan_stream(::fast_io::io_reserve_type_t<char, sink>);
::std::true_type print_synchronous_direct_scatter_output(::fast_io::io_reserve_type_t<char, sink>);

template <::std::size_t Index>
struct barrier
{};

template <::std::size_t Index>
void print_define(::fast_io::io_reserve_type_t<char, barrier<Index>>, sink, barrier<Index>);

// Every indexed leaf is direct-only and this namespace has no status-print customization. The library still checks
// all competing protocols and the concrete output independently; the marker changes no CPO priority.
template <::std::size_t Index>
::std::true_type print_semantic_optional_scatter_barrier_leaf(::fast_io::io_reserve_type_t<char, barrier<Index>>);

using optional = decltype(::fast_io::mnp::cond(false, "color"));
using literal = ::fast_io::manipulators::static_scatter_t<char, 3>;

template <::std::size_t Index>
using source = ::std::conditional_t<Index % 16u == 15u, barrier<Index>,
									::std::conditional_t<Index % 2u == 0u, optional, literal>>;

template <::std::size_t... Index>
consteval bool prove(::std::index_sequence<Index...>)
{
	return ::fast_io::details::decay::print_semantic_optional_scatter_barrier_plan_available<
		true, char, sink, source<Index>...>();
}

// Each sixteen-source block contains eight optional colors, seven mandatory literals, and one direct boundary.
// Scaling this assertion measures proof construction alone, not optimizer work or a changed runtime strategy.
static_assert(FAST_IO_COMPILE_PACK >= 16 && FAST_IO_COMPILE_PACK % 16 == 0);
static_assert(prove(::std::make_index_sequence<FAST_IO_COMPILE_PACK>{}));
} // namespace semantic_partition_compile
