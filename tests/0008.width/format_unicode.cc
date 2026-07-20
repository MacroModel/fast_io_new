#include <fast_io_format.h>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

template <typename char_type, ::std::size_t output_size, ::std::size_t fill_size>
constexpr bool direct_format_fill_test(
	::std::array<char_type, output_size> expected,
	::std::array<char_type, fill_size> fill, ::std::size_t repetitions)
{
	auto const options{::fast_io::fmt::details::make_text_field_options<char_type>(
		SIZE_MAX, 0u, ::fast_io::manipulators::scalar_placement::left,
		fill.data(), fill.size())};
	::std::array<char_type, output_size> output{};
	auto const end{::fast_io::fmt::details::emit_format_fill(
		output.data(), options, repetitions)};
	return end == output.data() + output.size() && output == expected;
}

static_assert(direct_format_fill_test<char, 5u, 1u>(
	::std::array<char, 5u>{'*', '*', '*', '*', '*'},
	::std::array<char, 1u>{'*'}, 5u));

static_assert(direct_format_fill_test<char8_t, 8u, 4u>(
	::std::array<char8_t, 8u>{0xf0u, 0x9fu, 0x98u, 0x80u,
							  0xf0u, 0x9fu, 0x98u, 0x80u},
	::std::array<char8_t, 4u>{0xf0u, 0x9fu, 0x98u, 0x80u}, 2u));

inline constexpr ::fast_io::fmt::basic_fixed_string pattern_width_format{
	u8"{:😀>5}"};
using pattern_width_argument =
	decltype(::fast_io::mnp::static_arg<42u>);
using static_pattern_width =
	::fast_io::fmt::details::compiled_static_format_program<
		pattern_width_format, ::fast_io::fmt::brace_fmt_t,
		pattern_width_argument>;
static_assert(::std::u8string_view{static_pattern_width::storage.data(),
								   static_pattern_width::size} == u8"😀😀😀42");

constexpr auto ascii_precision{
	::fast_io::fmt::details::measure_unicode_prefix(u8"aa", 2u, 1u)};
static_assert(ascii_precision.storage_size == 1u &&
			  ascii_precision.display_width == 1u);

int main()
{
	auto const ascii{::fast_io::fmt::concat_std<"[{:*>{}}]">(
		::std::string_view{"xy"}, 4095u)};
	if (ascii.size() != 4097u || ascii.front() != '[' || ascii.back() != ']' ||
		ascii[4093u] != '*' || ascii[4094u] != 'x' || ascii[4095u] != 'y')
	{
		return 1;
	}

	auto const utf8_fill{::fast_io::fmt::u8concat_std<u8"[{:😀>{}}]">(
		::std::u8string_view{u8"xy"}, 5u)};
	if (utf8_fill != ::std::u8string_view{u8"[😀😀😀xy]"})
	{
		return 2;
	}

	auto const scalar_utf8_fill{
		::fast_io::fmt::u8concat_std<pattern_width_format>(42u)};
	if (scalar_utf8_fill != ::std::u8string_view{
			static_pattern_width::storage.data(), static_pattern_width::size})
	{
		return 8;
	}

	auto const truncated{::fast_io::fmt::u8concat_std<u8"{:.2}">(
		::std::u8string_view{u8"界a"})};
	if (truncated != ::std::u8string_view{u8"界"})
	{
		return 3;
	}

	::std::string source(2048u, 'a');
	auto const long_field{::fast_io::fmt::concat_std<"{:*>4096}">(
		::std::string_view{source})};
	if (long_field.size() != 4096u || long_field[2047u] != '*' ||
		long_field[2048u] != 'a' || long_field.back() != 'a')
	{
		return 4;
	}

	char8_t volatile volatile_ascii[]{u8'a', u8'b', u8'c'};
	auto const volatile_measurement{
		::fast_io::fmt::details::measure_unicode_prefix(
			volatile_ascii, 3u, SIZE_MAX)};
	if (volatile_measurement.storage_size != 3u ||
		volatile_measurement.display_width != 3u)
	{
		return 5;
	}

	char8_t volatile volatile_fill[]{u8'*', u8'+'};
	char8_t volatile volatile_fill_output[6u]{};
	auto const volatile_fill_end{::fast_io::fmt::details::emit_format_fill_impl(
		volatile_fill_output, volatile_fill, 2u, 3u)};
	if (volatile_fill_end != volatile_fill_output + 6u ||
		volatile_fill_output[0u] != u8'*' || volatile_fill_output[1u] != u8'+' ||
		volatile_fill_output[4u] != u8'*' || volatile_fill_output[5u] != u8'+')
	{
		return 6;
	}

	char8_t volatile volatile_content[]{u8'x', u8'y'};
	::fast_io::fmt::details::basic_unicode_text_field<char8_t volatile> const
		volatile_field{{volatile_content, 2u}, {.storage_size = 2u}};
	char8_t volatile volatile_content_output[2u]{};
	auto const volatile_content_end{
		::fast_io::fmt::details::emit_unicode_text_field(
			volatile_content_output, volatile_field)};
	if (volatile_content_end != volatile_content_output + 2u ||
		volatile_content_output[0u] != u8'x' ||
		volatile_content_output[1u] != u8'y')
	{
		return 7;
	}
}
