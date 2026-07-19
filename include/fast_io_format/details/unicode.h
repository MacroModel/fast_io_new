#pragma once

#include "semantic.h"
#include "fixed_string.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace fast_io::fmt::details
{

/**
 * The result of decoding one source character.
 *
 * `code_units` is deliberately independent of `display_width`.  UTF-8 may use
 * four storage elements for a scalar which occupies one terminal column, while
 * a CJK scalar may use one UTF-32 element and occupy two columns.  Conflating
 * these quantities either splits an encoding at a precision boundary or
 * under-allocates the final concat destination.
 */
struct decoded_unicode_scalar
{
	char32_t code_point{};
	::std::size_t code_units{1u};
	bool valid{};
};

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr ::std::uint_least32_t
format_unicode_code_unit_value(char_type value) noexcept
{
	using clean_type = ::std::remove_cv_t<char_type>;
	using unsigned_type = ::std::make_unsigned_t<clean_type>;
	auto result{static_cast<unsigned_type>(value)};
	if constexpr (::std::same_as<clean_type, wchar_t> &&
				  ::fast_io::details::wide_is_none_execution_endian)
	{
		result = ::fast_io::byte_swap(result);
	}
	return static_cast<::std::uint_least32_t>(result);
}

[[nodiscard]] inline constexpr bool unicode_scalar_value(
	::std::uint_least32_t value) noexcept
{
	return value <= 0x10ffffu && !(0xd800u <= value && value <= 0xdfffu);
}

[[nodiscard]] inline constexpr bool utf8_continuation(
	::std::uint_least32_t value) noexcept
{
	return (value & 0xc0u) == 0x80u;
}

/**
 * Decodes one scalar without reading beyond `remaining`.
 *
 * Invalid input consumes exactly one code unit.  This recovery rule is needed
 * by both truncation and debug escaping: it guarantees progress and preserves
 * every original storage element instead of silently replacing or skipping a
 * malformed suffix.
 *
 * Non-Unicode execution characters intentionally take the conservative
 * branch.  An ordinary format string value does not prove that its argument
 * uses a particular implementation-defined single-byte code page.  Treating
 * each code unit as one display column is therefore the only encoding-neutral
 * promise here.
 */
template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr decoded_unicode_scalar decode_unicode_scalar(
	char_type const *first, ::std::size_t remaining) noexcept
{
	if (remaining == 0u)
	{
		return {};
	}

	using clean_type = ::std::remove_cv_t<char_type>;
	auto const first_value{format_unicode_code_unit_value(*first)};
	if constexpr (!::fast_io::details::is_unicode_execution_charset<clean_type>)
	{
		return {static_cast<char32_t>(first_value), 1u, true};
	}
	else if constexpr (::std::same_as<clean_type, char> ||
					   ::std::same_as<clean_type, char8_t> || sizeof(clean_type) == 1u)
	{
		if (first_value <= 0x7fu)
		{
			return {static_cast<char32_t>(first_value), 1u, true};
		}
		if (0xc2u <= first_value && first_value <= 0xdfu && remaining >= 2u)
		{
			auto const second{format_unicode_code_unit_value(first[1u])};
			if (utf8_continuation(second))
			{
				return {static_cast<char32_t>(((first_value & 0x1fu) << 6u) |
											  (second & 0x3fu)),
						2u, true};
			}
		}
		else if (0xe0u <= first_value && first_value <= 0xefu && remaining >= 3u)
		{
			auto const second{format_unicode_code_unit_value(first[1u])};
			auto const third{format_unicode_code_unit_value(first[2u])};
			bool const canonical_second{
				(first_value != 0xe0u || second >= 0xa0u) &&
				(first_value != 0xedu || second <= 0x9fu)};
			if (canonical_second && utf8_continuation(second) && utf8_continuation(third))
			{
				return {static_cast<char32_t>(((first_value & 0x0fu) << 12u) |
											  ((second & 0x3fu) << 6u) | (third & 0x3fu)),
						3u, true};
			}
		}
		else if (0xf0u <= first_value && first_value <= 0xf4u && remaining >= 4u)
		{
			auto const second{format_unicode_code_unit_value(first[1u])};
			auto const third{format_unicode_code_unit_value(first[2u])};
			auto const fourth{format_unicode_code_unit_value(first[3u])};
			bool const canonical_second{
				(first_value != 0xf0u || second >= 0x90u) &&
				(first_value != 0xf4u || second <= 0x8fu)};
			if (canonical_second && utf8_continuation(second) &&
				utf8_continuation(third) && utf8_continuation(fourth))
			{
				return {static_cast<char32_t>(((first_value & 0x07u) << 18u) |
											  ((second & 0x3fu) << 12u) | ((third & 0x3fu) << 6u) |
											  (fourth & 0x3fu)),
						4u, true};
			}
		}
		return {static_cast<char32_t>(first_value), 1u, false};
	}
	else if constexpr (::std::same_as<clean_type, char16_t> || sizeof(clean_type) == 2u)
	{
		if (0xd800u <= first_value && first_value <= 0xdbffu && remaining >= 2u)
		{
			auto const second{format_unicode_code_unit_value(first[1u])};
			if (0xdc00u <= second && second <= 0xdfffu)
			{
				auto const code_point{0x10000u + ((first_value - 0xd800u) << 10u) +
									  (second - 0xdc00u)};
				return {static_cast<char32_t>(code_point), 2u, true};
			}
		}
		if (0xd800u <= first_value && first_value <= 0xdfffu)
		{
			return {static_cast<char32_t>(first_value), 1u, false};
		}
		return {static_cast<char32_t>(first_value), 1u, true};
	}
	else
	{
		return {static_cast<char32_t>(first_value), 1u,
				unicode_scalar_value(first_value)};
	}
}

/**
 * Returns the standard-library-style estimated terminal width of one scalar.
 *
 * This deliberately follows the same stable 1-or-2-column approximation used
 * by fmt and by the C++ formatting wording.  It is not a grapheme-cluster or a
 * locale-sensitive `wcwidth` implementation: those facilities would make the
 * result depend on runtime tables and terminal policy, which is inappropriate
 * for the deterministic format layer.
 */
[[nodiscard]] inline constexpr ::std::size_t estimated_unicode_display_width(
	char32_t code_point) noexcept
{
	auto const cp{static_cast<::std::uint_least32_t>(code_point)};
	return 1u + static_cast<::std::size_t>(
					cp >= 0x1100u &&
					(cp <= 0x115fu || cp == 0x2329u || cp == 0x232au ||
					 (cp >= 0x2e80u && cp <= 0xa4cfu && cp != 0x303fu) ||
					 (cp >= 0xac00u && cp <= 0xd7a3u) ||
					 (cp >= 0xf900u && cp <= 0xfaffu) ||
					 (cp >= 0xfe10u && cp <= 0xfe19u) ||
					 (cp >= 0xfe30u && cp <= 0xfe6fu) ||
					 (cp >= 0xff00u && cp <= 0xff60u) ||
					 (cp >= 0xffe0u && cp <= 0xffe6u) ||
					 (cp >= 0x20000u && cp <= 0x2fffdu) ||
					 (cp >= 0x30000u && cp <= 0x3fffdu) ||
					 (cp >= 0x1f300u && cp <= 0x1f64fu) ||
					 (cp >= 0x1f900u && cp <= 0x1f9ffu)));
}

struct unicode_prefix_measurement
{
	::std::size_t storage_size{};
	::std::size_t display_width{};
};

/** Finds the largest scalar-aligned prefix which fits the display-width limit. */
template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr unicode_prefix_measurement measure_unicode_prefix(
	char_type const *data, ::std::size_t size,
	::std::size_t maximum_display_width = SIZE_MAX) noexcept
{
	if constexpr (!::fast_io::details::is_unicode_execution_charset<char_type>)
	{
		auto const selected{size < maximum_display_width ? size : maximum_display_width};
		return {selected, selected};
	}
	else
	{
		unicode_prefix_measurement result{};
		while (result.storage_size != size)
		{
			auto const scalar{decode_unicode_scalar(
				data + result.storage_size, size - result.storage_size)};
			auto const scalar_width{scalar.valid
										? estimated_unicode_display_width(scalar.code_point)
										: 1u};
			if (scalar_width > maximum_display_width -
								   (result.display_width < maximum_display_width
										? result.display_width
										: maximum_display_width))
			{
				break;
			}
			result.storage_size += scalar.code_units;
			result.display_width += scalar_width;
		}
		return result;
	}
}

struct format_padding_measurement
{
	::std::size_t left{};
	::std::size_t right{};
};

[[nodiscard]] inline constexpr format_padding_measurement measure_format_padding(
	::std::size_t content_width, ::std::size_t minimum_width,
	::fast_io::manipulators::scalar_placement placement) noexcept
{
	if (content_width >= minimum_width)
	{
		return {};
	}
	auto const padding{minimum_width - content_width};
	if (placement == ::fast_io::manipulators::scalar_placement::left)
	{
		return {0u, padding};
	}
	if (placement == ::fast_io::manipulators::scalar_placement::middle)
	{
		// fmt assigns the odd remainder to the right side.
		auto const left{padding / 2u};
		return {left, padding - left};
	}
	return {padding, 0u};
}

template <::fast_io::fmt::format_character char_type>
struct basic_text_field_options
{
	::std::size_t maximum_display_width{SIZE_MAX};
	::std::size_t minimum_width{};
	char_type fill[4u]{::fast_io::char_literal_v<u8' ', char_type>};
	::std::uint_least8_t fill_size{1u};
	::fast_io::manipulators::scalar_placement placement{
		::fast_io::manipulators::scalar_placement::left};
};

/**
 * Builds field policy from the already-compiled format specification.
 *
 * `fill_size` is a storage count.  One fill scalar may occupy multiple code
 * units, but the formatting grammar defines padding as a number of repetitions;
 * the allocator therefore multiplies repetitions by this count.
 */
template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr basic_text_field_options<char_type>
make_text_field_options(::std::size_t maximum_display_width,
						::std::size_t minimum_width,
						::fast_io::manipulators::scalar_placement placement,
						char_type const *fill = nullptr, ::std::size_t fill_size = 0u) noexcept
{
	basic_text_field_options<char_type> result{};
	result.maximum_display_width = maximum_display_width;
	result.minimum_width = minimum_width;
	result.placement = placement;
	if (fill != nullptr && fill_size != 0u)
	{
		result.fill_size = static_cast<::std::uint_least8_t>(fill_size <= 4u ? fill_size : 4u);
		for (::std::size_t index{}; index != result.fill_size; ++index)
		{
			result.fill[index] = fill[index];
		}
	}
	return result;
}

template <::fast_io::fmt::format_character char_type>
inline constexpr char_type *emit_format_fill(char_type *output,
											 basic_text_field_options<char_type> const &options,
											 ::std::size_t repetitions) noexcept
{
	for (::std::size_t repetition{}; repetition != repetitions; ++repetition)
	{
		for (::std::size_t index{}; index != options.fill_size; ++index)
		{
			*output++ = options.fill[index];
		}
	}
	return output;
}

template <::fast_io::fmt::format_character char_type>
struct basic_unicode_text_field
{
	using manip_tag = ::fast_io::manip_tag_t;
	::fast_io::basic_io_scatter_t<char_type> source{};
	basic_text_field_options<char_type> options{};
};

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr basic_unicode_text_field<char_type>
make_unicode_text_field(::fast_io::basic_io_scatter_t<char_type> source,
						basic_text_field_options<char_type> options) noexcept
{
	return {source, options};
}

template <::fast_io::fmt::format_character char_type>
struct unicode_text_field_measurement
{
	unicode_prefix_measurement content{};
	format_padding_measurement padding{};
	::std::size_t storage_size{};
};

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr unicode_text_field_measurement<char_type>
measure_unicode_text_field(basic_unicode_text_field<char_type> const &field) noexcept
{
	auto const content{measure_unicode_prefix(field.source.base, field.source.len,
											  field.options.maximum_display_width)};
	auto const padding{measure_format_padding(content.display_width,
											  field.options.minimum_width, field.options.placement)};
	auto const repetitions{padding.left + padding.right};
	return {content, padding,
			content.storage_size + repetitions * field.options.fill_size};
}

template <::fast_io::fmt::format_character char_type>
inline constexpr char_type *emit_unicode_text_field(char_type *output,
													basic_unicode_text_field<char_type> const &field) noexcept
{
	auto const measurement{measure_unicode_text_field(field)};
	output = emit_format_fill(output, field.options, measurement.padding.left);
	for (::std::size_t index{}; index != measurement.content.storage_size; ++index)
	{
		*output++ = field.source.base[index];
	}
	return emit_format_fill(output, field.options, measurement.padding.right);
}

} // namespace fast_io::fmt::details

