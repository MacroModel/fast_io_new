#pragma once

#include "semantic.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace fast_io::fmt::details
{

/**
 * A width node whose fill is one encoded scalar rather than one code unit.
 *
 * The core width node intentionally stores a single `Char`.  That is the ideal
 * representation for the overwhelmingly common ASCII fill, but a brace fill is
 * one Unicode scalar and may occupy four UTF-8 or two UTF-16 units.  Keeping this
 * uncommon representation in the format layer avoids enlarging every core width
 * node.  `width` counts repetitions; `fill_size` counts destination code units.
 * Their product is used only for storage and never fed back into the field-width
 * decision.
 */
template <::fast_io::fmt::format_character char_type, typename value_type>
struct basic_pattern_width
{
	using manip_tag = ::fast_io::manip_tag_t;

	value_type value;
	::std::size_t width{};
	::fast_io::manipulators::scalar_placement placement{};
	char_type fill[4u]{};
	::std::uint_least8_t fill_size{};
};

template <typename child_type, ::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr ::std::size_t pattern_width_child_reserve_size(
	child_type &child) noexcept
{
	using clean_type = ::std::remove_cvref_t<child_type>;
	if constexpr (::fast_io::reserve_printable<char_type, clean_type>)
	{
		return print_reserve_size(::fast_io::io_reserve_type<char_type, clean_type>);
	}
	else if constexpr (::fast_io::dynamic_reserve_printable<char_type, clean_type>)
	{
		return print_reserve_size(
			::fast_io::io_reserve_type<char_type, clean_type>, child);
	}
	else
	{
		static_assert(::fast_io::scatter_printable<char_type, clean_type>);
		return print_scatter_define(
			::fast_io::io_reserve_type<char_type, clean_type>, child).len;
	}
}

template <typename child_type, ::fast_io::fmt::format_character char_type>
inline constexpr char_type *emit_pattern_width_child(
	char_type *output, child_type &child) noexcept
{
	using clean_type = ::std::remove_cvref_t<child_type>;
	if constexpr (::fast_io::scatter_printable<char_type, clean_type>)
	{
		auto const scatter{print_scatter_define(
			::fast_io::io_reserve_type<char_type, clean_type>, child)};
		for (::std::size_t index{}; index != scatter.len; ++index)
		{
			*output++ = scatter.base[index];
		}
		return output;
	}
	else
	{
		return print_reserve_define(
			::fast_io::io_reserve_type<char_type, clean_type>, output, child);
	}
}

template <typename child_type, ::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr ::std::size_t pattern_width_internal_shift(
	child_type &child) noexcept
{
	using clean_type = ::std::remove_cvref_t<child_type>;
	if constexpr (::fast_io::printable_internal_shift<char_type, clean_type>)
	{
		return print_define_internal_shift(
			::fast_io::io_reserve_type<char_type, clean_type>, child);
	}
	else
	{
		return 0u;
	}
}

template <::fast_io::fmt::format_character char_type, typename value_type>
inline constexpr char_type *emit_pattern_fill(
	char_type *output, basic_pattern_width<char_type, value_type> const &field,
	::std::size_t repetitions) noexcept
{
	for (::std::size_t repetition{}; repetition != repetitions; ++repetition)
	{
		for (::std::size_t unit{}; unit != field.fill_size; ++unit)
		{
			*output++ = field.fill[unit];
		}
	}
	return output;
}

template <::fast_io::fmt::format_character char_type, typename value_type,
	typename child_type>
inline constexpr char_type *emit_pattern_width_impl(
	char_type *output, basic_pattern_width<char_type, value_type> const &field,
	child_type &child) noexcept
{
	auto const child_end{emit_pattern_width_child<child_type, char_type>(output, child)};
	auto const child_size{static_cast<::std::size_t>(child_end - output)};
	if (field.width <= child_size)
	{
		return child_end;
	}

	auto const repetitions{field.width - child_size};
	::std::size_t left_repetitions{};
	::std::size_t right_repetitions{};
	auto placement{field.placement};
	if (placement == ::fast_io::manipulators::scalar_placement::left)
	{
		right_repetitions = repetitions;
	}
	else if (placement == ::fast_io::manipulators::scalar_placement::middle)
	{
		left_repetitions = repetitions / 2u;
		right_repetitions = repetitions - left_repetitions;
	}
	else if (placement == ::fast_io::manipulators::scalar_placement::internal)
	{
		auto const shift{pattern_width_internal_shift<child_type, char_type>(child)};
		if (shift <= child_size && shift != 0u)
		{
			auto const inserted_units{repetitions * field.fill_size};
			for (auto source{child_end}; source != output + shift;)
			{
				--source;
				source[inserted_units] = *source;
			}
			(void)emit_pattern_fill(output + shift, field, repetitions);
			return child_end + inserted_units;
		}
		// A non-numeric custom child cannot prove a sign/prefix boundary.  Core
		// width uses the same safe fallback: internal becomes right alignment.
		left_repetitions = repetitions;
	}
	else
	{
		left_repetitions = repetitions;
	}

	auto const left_units{left_repetitions * field.fill_size};
	if (left_units != 0u)
	{
		for (auto source{child_end}; source != output;)
		{
			--source;
			source[left_units] = *source;
		}
	}
	(void)emit_pattern_fill(output, field, left_repetitions);
	auto end{child_end + left_units};
	return emit_pattern_fill(end, field, right_repetitions);
}

template <::fast_io::fmt::format_character char_type, typename T>
	requires ::fast_io::details::width_storable<T>
[[nodiscard]] inline constexpr auto make_pattern_width(
	T &&value, ::std::size_t width,
	::fast_io::manipulators::scalar_placement placement,
	char_type const *fill, ::std::size_t fill_size)
	noexcept(::fast_io::details::width_storage_nothrow_constructible<T>)
{
	using storage_type = ::fast_io::details::width_storage_type<T>;
	basic_pattern_width<char_type, storage_type> result{
		::fast_io::details::width_store(::std::forward<T>(value)), width,
		placement, {}, static_cast<::std::uint_least8_t>(fill_size)};
	for (::std::size_t index{}; index != fill_size; ++index)
	{
		result.fill[index] = fill[index];
	}
	return result;
}

} // namespace fast_io::fmt::details

