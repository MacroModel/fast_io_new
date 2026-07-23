#include <type_traits>

#include <fast_io.h>
#include <fast_io_dsal/string.h>

namespace test
{

struct unclassified_source
{
	unsigned value;
};

struct malformed_graph_source
{
	unsigned value;
};

struct classified_source
{
	unsigned value;
};

struct replacement
{
	char value;
};

template <typename T>
concept source_type =
	::std::same_as<T, unclassified_source> ||
	::std::same_as<T, malformed_graph_source> ||
	::std::same_as<T, classified_source>;

template <::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type, replacement>) noexcept
{
	return 1u;
}

template <::std::integral char_type>
inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type, replacement>,
	char_type *iter, replacement value) noexcept
{
	*iter = static_cast<char_type>(value.value);
	return iter + 1;
}

template <::std::integral char_type, source_type T>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_query_inline_safe(
	::fast_io::io_reserve_type_t<char_type, T>) noexcept
{
	return {};
}

template <::std::integral char_type, source_type T>
[[nodiscard]] inline constexpr bool
print_compiler_constant_materialization_eligible(
	::fast_io::io_reserve_type_t<char_type, T>, T const &value) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
	return __builtin_constant_p(value.value) && value.value < 10u;
#else
	(void)value;
	return false;
#endif
}

template <::std::integral char_type, source_type T>
[[nodiscard]] inline constexpr replacement
print_compiler_constant_materialize(
	::fast_io::io_reserve_type_t<char_type, T>, T const &value) noexcept
{
	return {static_cast<char>('0' + value.value)};
}

template <::std::integral char_type, source_type T>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_pre_normalization_safe(
	::fast_io::io_reserve_type_t<char_type, T>) noexcept
{
	return {};
}

template <::std::integral char_type, source_type T>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_simple_scalar_source(
	::fast_io::io_reserve_type_t<char_type, T>) noexcept
{
	return {};
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_graph_proven(
	::fast_io::io_reserve_type_t<char_type, classified_source>) noexcept
{
	return {};
}

// A truth-valued result is deliberately insufficient. The provider must return
// the exact proof token so accidental semantic-only extensions stay fail-closed.
template <::std::integral char_type>
[[nodiscard]] inline constexpr bool
print_compiler_constant_materialization_graph_proven(
	::fast_io::io_reserve_type_t<char_type, malformed_graph_source>) noexcept
{
	return true;
}

static_assert(
	::fast_io::compiler_constant_pre_normalization_safe<
		char, unclassified_source>);
static_assert(
	::fast_io::compiler_constant_pre_normalization_safe<
		char, malformed_graph_source>);
static_assert(
	::fast_io::compiler_constant_pre_normalization_safe<
		char, classified_source>);

static_assert(
	!::fast_io::compiler_constant_materialization_graph_proven_source_shape<
		char, unclassified_source>);
static_assert(
	!::fast_io::compiler_constant_materialization_graph_proven_source_shape<
		char, malformed_graph_source>);
static_assert(
	::fast_io::compiler_constant_materialization_graph_proven<
		char, classified_source>);

static_assert(
	!::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_candidate_v<
			char, unclassified_source>);
static_assert(
	!::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_candidate_v<
			char, malformed_graph_source>);

#if defined(__GNUC__) && !defined(__clang__)
static_assert(
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_candidate_v<
			char, classified_source>);
#elif defined(__clang__) && 21 <= __clang_major__
static_assert(
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_candidate_v<
			char, classified_source>);
#else
static_assert(
	!::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_candidate_v<
			char, classified_source>);
#endif

// These are independent normalized consumers. Neither may infer a deletion
// proof merely because the semantic materializer is well-formed.
static_assert(
	!::fast_io::operations::decay::
		print_compiler_constant_materialization_available<
			false, ::fast_io::basic_obuffer_view_ref<char>,
			unclassified_source>());
static_assert(
	!::fast_io::details::decay::
		basic_general_concat_compiler_constant_materialization_available<
			false, char, ::fast_io::string, unclassified_source>());
static_assert(
	!::fast_io::details::inplace_to_compiler_constant_source_available<
		char, unsigned, unclassified_source>());

} // namespace test

int main() {}
