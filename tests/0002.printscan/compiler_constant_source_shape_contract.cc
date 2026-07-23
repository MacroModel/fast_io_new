#include <string>
#include <string_view>
#include <type_traits>

#include <fast_io.h>
#include <fast_io_format.h>

namespace
{

struct marker_only_source
{};

struct borrowed_text_marker_only_source
{};

struct malformed_borrowed_text_marker_source
{};

using precision_source = ::std::remove_cvref_t<decltype(
	::fast_io::mnp::fixed<
		::fast_io::manipulators::floating_precision::
			fractional_preserve_trailing_zero>(3.125, 6u))>;
using fixed_width_precision_source = ::fast_io::manipulators::width_t<
	::fast_io::manipulators::scalar_placement::right, precision_source>;
using fixed_width_fill_precision_source = ::fast_io::manipulators::width_ch_t<
	::fast_io::manipulators::scalar_placement::right,
	precision_source, char>;
using runtime_width_precision_source =
	::fast_io::manipulators::width_runtime_t<precision_source>;
using runtime_width_fill_precision_source =
	::fast_io::manipulators::width_runtime_ch_t<precision_source, char>;
using format_integer_source =
	::fast_io::manipulators::format_scalar_t<int, 0u, false>;
using borrowed_format_source =
	::fast_io::manipulators::format_scalar_t<::std::string, 0u, false>;
inline constexpr auto expanded_format_flags = []() consteval {
	auto flags{
		::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.showbase = true;
	flags.showpos = true;
	flags.floating =
		::fast_io::manipulators::floating_format::hexfloat;
	return flags;
}();
using expanded_format_source =
	::fast_io::manipulators::format_scalar_t<
		::fast_io::manipulators::scalar_manip_t<
			expanded_format_flags, double>,
		2u, true>;
using expanded_format_replacement =
	::fast_io::details::compiler_constant_materialized_t<
		char, expanded_format_source>;

// This primitive marker deliberately supplies no query or materialization CPO.
// It proves that the source-shape concept can be inspected before the complete
// replacement contract without recursively forming that contract.
[[maybe_unused]] inline constexpr ::std::true_type
print_compiler_constant_source_prefer_expanded_fragments(
	::fast_io::io_reserve_type_t<char, marker_only_source>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::true_type
print_compiler_constant_borrowed_text_leaf(
	::fast_io::io_reserve_type_t<char,
		borrowed_text_marker_only_source>) noexcept
{
	return {};
}

// The source-shape protocol requires the exact marker type. A merely
// truth-valued result must not opt a source into a consumer cost partition.
[[maybe_unused]] inline constexpr bool
print_compiler_constant_borrowed_text_leaf(
	::fast_io::io_reserve_type_t<char,
		malformed_borrowed_text_marker_source>) noexcept
{
	return true;
}

static_assert(
	::fast_io::compiler_constant_expanded_fragment_preferred_source_shape<
		char, marker_only_source>);
static_assert(
	!::fast_io::compiler_constant_expanded_fragment_preferred_source<
		char, marker_only_source>);
static_assert(
	::fast_io::compiler_constant_expanded_fragment_preferred_source_shape<
		char, double>);
static_assert(
	::fast_io::compiler_constant_expanded_fragment_preferred_source<
		char, double>);
static_assert(
	::fast_io::compiler_constant_materialization_graph_proven_source_shape<
		char, int>);
static_assert(
	::fast_io::compiler_constant_materialization_graph_proven_source_shape<
		char, double>);
static_assert(
	::fast_io::compiler_constant_simple_scalar_source_shape<
		char, format_integer_source>);
static_assert(
	::fast_io::compiler_constant_materialization_graph_proven_source_shape<
		char, format_integer_source>);
// The format wrapper forwards the primitive source marker without requiring
// the child to expose a complete replacement protocol. This keeps early
// compiler partitions substitution-safe.
static_assert(
	::fast_io::compiler_constant_expanded_fragment_preferred_source_shape<
		char,
		::fast_io::manipulators::format_scalar_t<
			marker_only_source, 2u, true>>);
// A real floating source proves the complete implication: materializing the
// wrapped source produces a wrapped proxy whose expanded fragments spell
// exactly the wrapper's ordinary reserve output.
static_assert(
	::fast_io::compiler_constant_expanded_fragment_preferred_source<
		char, expanded_format_source>);
static_assert(
	::fast_io::compiler_constant_expanded_fragment_preferred<
		char, expanded_format_replacement>);
static_assert(
	!::fast_io::compiler_constant_expanded_fragment_preferred_source_shape<
		char, int>);
static_assert(
	!::fast_io::compiler_constant_expanded_fragment_preferred_source_shape<
		char,
		::fast_io::manipulators::format_scalar_t<int, 0u, false>>);
static_assert(
	::fast_io::compiler_constant_borrowed_text_source_shape<
		char, borrowed_text_marker_only_source>);
static_assert(
	!::fast_io::compiler_constant_borrowed_text_source_shape<
		char, malformed_borrowed_text_marker_source>);
static_assert(
	::fast_io::compiler_constant_borrowed_text_source_shape<
		char, ::std::string>);
static_assert(
	::fast_io::compiler_constant_borrowed_text_source_shape<
		char, ::std::string_view>);
static_assert(
	::fast_io::compiler_constant_borrowed_text_source_shape<
		char, borrowed_format_source>);
// The wrapper itself satisfies the materialization protocol, so this rejection
// is not vacuous. Both the public source boundary and concat's later normalized
// active-source boundary must keep the redundant text proxy structurally absent.
static_assert(
	::fast_io::compiler_constant_printable<char, borrowed_format_source>);
static_assert(
	!::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_candidate_v<
			char, borrowed_format_source>);
static_assert(
	!::fast_io::details::decay::
		basic_general_concat_active_source_codegen_supported<
			char, borrowed_format_source>());
static_assert(
	!::fast_io::details::decay::
		basic_general_concat_normalized_compiler_constant_codegen_supported<
			char, borrowed_format_source>());
static_assert(
	!::fast_io::compiler_constant_borrowed_text_source_shape<
		char,
		::fast_io::manipulators::format_scalar_t<
			::std::string_view, 2u, true>>);
static_assert(
	!::fast_io::compiler_constant_borrowed_text_source_shape<
		char,
		::fast_io::manipulators::format_scalar_t<
			malformed_borrowed_text_marker_source, 0u, false>>);
static_assert(
	!::fast_io::compiler_constant_borrowed_text_source_shape<char, int>);
static_assert(
	::fast_io::compiler_constant_dynamic_precision_floating_source_shape<
		char, fixed_width_precision_source>);
static_assert(
	::fast_io::compiler_constant_dynamic_precision_floating_source_shape<
		char, fixed_width_fill_precision_source>);
static_assert(
	::fast_io::compiler_constant_dynamic_precision_floating_source_shape<
		char, runtime_width_precision_source>);
static_assert(
	::fast_io::compiler_constant_dynamic_precision_floating_source_shape<
		char, runtime_width_fill_precision_source>);

#if defined(__clang__) || !defined(__GNUC__)
// Clang 13--23 retain the complete precision planner in the direct literal
// query root, while native MSVC has no optimizer query. Neither that provider
// nor a width wrapper may claim a proven graph even though their semantic
// replacement contracts remain valid.
static_assert(
	!::fast_io::compiler_constant_materialization_graph_proven_source_shape<
		char, precision_source>);
static_assert(
	!::fast_io::compiler_constant_materialization_graph_proven_source_shape<
		char, fixed_width_precision_source>);
#else
static_assert(
	::fast_io::compiler_constant_materialization_graph_proven_source_shape<
		char, precision_source>);
static_assert(
	::fast_io::compiler_constant_materialization_graph_proven_source_shape<
		char, fixed_width_precision_source>);
#endif

} // namespace

int main() {}