namespace fast_io
{

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<output_char_type,
								 ::fast_io::fmt::details::basic_unicode_text_field<source_char_type>>,
	::fast_io::fmt::details::basic_unicode_text_field<source_char_type> field) noexcept
{
	return ::fast_io::fmt::details::measure_unicode_text_field(field).storage_size;
}

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
inline constexpr output_char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<output_char_type,
								 ::fast_io::fmt::details::basic_unicode_text_field<source_char_type>>,
	output_char_type *output,
	::fast_io::fmt::details::basic_unicode_text_field<source_char_type> field) noexcept
{
	return ::fast_io::fmt::details::emit_unicode_text_field(output, field);
}

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
[[nodiscard]] inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<output_char_type,
								 ::fast_io::fmt::details::basic_unicode_text_field<source_char_type>>
		tag,
	::fast_io::fmt::details::basic_unicode_text_field<source_char_type> field) noexcept
{
	return print_reserve_size(tag, field);
}

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
inline constexpr output_char_type *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<output_char_type,
								 ::fast_io::fmt::details::basic_unicode_text_field<source_char_type>>,
	output_char_type *output, ::std::size_t,
	::fast_io::fmt::details::basic_unicode_text_field<source_char_type> field) noexcept
{
	return ::fast_io::fmt::details::emit_unicode_text_field(output, field);
}

/**
 * Exact sizing scans the source before emission, so value-initializing the
 * destination would add a third full-memory pass.  The endpoint-returning,
 * non-throwing define CPO proves that an overwrite-capable concat destination
 * may safely construct its final storage directly.
 */
template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
[[nodiscard]] inline constexpr ::std::true_type
	print_precise_resize_initialization_sensitive(
		::fast_io::io_reserve_type_t<output_char_type,
									 ::fast_io::fmt::details::basic_unicode_text_field<source_char_type>>) noexcept
{
	return {};
}

} // namespace fast_io
