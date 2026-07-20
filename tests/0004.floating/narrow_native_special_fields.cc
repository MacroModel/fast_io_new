#include <bit>
#include <cfenv>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include <fast_io_freestanding.h>

#pragma STDC FENV_ACCESS ON

namespace
{

#if defined(__STDCPP_FLOAT16_T__) || defined(__FLT16_MANT_DIG__)

template <bool uppercase>
inline constexpr auto decimal_flags = []() constexpr noexcept {
	auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.floating = ::fast_io::manipulators::floating_format::general;
	flags.showpos = true;
	flags.uppercase = uppercase;
	flags.nan_show_sign = true;
	flags.nan_show_type = true;
	flags.precision =
		::fast_io::manipulators::floating_precision::significant;
	return flags;
}();

// Half precision is a native binary32-core field domain.  Scalar formatting
// consumes its original fields directly, while runtime-precision formatting
// constructs the exact binary32 carrier from those integer fields.  Neither
// route needs an AArch64 FCVT that could quiet a signaling NaN.
using half_type = _Float16;
static_assert(
	::fast_io::details::print_floating_decimal_direct_supported<half_type>);
static_assert(
	!::fast_io::details::print_floating_decimal_via_float<half_type>);
static_assert(!::fast_io::details::
				  print_floating_requires_object_field_capture<half_type>);
static_assert(!::fast_io::details::
				  print_floating_decimal_requires_integer_transport<half_type>);

#if defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE) && defined(__clang__) && \
	(defined(__aarch64__) || defined(_M_ARM64)) &&                    \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
// The AArch64 scalar and owning aggregate ABIs preserve both native 16-bit
// formats by value.  The borrowed-parameter workaround is strictly the x86
// Clang bfloat16 lowering exception.
static_assert(
	::fast_io::details::print_floating_decimal_direct_supported<__bf16>);
static_assert(
	!::fast_io::details::print_floating_decimal_via_float<__bf16>);
static_assert(!::fast_io::details::
				  print_floating_requires_object_field_capture<__bf16>);
static_assert(!::fast_io::details::
				  print_floating_decimal_requires_integer_transport<__bf16>);
#endif

template <::std::integral char_type>
[[nodiscard]] bool equals(char_type const *first, char_type const *last,
						  ::std::u8string_view expected) noexcept
{
	if (static_cast<::std::size_t>(last - first) != expected.size())
	{
		return false;
	}
	for (::std::size_t index{}; index != expected.size(); ++index)
	{
		if (first[index] !=
			::fast_io::char_literal<char_type>(expected[index]))
		{
			return false;
		}
	}
	return true;
}

template <typename floating_type, ::std::integral char_type, bool uppercase>
#if __has_cpp_attribute(gnu::noinline)
[[gnu::noinline]]
#endif
[[nodiscard]] bool check_value(::std::uint_least16_t raw,
							   ::std::u8string_view expected) noexcept
{
	constexpr auto flags{decimal_flags<uppercase>};
	using scalar_type =
		::fast_io::manipulators::scalar_manip_t<flags, floating_type>;
	using precision_type =
		::fast_io::manipulators::scalar_manip_precision_t<flags, floating_type>;
	using scalar_tag = ::fast_io::io_reserve_type_t<char_type, scalar_type>;
	using precision_tag =
		::fast_io::io_reserve_type_t<char_type, precision_type>;
	using scalar_parameter = ::std::conditional_t<
		::fast_io::details::print_floating_decimal_requires_integer_transport<
			floating_type>,
		scalar_type const &,
		::std::conditional_t<
			::fast_io::details::print_floating_requires_object_field_capture<
				floating_type>,
			::fast_io::details::floating_precise_field_parameter<
				scalar_type, floating_type>,
			scalar_type>>;
	using precision_parameter = ::std::conditional_t<
		::fast_io::details::print_floating_decimal_requires_integer_transport<
			floating_type>,
		precision_type const &,
		::std::conditional_t<
			::fast_io::details::print_floating_requires_object_field_capture<
				floating_type>,
			::fast_io::details::floating_precise_field_parameter<
				precision_type, floating_type>,
			precision_type>>;
	static_assert(::std::same_as<
				  ::fast_io::details::floating_precise_parameter_t<scalar_type, floating_type>,
				  scalar_parameter>);
	static_assert(::std::same_as<
				  ::fast_io::details::floating_precise_parameter_t<precision_type, floating_type>,
				  precision_parameter>);

	auto const value{::std::bit_cast<floating_type>(raw)};
	scalar_type scalar{value};
	precision_type precision{value, 3u};
	char_type ordinary[64u]{};
	char_type precise[64u]{};

	::std::feclearexcept(FE_ALL_EXCEPT);
	auto const scalar_end{::fast_io::print_reserve_define(
		scalar_tag{}, ordinary, scalar)};
	if (::std::fetestexcept(FE_INVALID) != 0 ||
		!equals(ordinary, scalar_end, expected))
	{
		return false;
	}
	auto const scalar_size{::fast_io::print_reserve_precise_size(
		scalar_tag{}, scalar)};
	::std::feclearexcept(FE_ALL_EXCEPT);
	auto const scalar_precise_end{::fast_io::print_reserve_precise_define(
		scalar_tag{}, precise, scalar_size, scalar)};
	if (::std::fetestexcept(FE_INVALID) != 0 || scalar_size != expected.size() ||
		!equals(precise, scalar_precise_end, expected))
	{
		return false;
	}

	::std::feclearexcept(FE_ALL_EXCEPT);
	auto const precision_end{::fast_io::print_reserve_define(
		precision_tag{}, ordinary, precision)};
	if (::std::fetestexcept(FE_INVALID) != 0 ||
		!equals(ordinary, precision_end, expected))
	{
		return false;
	}
	auto const precision_size{::fast_io::print_reserve_precise_size(
		precision_tag{}, precision)};
	::std::feclearexcept(FE_ALL_EXCEPT);
	auto const precision_precise_end{::fast_io::print_reserve_precise_define(
		precision_tag{}, precise, precision_size, precision)};
	return ::std::fetestexcept(FE_INVALID) == 0 &&
		   precision_size == expected.size() &&
		   equals(precise, precision_precise_end, expected);
}

template <typename floating_type, ::std::integral char_type>
[[nodiscard]] bool check_character_type(::std::uint_least16_t signaling,
										::std::uint_least16_t negative_signaling,
										::std::uint_least16_t indeterminate,
										::std::uint_least16_t infinity) noexcept
{
	return check_value<floating_type, char_type, false>(signaling, u8"+nan(snan)") &&
		   check_value<floating_type, char_type, false>(negative_signaling, u8"-nan(snan)") &&
		   check_value<floating_type, char_type, false>(indeterminate, u8"-nan(ind)") &&
		   check_value<floating_type, char_type, false>(infinity, u8"+inf") &&
		   check_value<floating_type, char_type, true>(signaling, u8"+NAN(SNAN)") &&
		   check_value<floating_type, char_type, true>(indeterminate, u8"-NAN(IND)") &&
		   check_value<floating_type, char_type, true>(infinity, u8"+INF");
}

template <typename floating_type>
[[nodiscard]] bool check_all_character_types(
	::std::uint_least16_t signaling,
	::std::uint_least16_t negative_signaling,
	::std::uint_least16_t indeterminate,
	::std::uint_least16_t infinity) noexcept
{
	return check_character_type<floating_type, char>(signaling, negative_signaling,
													 indeterminate, infinity) &&
		   check_character_type<floating_type, wchar_t>(signaling, negative_signaling,
														indeterminate, infinity) &&
		   check_character_type<floating_type, char8_t>(signaling, negative_signaling,
														indeterminate, infinity) &&
		   check_character_type<floating_type, char16_t>(signaling, negative_signaling,
														 indeterminate, infinity) &&
		   check_character_type<floating_type, char32_t>(signaling, negative_signaling,
														 indeterminate, infinity);
}

#endif

} // namespace

int main()
{
#if defined(__STDCPP_FLOAT16_T__) || defined(__FLT16_MANT_DIG__)
	if (!check_all_character_types<half_type>(0x7c01u, 0xfc01u, 0xfe00u,
											  0x7c00u))
	{
		return 1;
	}
#if defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE) && defined(__clang__)
	if (!check_all_character_types<__bf16>(0x7f81u, 0xff81u, 0xffc0u,
										   0x7f80u))
	{
		return 2;
	}
#endif
	return 0;
#else
	return 0;
#endif
}
