// Measures only the exact-status type graph, with no selected-record emitter bodies. Every optional leaf has a
// distinct static extent so all 2^N active subsequences have distinct type packs. Header parsing is included.
#include <fast_io.h>

namespace fast_io::operations::decay
{
#include <fast_io_core_impl/operations/printimpl/print_semantic_status_probe.h>
}

#ifndef FAST_IO_COMPILE_CONDITIONS
#define FAST_IO_COMPILE_CONDITIONS 8
#endif

namespace semantic_status_probe_compile
{
struct output
{
	using output_char_type = char;
};

template <::std::size_t index>
using optional = decltype(::fast_io::details::decay::print_semantic_input_forward<char>(
	::fast_io::mnp::cond(false, ::fast_io::manipulators::static_scatter_t<char, index + 2u>{})));

template <::std::size_t... I>
consteval bool probe(::std::index_sequence<I...>)
{
	return ::fast_io::operations::decay::print_semantic_any_exact_status<false, char, output, optional<I>...>();
}

static_assert(!probe(::std::make_index_sequence<FAST_IO_COMPILE_CONDITIONS>{}));
} // namespace semantic_status_probe_compile
