#include <cstddef>
#include <type_traits>

#include <fast_io.h>
#include <fast_io_format.h>

namespace
{

struct marker_only_proxy
{};

struct malformed_boolean_proxy
{};

template <::std::integral char_type>
[[nodiscard]] inline constexpr ::std::true_type
	print_compiler_constant_flat_integer_replacement(
		::fast_io::io_reserve_type_t<char_type, marker_only_proxy>) noexcept
{
	return {};
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type,
								 malformed_boolean_proxy>) noexcept
{
	return 1u;
}

template <::std::integral char_type>
inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type, malformed_boolean_proxy>,
	char_type *iter, malformed_boolean_proxy) noexcept
{
	*iter = ::fast_io::char_literal_v<u8'0', char_type>;
	return iter + 1;
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char_type, malformed_boolean_proxy>,
	malformed_boolean_proxy) noexcept
{
	return 1u;
}

template <::std::integral char_type>
inline constexpr char_type *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char_type, malformed_boolean_proxy>,
	char_type *iter, ::std::size_t, malformed_boolean_proxy &) noexcept
{
	*iter = ::fast_io::char_literal_v<u8'0', char_type>;
	return iter + 1;
}

// A truth-valued result is deliberately insufficient. Consumers require the
// exact proof token after the complete non-throwing precise protocol is valid.
template <::std::integral char_type>
[[nodiscard]] inline constexpr bool
	print_compiler_constant_flat_integer_replacement(
		::fast_io::io_reserve_type_t<char_type,
									 malformed_boolean_proxy>) noexcept
{
	return true;
}

using direct_integer_proxy =
	::fast_io::manipulators::compiler_constant_scalar_manip_t<
		::fast_io::manipulators::integral_default_scalar_flags, unsigned>;
using materialized_integer =
	::fast_io::details::compiler_constant_materialized_t<char, unsigned>;
using formatted_integer =
	::fast_io::manipulators::format_scalar_t<
		materialized_integer, 2u, true>;
using boolalpha_source =
	::std::remove_cvref_t<decltype(::fast_io::mnp::boolalpha(true))>;
using boolalpha_replacement =
	::fast_io::details::compiler_constant_materialized_t<
		char, boolalpha_source>;
using dynamic_precision_source = ::std::remove_cvref_t<decltype(::fast_io::mnp::fixed<
																::fast_io::manipulators::floating_precision::
																	fractional_preserve_trailing_zero>(
	3.125, 6u))>;
using dynamic_precision_replacement =
	::fast_io::details::compiler_constant_materialized_t<
		char, dynamic_precision_source>;
using width_replacement = ::fast_io::manipulators::width_t<
	::fast_io::manipulators::scalar_placement::right,
	materialized_integer>;
using timestamp_replacement =
	::fast_io::details::compiler_constant_materialized_t<
		char, ::fast_io::unix_timestamp>;

static_assert(
	::fast_io::compiler_constant_flat_integer_replacement<
		char, direct_integer_proxy>);
static_assert(
	::fast_io::compiler_constant_flat_integer_replacement<
		char, materialized_integer>);
static_assert(
	::fast_io::compiler_constant_flat_integer_replacement<
		char, formatted_integer>);

static_assert(
	!::fast_io::compiler_constant_flat_integer_replacement<
		char, boolalpha_replacement>);
static_assert(
	!::fast_io::compiler_constant_flat_integer_replacement<
		char, dynamic_precision_replacement>);
static_assert(
	!::fast_io::compiler_constant_flat_integer_replacement<
		char, width_replacement>);

// The marker cannot stand alone: the concept also requires the exact
// non-throwing pointer-reporting reserve protocol.
static_assert(
	!::fast_io::compiler_constant_flat_integer_replacement<
		char, marker_only_proxy>);
static_assert(
	::fast_io::nothrow_precise_reserve_printable<
		char, malformed_boolean_proxy>);
static_assert(
	!::fast_io::compiler_constant_flat_integer_replacement<
		char, malformed_boolean_proxy>);

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ == 11
static_assert(
	::fast_io::details::decay::
		basic_general_concat_compiler_constant_replacement_codegen_supported<
			char, materialized_integer>());
// The GCC 11 exception is intentionally non-compositional. No assembly proof
// exists for either two exact integer leaves or a mixed replacement graph.
static_assert(
	!::fast_io::details::decay::
		basic_general_concat_compiler_constant_replacement_codegen_supported<
			char, materialized_integer, materialized_integer>());
static_assert(
	!::fast_io::details::decay::
		basic_general_concat_compiler_constant_replacement_codegen_supported<
			char, materialized_integer, timestamp_replacement>());
#endif

} // namespace

int main()
{}
