#pragma once

#if !defined(FAST_IO_UWVM_EXPERIMENTAL_PROOFS) || !FAST_IO_UWVM_EXPERIMENTAL_PROOFS
#error "Rejected experiment: define FAST_IO_UWVM_EXPERIMENTAL_PROOFS=1 only to reproduce the documented policy mismatch"
#endif

#include <fast_io.h>
#include <uwvm2/parser/wasm/standard/wasm1/features/def.h>

// REJECTED INTEGRATION EXPERIMENT. Do not deploy these declarations in uwvm2.
// The differential trace found changed reserve descriptor grouping and, for function signatures, changed context
// capture boundaries. The proposed context-barrier promise below is therefore false for this use. The declarations
// are retained only behind the explicit experiment gate so the compile-memory/policy tradeoff remains reproducible.
// The production fast_io change does not introduce these promises or enable any additional runtime plan.
// Pinned input: uwvm2 commit 4737560818049fb3a46a2e3f8a58ea266065da16, non-module Linux builds only.
// This is not a fast_io public header and does not modify the live uwvm2 checkout. Re-audit it before using another
// snapshot: a new formatter, feature extension, or status-print owner can invalidate a provider's semantic promise.
// The exact direct, reserve, and context categories below were checked independently; they are not interchangeable.
// In particular, these associated namespaces contain no status_print_define in the pinned source tree. The consumer
// continues to check all structural competitors, the actual destination, and exact whole-record status precedence.
// No argument is split at an arbitrary pack length and no formatting operation or optimizer flag is removed.

// These direct-only diagnostic leaves synchronously emit on the supplied observer. Their ordinary control boundary
// already drains the preceding prefix, and they neither retain sources nor mutate later captured condition values.
namespace uwvm2::uwvm::utils::memory
{
struct print_memory;
template <::std::integral Char>
inline constexpr ::std::true_type print_semantic_optional_scatter_barrier_leaf(
	::fast_io::io_reserve_type_t<Char, print_memory>) noexcept
{
	return {};
}
} // namespace uwvm2::uwvm::utils::memory

namespace uwvm2::parser::wasm::base
{
struct error_output_t;
template <::std::integral Char>
inline constexpr ::std::true_type print_semantic_optional_scatter_barrier_leaf(
	::fast_io::io_reserve_type_t<Char, error_output_t>) noexcept
{
	return {};
}
} // namespace uwvm2::parser::wasm::base

namespace uwvm2::validation::error
{
struct error_output_t;
template <::std::integral Char>
inline constexpr ::std::true_type print_semantic_optional_scatter_barrier_leaf(
	::fast_io::io_reserve_type_t<Char, error_output_t>) noexcept
{
	return {};
}
} // namespace uwvm2::validation::error

namespace uwvm2::parser::wasm_custom::customs
{
struct name_error_output_t;
template <::std::integral Char>
inline constexpr ::std::true_type print_semantic_optional_scatter_barrier_leaf(
	::fast_io::io_reserve_type_t<Char, name_error_output_t>) noexcept
{
	return {};
}
} // namespace uwvm2::parser::wasm_custom::customs

namespace uwvm2::utils::cmdline::details
{
struct usage_printer;
inline constexpr ::std::true_type print_semantic_optional_scatter_barrier_leaf(
	::fast_io::io_reserve_type_t<char8_t, usage_printer>) noexcept
{
	return {};
}
} // namespace uwvm2::utils::cmdline::details

namespace uwvm2::parser::wasm::standard::wasm1::type
{
struct limits_type_section_details_wrapper_t;
struct memory_type_section_details_wrapper_t;
struct table_type_section_details_wrapper_t;
struct global_type_section_details_wrapper_t;
template <::std::integral Char, typename T>
	requires(::std::same_as<T, memory_type_section_details_wrapper_t> ||
			 ::std::same_as<T, table_type_section_details_wrapper_t> ||
			 ::std::same_as<T, global_type_section_details_wrapper_t>)
inline constexpr ::std::true_type print_semantic_optional_scatter_status_transparent_leaf(
	::fast_io::io_reserve_type_t<Char, T>) noexcept
{
	// These closed POD summaries have pure reserve size/define pairs. The original reserve route and native scatter
	// grouping remain selected, while this exact-type list excludes external namespace-owning feature extensions.
	return {};
}
template <::std::integral Char>
inline constexpr ::std::true_type print_semantic_optional_scatter_status_transparent_leaf(
	::fast_io::io_reserve_type_t<Char, limits_type_section_details_wrapper_t>) noexcept
{
	// Unlike print_memory, this leaf has a pure dynamic-reserve formatter. It stays in its ordinary coalescible
	// segment; declaring a direct barrier would be false and is correctly rejected by the consumer.
	return {};
}
} // namespace uwvm2::parser::wasm::standard::wasm1::type

namespace uwvm2::uwvm::wasm::type
{
struct module_memory_limit_section_details_wrapper_t;
template <::std::integral Char>
inline constexpr ::std::true_type print_semantic_optional_scatter_barrier_leaf(
	::fast_io::io_reserve_type_t<Char, module_memory_limit_section_details_wrapper_t>) noexcept
{
	return {};
}
} // namespace uwvm2::uwvm::wasm::type

namespace uwvm2::parser::wasm::standard::wasm1p1::features
{
struct wasm1p1;
struct table_type_section_details_wrapper_t;
struct global_type_section_details_wrapper_t;
template <::std::integral Char, typename T>
	requires(::std::same_as<T, table_type_section_details_wrapper_t> ||
			 ::std::same_as<T, global_type_section_details_wrapper_t>)
inline constexpr ::std::true_type print_semantic_optional_scatter_status_transparent_leaf(
	::fast_io::io_reserve_type_t<Char, T>) noexcept
{
	// Both built-in summaries own reserve materialization, not a direct/context boundary.
	return {};
}
} // namespace uwvm2::parser::wasm::standard::wasm1p1::features
namespace uwvm2::parser::wasm::standard::wasm1::features
{
struct wasm1;
template <::std::integral Char, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
	requires((::std::same_as<Fs, wasm1> ||
			  ::std::same_as<Fs, ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1>) &&
			 ...)
inline constexpr ::std::true_type print_semantic_optional_scatter_context_barrier_leaf(
	::fast_io::io_reserve_type_t<Char, final_function_type_section_details_wrapper_t<Fs...>>) noexcept
{
	// Rejected hypothesis: absence of an external ADL namespace is not sufficient to prove a context boundary.
	// The ordinary scanner can capture the adjacent prefix and suffix into the same context window. This marker
	// wrongly suppresses that aggregation; keep it only as the negative-control reproducer, never as an adaptation.
	return {};
}
} // namespace uwvm2::parser::wasm::standard::wasm1::features
