#include <cstddef>
#include <limits>

#include <fast_io_freestanding.h>

namespace
{

template <bool decorated>
inline constexpr auto flags = []() constexpr noexcept {
	auto result{::fast_io::manipulators::floating_point_default_scalar_flags};
	result.floating = ::fast_io::manipulators::floating_format::hexfloat;
	result.precision = ::fast_io::manipulators::floating_precision::fractional;
	result.showbase = decorated;
	result.showpos = decorated;
	result.uppercase = decorated;
	result.uppercase_e = decorated;
	result.uppercase_showbase = decorated;
	result.comma = decorated;
	return result;
}();

template <::std::integral char_type, bool decorated>
[[nodiscard]] bool check_value(double value) noexcept
{
	constexpr auto selected_flags{flags<decorated>};
	constexpr auto maximum{(::std::numeric_limits<::std::size_t>::max)()};
	char_type actual[128u]{};
	char_type expected[128u]{};
	auto const fields{
		::fast_io::details::compiler_constant_floating_capture_fields(value)};
	auto const actual_end{
		::fast_io::details::compiler_constant_hex_precision_fields_runtime_define<
			selected_flags>(actual, fields, maximum)};
	auto const expected_end{
		::fast_io::details::print_rsvhexfloat_precision_define_impl<
			selected_flags.showbase, selected_flags.uppercase_showbase,
			selected_flags.showpos, selected_flags.uppercase,
			selected_flags.uppercase_e, selected_flags.comma,
			selected_flags.rounding, selected_flags.precision,
			selected_flags.nan_show_sign, selected_flags.nan_show_type>(
				expected, value, maximum)};
	auto const actual_size{static_cast<::std::size_t>(actual_end - actual)};
	auto const expected_size{static_cast<::std::size_t>(expected_end - expected)};
	if (actual_size != expected_size ||
		actual_size !=
			::fast_io::details::compiler_constant_hex_precision_fields_size_impl<
				selected_flags>(fields, maximum))
	{
		return false;
	}
	for (::std::size_t index{}; index != actual_size; ++index)
	{
		if (actual[index] != expected[index])
		{
			return false;
		}
	}
	return true;
}

template <::std::integral char_type>
[[nodiscard]] bool check_character_type() noexcept
{
	constexpr double values[]{0.0, -0.0, 1.0, -1.5,
		(::std::numeric_limits<double>::denorm_min)(),
		(::std::numeric_limits<double>::max)(),
		(::std::numeric_limits<double>::infinity)(),
		(::std::numeric_limits<double>::quiet_NaN)()};
	for (auto const value : values)
	{
		if (!check_value<char_type, false>(value) ||
			!check_value<char_type, true>(value))
		{
			return false;
		}
	}
	return true;
}

} // namespace

int main()
{
	return !(check_character_type<char>() && check_character_type<wchar_t>() &&
		check_character_type<char8_t>() && check_character_type<char16_t>() &&
		check_character_type<char32_t>());
}
