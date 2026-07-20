#include <fast_io_format.h>

#include <bit>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <type_traits>

namespace
{

template <::fast_io::manipulators::floating_format format, bool uppercase,
		  bool alternate_form>
[[nodiscard]] consteval ::fast_io::manipulators::scalar_flags
general_child_flags() noexcept
{
	::fast_io::manipulators::scalar_flags flags{};
	flags.floating = format;
	flags.uppercase = uppercase;
	flags.uppercase_e = uppercase;
	flags.precision = alternate_form
						  ? ::fast_io::manipulators::floating_precision::
								significant_preserve_trailing_zero
						  : ::fast_io::manipulators::floating_precision::significant;
	return flags;
}

template <bool uppercase, bool alternate_form>
[[nodiscard]] constexpr auto make_general(double value,
										  ::std::size_t precision) noexcept
{
	constexpr auto fixed_flags{general_child_flags<
		::fast_io::manipulators::floating_format::fixed, uppercase,
		alternate_form>()};
	constexpr auto scientific_flags{general_child_flags<
		::fast_io::manipulators::floating_format::scientific, uppercase,
		alternate_form>()};
	using fixed_scalar = ::fast_io::manipulators::scalar_manip_precision_t<
		fixed_flags, double>;
	using scientific_scalar =
		::fast_io::manipulators::scalar_manip_precision_t<scientific_flags,
														  double>;
	using fixed_formatted =
		::fast_io::manipulators::format_scalar_t<fixed_scalar, 0u, false>;
	using scientific_formatted = ::fast_io::manipulators::format_scalar_t<
		scientific_scalar, 0u, false>;
	return ::fast_io::fmt::details::make_general_float<alternate_form>(
		fixed_formatted{fixed_scalar{value, precision}},
		scientific_formatted{scientific_scalar{value, precision}}, precision);
}

template <::std::integral char_type, typename general_type>
[[nodiscard]] constexpr char_type *emit_reference(
	char_type *iter, general_type const &value) noexcept
{
	using fixed_type = ::std::remove_cvref_t<decltype(value.fixed)>;
	using scientific_type =
		::std::remove_cvref_t<decltype(value.scientific)>;
	auto apply_alternate = [&](char_type *end) constexpr noexcept {
		if constexpr (general_type::preserves_trailing_zero)
		{
			return ::fast_io::fmt::details::general_float_apply_alternate(
				iter, end);
		}
		else
		{
			return end;
		}
	};
	bool const initial_scientific{
		::fast_io::fmt::details::general_float_initial_scientific(
			value.fixed, value.precision)};
	if (initial_scientific)
	{
		auto end{print_reserve_define(
			::fast_io::io_reserve_type<char_type, scientific_type>, iter,
			value.scientific)};
		end = apply_alternate(end);
		auto const exponent{
			::fast_io::fmt::details::general_float_rounded_exponent(iter, end)};
		if (::fast_io::fmt::details::general_float_uses_scientific(
				exponent, value.precision))
		{
			return end;
		}
		auto selected_end{print_reserve_define(
			::fast_io::io_reserve_type<char_type, fixed_type>, iter,
			value.fixed)};
		return apply_alternate(selected_end);
	}
	auto end{print_reserve_define(
		::fast_io::io_reserve_type<char_type, fixed_type>, iter, value.fixed)};
	end = apply_alternate(end);
	auto const exponent{
		::fast_io::fmt::details::general_float_rounded_exponent(iter, end)};
	if (!::fast_io::fmt::details::general_float_uses_scientific(
			exponent, value.precision))
	{
		return end;
	}
	auto selected_end{print_reserve_define(
		::fast_io::io_reserve_type<char_type, scientific_type>, iter,
		value.scientific)};
	return apply_alternate(selected_end);
}

template <::std::integral char_type, bool uppercase, bool alternate_form>
[[nodiscard]] bool optimized_matches_reference(
	double value, ::std::size_t precision)
{
	char_type optimized[1024u]{};
	char_type reference[1024u]{};
	auto const general{make_general<uppercase, alternate_form>(
		value, precision == 0u ? 1u : precision)};
	using general_type = ::std::remove_cvref_t<decltype(general)>;
	auto const optimized_end{print_reserve_define(
		::fast_io::io_reserve_type<char_type, general_type>, optimized,
		general)};
	auto const reference_end{emit_reference(reference, general)};
	auto const optimized_size{
		static_cast<::std::size_t>(optimized_end - optimized)};
	auto const reference_size{
		static_cast<::std::size_t>(reference_end - reference)};
	return optimized_size == reference_size &&
		   ::std::memcmp(optimized, reference,
						 optimized_size * sizeof(char_type)) == 0;
}

template <::std::integral char_type>
[[nodiscard]] bool all_presentations_match(
	double value, ::std::size_t precision)
{
	return optimized_matches_reference<char_type, false, false>(
			   value, precision) &&
		   optimized_matches_reference<char_type, true, false>(value, precision) &&
		   optimized_matches_reference<char_type, false, true>(value, precision);
}

[[nodiscard]] bool neighborhood_matches(double center,
										::std::size_t precision)
{
	auto value{center};
	for (unsigned index{}; index != 16u; ++index)
	{
		if (!all_presentations_match<char>(value, precision) ||
			!all_presentations_match<char>(-value, precision))
		{
			return false;
		}
		value = ::std::nextafter(value,
								 -::std::numeric_limits<double>::infinity());
	}
	value = center;
	for (unsigned index{}; index != 16u; ++index)
	{
		if (!all_presentations_match<char>(value, precision) ||
			!all_presentations_match<char>(-value, precision))
		{
			return false;
		}
		value = ::std::nextafter(value,
								 ::std::numeric_limits<double>::infinity());
	}
	return true;
}

} // namespace

int main()
{
	double const special_values[]{
		0.0,
		-0.0,
		::std::numeric_limits<double>::infinity(),
		-::std::numeric_limits<double>::infinity(),
		::std::numeric_limits<double>::quiet_NaN(),
		::std::numeric_limits<double>::denorm_min(),
		::std::numeric_limits<double>::min(),
		::std::numeric_limits<double>::max(),
		1.0,
		-1.0,
		12345.678901,
		-12345.678901};
	for (::std::size_t precision{}; precision != 21u; ++precision)
	{
		for (double value : special_values)
		{
			if (!all_presentations_match<char>(value, precision) ||
				!all_presentations_match<char16_t>(value, precision))
			{
				return 1;
			}
		}
		if (!neighborhood_matches(1e-4, precision) ||
			!neighborhood_matches(5e-5, precision))
		{
			return 1;
		}
		auto const normalized_precision{precision == 0u ? 1u : precision};
		auto const upper{::std::pow(
			10.0, static_cast<double>(normalized_precision))};
		if (!neighborhood_matches(upper, precision) ||
			!neighborhood_matches(upper / 2.0, precision))
		{
			return 1;
		}
	}

	::std::uint64_t state{0x6a09e667f3bcc909ULL};
	for (::std::size_t index{}; index != 50000u; ++index)
	{
		state ^= state << 13u;
		state ^= state >> 7u;
		state ^= state << 17u;
		auto const value{::std::bit_cast<double>(state)};
		auto const precision{static_cast<::std::size_t>((state >> 58u) % 21u)};
		if (!all_presentations_match<char>(value, precision))
		{
			return 1;
		}
	}
	for (::std::size_t precision : {50u, 100u, 600u})
	{
		for (double value : special_values)
		{
			if (!all_presentations_match<char>(value, precision))
			{
				return 1;
			}
		}
	}
}