namespace fast_io
{

template <::fast_io::fmt::format_character char_type, typename value_type>
	requires(
		::fast_io::reserve_printable<char_type, ::std::remove_cvref_t<value_type>> ||
		::fast_io::dynamic_reserve_printable<char_type, ::std::remove_cvref_t<value_type>> ||
		::fast_io::scatter_printable<char_type, ::std::remove_cvref_t<value_type>>)
[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::fmt::details::basic_pattern_width<char_type, value_type>>,
	::fast_io::fmt::details::basic_pattern_width<char_type, value_type> field) noexcept
{
	if constexpr (::std::is_reference_v<value_type>)
	{
		::fast_io::parameter<value_type> child{field.value};
		auto const child_size{
			::fast_io::fmt::details::pattern_width_child_reserve_size<
				decltype(child), char_type>(child)};
		return ::fast_io::details::intrinsics::add_or_overflow_die(
			child_size,
			::fast_io::details::intrinsics::mul_or_overflow_die(
				field.width, field.fill_size));
	}
	else
	{
		auto const child_size{
			::fast_io::fmt::details::pattern_width_child_reserve_size<
				value_type, char_type>(field.value)};
		return ::fast_io::details::intrinsics::add_or_overflow_die(
			child_size,
			::fast_io::details::intrinsics::mul_or_overflow_die(
				field.width, field.fill_size));
	}
}

template <::fast_io::fmt::format_character char_type, typename value_type>
	requires(
		::fast_io::reserve_printable<char_type, ::std::remove_cvref_t<value_type>> ||
		::fast_io::dynamic_reserve_printable<char_type, ::std::remove_cvref_t<value_type>> ||
		::fast_io::scatter_printable<char_type, ::std::remove_cvref_t<value_type>>)
inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::fmt::details::basic_pattern_width<char_type, value_type>>,
	char_type *output,
	::fast_io::fmt::details::basic_pattern_width<char_type, value_type> field) noexcept
{
	if constexpr (::std::is_reference_v<value_type>)
	{
		::fast_io::parameter<value_type> child{field.value};
		return ::fast_io::fmt::details::emit_pattern_width_impl(
			output, field, child);
	}
	else
	{
		return ::fast_io::fmt::details::emit_pattern_width_impl(
			output, field, field.value);
	}
}

} // namespace fast_io
