#include <cstddef>
#include <type_traits>

#include <fast_io_freestanding.h>

namespace
{

template <::std::integral char_type>
[[nodiscard]] inline constexpr bool check_character_arithmetic() noexcept
{
	for (char8_t digit{}; digit != 10u; ++digit)
	{
		if (::fast_io::char_literal_add<char_type>(digit) !=
			::fast_io::char_literal<char_type>(
				static_cast<char8_t>(u8'0' + digit)))
		{
			return false;
		}
	}
	for (char8_t digit{}; digit != 6u; ++digit)
	{
		if (::fast_io::char_literal_add<char_type, u8'a'>(digit) !=
				::fast_io::char_literal<char_type>(
					static_cast<char8_t>(u8'a' + digit)) ||
			::fast_io::char_literal_add<char_type, u8'A'>(digit) !=
				::fast_io::char_literal<char_type>(
					static_cast<char8_t>(u8'A' + digit)))
		{
			return false;
		}
	}
	return true;
}

template <::std::integral char_type, ::std::size_t base, bool uppercase,
		  bool modern_octal, ::std::size_t extent>
[[nodiscard]] inline constexpr bool check_show_base(
	char8_t const (&expected)[extent]) noexcept
{
	char_type storage[2u]{};
	auto const end{::fast_io::details::print_reserve_show_base_impl<
		base, uppercase, modern_octal>(storage)};
	if (end != storage + (extent - 1u))
	{
		return false;
	}
	for (::std::size_t index{}; index + 1u != extent; ++index)
	{
		if (storage[index] !=
			::fast_io::char_literal<char_type>(expected[index]))
		{
			return false;
		}
	}
	return true;
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr bool check_integer_prefixes() noexcept
{
	return check_show_base<char_type, 2u, false, false>(u8"0b") &&
		   check_show_base<char_type, 2u, true, false>(u8"0B") &&
		   check_show_base<char_type, 3u, false, false>(u8"0t") &&
		   check_show_base<char_type, 3u, true, false>(u8"0T") &&
		   check_show_base<char_type, 8u, false, true>(u8"0o") &&
		   check_show_base<char_type, 8u, true, true>(u8"0O") &&
		   check_show_base<char_type, 8u, false, false>(u8"0") &&
		   check_show_base<char_type, 16u, false, false>(u8"0x") &&
		   check_show_base<char_type, 16u, true, false>(u8"0X");
}

template <typename floating_type>
[[nodiscard]] inline constexpr bool same_fields(
	floating_type left, floating_type right) noexcept
{
	auto const lhs{::fast_io::details::get_punned_result(left)};
	auto const rhs{::fast_io::details::get_punned_result(right)};
	return lhs.mantissa == rhs.mantissa && lhs.exponent == rhs.exponent &&
		   lhs.sign == rhs.sign;
}

template <::std::integral char_type, ::std::size_t extent,
		  typename manipulator_type,
		  typename floating_type>
[[nodiscard]] bool check(
	char8_t const (&text)[extent], manipulator_type manipulator,
	floating_type const &value, floating_type expected,
	::std::size_t expected_consumed) noexcept
{
	using scanner_type = ::std::remove_cvref_t<manipulator_type>;
	char_type input[extent]{};
	for (::std::size_t index{}; index != extent; ++index)
	{
		input[index] = ::fast_io::char_literal<char_type>(text[index]);
	}
	auto const result{::fast_io::scan_contiguous_define(
		::fast_io::io_reserve_type<char_type, scanner_type>, input,
		input + extent - 1u, manipulator)};
	return result.code == ::fast_io::parse_code::ok &&
		   static_cast<::std::size_t>(result.iter - input) == expected_consumed &&
		   same_fields(value, expected);
}

template <::std::integral char_type>
[[nodiscard]] bool check_decimal_getters() noexcept
{
	using rounding = ::fast_io::manipulators::floating_rounding;
	using precision = ::fast_io::manipulators::floating_precision;

	double decimal_value{};
	auto decimal{
		::fast_io::manipulators::comma_decimal_get(decimal_value)};
	static_assert(::fast_io::contiguous_scannable<char_type, decltype(decimal)>);
	static_assert(::fast_io::context_scannable<char_type, decltype(decimal)>);
	if (!check<char_type>(u8"1,25e2|", decimal, decimal_value, 125.0, 6u))
	{
		return false;
	}

	// Comma mode is an actual grammar choice, not an additional accepted
	// separator.  A dot therefore terminates this token after its integer part.
	double dot_value{};
	auto dot{::fast_io::manipulators::comma_decimal_get(dot_value)};
	if (!check<char_type>(u8"1.25|", dot, dot_value, 1.0, 1u))
	{
		return false;
	}

	double plus_value{};
	auto plus{::fast_io::manipulators::comma_decimal_get<
		rounding::nearest_to_even, true>(plus_value)};
	if (!check<char_type>(u8"+0,5|", plus, plus_value, 0.5, 4u))
	{
		return false;
	}

	double fixed_value{};
	auto fixed{::fast_io::manipulators::comma_fixed_get(fixed_value)};
	if (!check<char_type>(u8"-12,5e2|", fixed, fixed_value, -12.5, 5u))
	{
		return false;
	}

	double scientific_value{};
	auto scientific{
		::fast_io::manipulators::comma_scientific_get(scientific_value)};
	if (!check<char_type>(
			u8"1,25E+2|", scientific, scientific_value, 125.0, 7u))
	{
		return false;
	}

	double precision_value{};
	auto precision_scanner{
		::fast_io::manipulators::comma_decimal_get<
			precision::fractional, rounding::nearest_to_even>(
			precision_value, 4u)};
	static_assert(::fast_io::terminal_contiguous_padding_scannable<
				  char_type, decltype(precision_scanner)>);
	return check<char_type>(
		u8"1,2500|", precision_scanner, precision_value, 1.25, 6u);
}

template <::std::integral char_type>
[[nodiscard]] bool check_hex_getters() noexcept
{
	using rounding = ::fast_io::manipulators::floating_rounding;

	double value{};
	auto scanner{::fast_io::manipulators::comma_hexfloat_get(value)};
	static_assert(::fast_io::contiguous_scannable<char_type, decltype(scanner)>);
	static_assert(::fast_io::context_scannable<char_type, decltype(scanner)>);
	if (!check<char_type>(u8"1,8p+1|", scanner, value, 3.0, 6u))
	{
		return false;
	}

	double prefixed_value{};
	auto prefixed{
		::fast_io::manipulators::comma_hexfloat0x_get(prefixed_value)};
	if (!check<char_type>(
			u8"0x1,8p+1|", prefixed, prefixed_value, 3.0, 8u))
	{
		return false;
	}

	double rounded_value{};
	auto rounded{::fast_io::manipulators::comma_hexfloat_get<
		rounding::toward_zero>(rounded_value)};
	return check<char_type>(u8"1,4p+1|", rounded, rounded_value, 2.5, 6u);
}

template <::std::integral char_type>
[[nodiscard]] bool check_type() noexcept
{
	return check_character_arithmetic<char_type>() &&
		   check_integer_prefixes<char_type>() &&
		   check_decimal_getters<char_type>() && check_hex_getters<char_type>();
}

} // namespace

int main()
{
	return check_type<char>() && check_type<wchar_t>() && check_type<char8_t>() &&
				   check_type<char16_t>() && check_type<char32_t>() &&
				   check_type<signed char>() && check_type<unsigned char>()
			   ? 0
			   : 1;
}
