#include <cstddef>
#include <type_traits>

#include <fast_io.h>
#include <fast_io_format.h>

namespace
{

consteval auto fixed_preserve_flags(
	::fast_io::manipulators::floating_rounding rounding =
		::fast_io::manipulators::floating_rounding::nearest_to_even)
{
	auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.floating = ::fast_io::manipulators::floating_format::fixed;
	flags.precision = ::fast_io::manipulators::floating_precision::
		fractional_preserve_trailing_zero;
	flags.rounding = rounding;
	return flags;
}

consteval auto fixed_compact_flags()
{
	auto flags{fixed_preserve_flags()};
	flags.precision =
		::fast_io::manipulators::floating_precision::fractional;
	return flags;
}

consteval auto hex_flags(
	::fast_io::manipulators::floating_rounding rounding =
		::fast_io::manipulators::floating_rounding::nearest_to_even)
{
	auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.floating = ::fast_io::manipulators::floating_format::hexfloat;
	flags.precision = ::fast_io::manipulators::floating_precision::fractional;
	flags.rounding = rounding;
	return flags;
}

template <::std::integral char_type, auto flags, ::std::size_t precision>
consteval bool selected(double value = 0.0)
{
	using source_type =
		::fast_io::manipulators::scalar_manip_precision_t<flags, double>;
	constexpr auto tag{::fast_io::io_reserve_type<char_type, source_type>};
	return print_compiler_constant_materialization_eligible(
		tag, source_type{value, precision});
}

template <::std::integral char_type>
consteval bool check_character_capacity()
{
	constexpr ::std::size_t capacity{
		::fast_io::details::compiler_constant_decimal_precision_capacity<
			char_type>};
	static_assert(2u <= capacity);
	// fixed(0), preserving P digits, is exactly `0.` plus P zeroes.
	return selected<char_type, fixed_preserve_flags(), capacity - 2u>() &&
		!selected<char_type, fixed_preserve_flags(), capacity - 1u>() &&
		selected<char_type, fixed_preserve_flags(), 50u>() ==
			(52u <= capacity) &&
		selected<char_type, fixed_preserve_flags(), 200u>() ==
			(202u <= capacity) &&
		!selected<char_type, fixed_preserve_flags(), 256u>() &&
		!selected<char_type, fixed_preserve_flags(), 600u>() &&
		!selected<char_type, fixed_preserve_flags(), 1025u>() &&
		// P itself may exceed the byte budget when the selected precision mode
		// does not preserve padding.  The actual spelling is one code unit.
		selected<char_type, fixed_compact_flags(), 600u>() &&
		selected<char_type, fixed_compact_flags(), 1024u>();
}

static_assert(check_character_capacity<char>());
static_assert(check_character_capacity<wchar_t>());
static_assert(check_character_capacity<char8_t>());
static_assert(check_character_capacity<char16_t>());
static_assert(check_character_capacity<char32_t>());
static_assert(selected<char, hex_flags(), 40u>(1.25));
static_assert(!selected<char, hex_flags(), 41u>(1.25));

using current_decimal = ::fast_io::manipulators::scalar_manip_precision_t<
	fixed_preserve_flags(
		::fast_io::manipulators::floating_rounding::current_environment),
	double>;
using current_hex = ::fast_io::manipulators::scalar_manip_precision_t<
	hex_flags(::fast_io::manipulators::floating_rounding::current_environment),
	double>;
static_assert(!::fast_io::compiler_constant_printable<char, current_decimal>);
static_assert(!::fast_io::compiler_constant_printable<char, current_hex>);

using precision_source = ::fast_io::manipulators::scalar_manip_precision_t<
	fixed_preserve_flags(), double>;
constexpr precision_source constant_source{1.25, 3u};
constexpr auto precision_proxy{print_compiler_constant_materialize(
	::fast_io::io_reserve_type<char, precision_source>, constant_source)};
using precision_proxy_type = ::std::remove_cv_t<decltype(precision_proxy)>;
using scalar_proxy_type =
	::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
		char, ::fast_io::manipulators::floating_point_default_scalar_flags,
		double>;
constexpr auto scalar_two{
	::fast_io::details::compiler_constant_floating_scalar_materialize<
		char, ::fast_io::manipulators::floating_point_default_scalar_flags>(2.0)};
constexpr auto scalar_three_point_two{
	::fast_io::details::compiler_constant_floating_scalar_materialize<
		char, ::fast_io::manipulators::floating_point_default_scalar_flags>(3.2)};

template <typename value_type>
concept prefers_precise_compact = requires {
	{
		print_compiler_constant_prefer_precise_compact(
			::fast_io::io_reserve_type<char, value_type>)
	} -> ::std::same_as<::std::true_type>;
};

static_assert(prefers_precise_compact<precision_proxy_type>);
static_assert(prefers_precise_compact<scalar_proxy_type>);
constexpr auto scalar_two_fragment{
	print_compiler_constant_single_static_fragment(
		::fast_io::io_reserve_type<char, scalar_proxy_type>, scalar_two)};
constexpr auto scalar_three_point_two_fragment{
	print_compiler_constant_single_static_fragment(
		::fast_io::io_reserve_type<char, scalar_proxy_type>,
		scalar_three_point_two)};
static_assert(scalar_two_fragment.len == 1u &&
	*scalar_two_fragment.base == ::fast_io::char_literal_v<u8'2', char>);
static_assert(scalar_three_point_two_fragment.len == 0u);
static_assert(noexcept(print_reserve_precise_size(
	::fast_io::io_reserve_type<char, precision_proxy_type>, precision_proxy)));
static_assert(noexcept(print_reserve_precise_define(
	::fast_io::io_reserve_type<char, precision_proxy_type>,
	static_cast<char *>(nullptr), ::std::size_t{}, precision_proxy)));

using format_proxy = ::fast_io::manipulators::format_scalar_t<
	precision_proxy_type, 0u, false>;
static_assert(prefers_precise_compact<format_proxy>);

#if defined(__GNUC__) || defined(__clang__)
[[gnu::noinline]]
#endif
bool unknown_value(double value) noexcept
{
	precision_source source{value, 3u};
	return print_compiler_constant_materialization_eligible(
		::fast_io::io_reserve_type<char, precision_source>, source);
}

#if defined(__GNUC__) || defined(__clang__)
[[gnu::noinline]]
#endif
bool unknown_precision(::std::size_t precision) noexcept
{
	precision_source source{1.25, precision};
	return print_compiler_constant_materialization_eligible(
		::fast_io::io_reserve_type<char, precision_source>, source);
}

} // namespace

int main(int argc, char **)
{
	return unknown_value(static_cast<double>(argc)) ||
		unknown_precision(static_cast<::std::size_t>(argc))
		? 1
		: 0;
}
