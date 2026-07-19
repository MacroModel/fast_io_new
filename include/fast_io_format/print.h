#pragma once

// The output front door is the only format component that needs hosted stream
// adapters and the default standard-output objects. Lowering itself remains
// reusable by the freestanding concat front doors.
#include "../fast_io.h"
#include "details/brace_rule.h"
#include "details/lower.h"
#include "details/printf_rule.h"

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace fast_io::fmt::details
{

/** Recognizes an output stream without imposing a character domain. */
template <typename output_type>
concept format_output = requires(output_type &output) {
	typename ::std::remove_cvref_t<
		decltype(::fast_io::operations::output_stream_ref(output))>::output_char_type;
};

/** Proves that an output reference consumes the requested code-unit domain. */
template <typename output_type, typename char_type>
concept format_output_for = format_output<output_type> &&
							::std::same_as<typename ::std::remove_cvref_t<decltype(::fast_io::operations::output_stream_ref(
											   ::std::declval<output_type &>()))>::output_char_type,
										   char_type>;

template <typename char_type, typename component_type>
concept format_passive_component_normalizable = requires(
	::std::remove_reference_t<component_type> &component) {
	::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(component));
};

template <typename output_type>
inline consteval bool print_passive_mixed_put_area_output_available() noexcept
{
	using source_output_type = ::std::remove_reference_t<output_type>;
	if constexpr (!format_output<source_output_type>)
	{
		return false;
	}
	else
	{
		using output_reference = ::std::remove_cvref_t<decltype(
			::fast_io::operations::output_stream_ref(
				::std::declval<source_output_type &>()))>;
		using char_type = typename output_reference::output_char_type;
		return ::fast_io::operations::decay::defines::
			   has_obuffer_basic_operations<output_reference> &&
			   ::fast_io::operations::decay::
				   print_passive_mixed_put_area_destination_safe<
					   char_type, output_reference> &&
			   !::fast_io::operations::decay::defines::
				   has_output_or_io_stream_mutex_ref_define<output_reference>;
	}
}

/** Tests the exact normalized pack used by the ordinary print front door without invoking any source CPO twice. */
template <typename output_type, typename... component_types>
inline consteval bool print_lowered_passive_mixed_put_area_available() noexcept
{
#if !defined(__clang__)
	return false;
#else
	using source_output_type = ::std::remove_reference_t<output_type>;
	if constexpr (!print_passive_mixed_put_area_output_available<
		output_type>())
	{
		return false;
	}
	else
	{
		using output_reference = ::std::remove_cvref_t<decltype(
			::fast_io::operations::output_stream_ref(
				::std::declval<source_output_type &>()))>;
		using char_type = typename output_reference::output_char_type;
		if constexpr ((format_passive_component_normalizable<
						char_type, component_types> && ...))
		{
			return ::fast_io::operations::decay::
				print_passive_mixed_put_area_fast_entry_available<
					false, output_reference,
					decltype(::fast_io::io_print_forward<char_type>(
						::fast_io::io_print_alias(
							::std::declval<::std::remove_reference_t<component_types> &>())))...>();
		}
		else
		{
			return false;
		}
	}
#endif
}

template <basic_fixed_string format_literal, typename grammar_type,
		  typename output_type, typename... argument_types,
		  ::std::size_t... operation_index>
inline consteval bool print_program_passive_mixed_put_area_available_impl(
	::std::index_sequence<operation_index...>) noexcept
{
	using argument_pack = indexed_argument_pack<
		::std::index_sequence_for<::std::remove_reference_t<argument_types>...>,
		::std::remove_reference_t<argument_types>...>;
	return print_lowered_passive_mixed_put_area_available<
		output_type,
		decltype(make_format_operation<format_literal, grammar_type, operation_index>(
			::std::declval<argument_pack &>()))...>();
}

template <basic_fixed_string format_literal, typename grammar_type,
		  typename output_type, typename... argument_types>
inline consteval bool print_program_passive_mixed_put_area_available() noexcept
{
#if !defined(__clang__)
	return false;
#else
	if constexpr ((false || ... ||
		::fast_io::fmt::is_static_format_argument_holder_v<
			::std::remove_reference_t<argument_types>>))
	{
		return false;
	}
	else if constexpr (!print_passive_mixed_put_area_output_available<
		output_type>())
	{
		return false;
	}
	else
	{
		using literal_program = compiled_literal_program<format_literal, grammar_type>;
		if constexpr (literal_program::literal_only)
		{
			return false;
		}
		else
		{
			constexpr auto operation_count{
				checked_program<format_literal, grammar_type>.operation_count};
			return print_program_passive_mixed_put_area_available_impl<
				format_literal, grammar_type, output_type, argument_types...>(
				::std::make_index_sequence<operation_count>{});
		}
	}
#endif
}

template <basic_fixed_string format_literal, typename grammar_type,
		  typename... argument_types>
struct print_primary_passive_mixed_put_area_available : ::std::false_type
{};

template <basic_fixed_string format_literal, typename grammar_type,
		  typename first_type, typename... argument_types>
inline consteval bool print_primary_passive_mixed_put_area_available_impl() noexcept
{
	if constexpr (!format_output_for<
		first_type, typename decltype(format_literal)::value_type>)
	{
		return false;
	}
	else
	{
		return print_program_passive_mixed_put_area_available<
			format_literal, grammar_type, first_type, argument_types...>();
	}
}

template <basic_fixed_string format_literal, typename grammar_type,
		  typename first_type, typename... argument_types>
struct print_primary_passive_mixed_put_area_available<
	format_literal, grammar_type, first_type, argument_types...>
	: ::std::bool_constant<print_primary_passive_mixed_put_area_available_impl<
		  format_literal, grammar_type, first_type, argument_types...>()>
{};

/** Named continuation that sends a lowered component pack to the ordinary print front door. */
template <typename output_type>
struct print_lowered_components
{
	output_type &&output;

	template <typename... component_types>
	inline constexpr void operator()(component_types &&...components) const
	{
		if constexpr (sizeof...(component_types) != 0u)
		{
			::fast_io::print(
				::std::forward<output_type>(output),
				::std::forward<component_types>(components)...);
		}
	}
};

/** Narrow continuation selected only after the complete lowered type graph passed the passive-run proof. */
template <typename output_type>
struct print_passive_mixed_lowered_components
{
	output_type &&output;

	template <typename... component_types>
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	inline constexpr void operator()(component_types &&...components) const
	{
		static_assert(print_lowered_passive_mixed_put_area_available<
			output_type, component_types...>());
		decltype(auto) outref{
			::fast_io::operations::output_stream_ref(output)};
		using output_reference = ::std::remove_cvref_t<decltype(outref)>;
		using char_type = typename output_reference::output_char_type;
		::fast_io::operations::decay::print_passive_mixed_put_area_fast_entry<false>(
			outref,
			::fast_io::io_print_forward<char_type>(
				::fast_io::io_print_alias(components))...);
	}
};

/**
 * Emits a structural literal through one explicitly supplied grammar rule.
 *
 * This is the sole grammar-neutral output kernel. The rule participates only
 * in the compile/lower CPO lookup and is required to be an empty type token;
 * it therefore contributes no runtime parameter state. The callback receives
 * the final typed component pack and sends it through ordinary fast_io::print,
 * preserving the core alias, ABI decay, semantic-pack flattening, concat, and
 * syscall strategy selection.
 *
 * Requiring an explicit output here is a deliberate proof boundary. Built-in
 * facades perform the same device-first type classification as
 * fast_io::io::print before entering this kernel; a third-party rule frontend
 * must make that choice explicitly as well.
 */
template <basic_fixed_string format_literal, format_grammar grammar_type,
		  typename output_type, typename... argument_types>
	requires format_output_for<output_type,
							   typename decltype(format_literal)::value_type>
inline constexpr void print_with_rule(
	grammar_type, output_type &&output, argument_types &&...arguments)
{
	using rule_type = ::std::remove_cvref_t<grammar_type>;
	::fast_io::fmt::details::lower_format_program<format_literal, rule_type>(
		::fast_io::fmt::details::print_lowered_components<output_type>{
			::std::forward<output_type>(output)},
		arguments...);
}

template <basic_fixed_string format_literal, typename grammar_type,
		  typename callback_type, typename argument_pack,
		  ::std::size_t... operation_index>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void print_passive_mixed_program_impl(
	callback_type &&callback, argument_pack &arguments,
	::std::index_sequence<operation_index...>)
{
	::std::forward<callback_type>(callback)(
		make_format_operation<format_literal, grammar_type, operation_index>(arguments)...);
}

/** Lowers an already-proved passive program without placing its component pack across an ABI boundary. */
template <basic_fixed_string format_literal, format_grammar grammar_type,
		  typename output_type, typename... argument_types>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void print_passive_mixed_with_rule(
	grammar_type, output_type &&output, argument_types &&...arguments)
{
	using rule_type = ::std::remove_cvref_t<grammar_type>;
	auto indexed_arguments{make_indexed_argument_pack(arguments...)};
	constexpr auto operation_count{
		checked_program<format_literal, rule_type>.operation_count};
	print_passive_mixed_program_impl<format_literal, rule_type>(
		print_passive_mixed_lowered_components<output_type>{
			::std::forward<output_type>(output)},
		indexed_arguments, ::std::make_index_sequence<operation_count>{});
}

/** Selects the hosted default output for one explicit code-unit domain. */
template <typename char_type>
[[nodiscard]] inline constexpr auto default_format_output() noexcept
{
#if __has_include(<stdio.h>)
	// Match io::print's C-stdout boundary where fast_io exposes a portable C
	// observer. char16_t/char32_t have no such portable C stream and therefore
	// use the native descriptor observer below.
	if constexpr (::std::same_as<char_type, char>)
	{
		return ::fast_io::c_stdout();
	}
	else if constexpr (::std::same_as<char_type, wchar_t>)
	{
		return ::fast_io::wc_stdout();
	}
	else if constexpr (::std::same_as<char_type, char8_t>)
	{
		return ::fast_io::u8c_stdout();
	}
	else
#endif
		return ::fast_io::native_stdout<char_type>();
}

/**
 * Implements one built-in facade with io::print-compatible device selection.
 *
 * A stream object remains a stream even when its character domain is wrong;
 * the dedicated assertion below must diagnose that mismatch rather than
 * silently reclassifying the object as a value for default stdout.
 */
template <typename expected_char_type, basic_fixed_string format_literal,
		  format_grammar grammar_type, typename first_type,
		  typename... argument_types>
inline constexpr void print_builtin_with_rule(
	grammar_type grammar, first_type &&first,
	argument_types &&...arguments)
{
	using literal_char_type = typename decltype(format_literal)::value_type;
	static_assert(::std::same_as<literal_char_type, expected_char_type>,
				  "fast_io format: the format literal character type does not match the selected print function");
	if constexpr (::fast_io::fmt::details::format_output<first_type>)
	{
		static_assert(
			::fast_io::fmt::details::format_output_for<first_type,
													   expected_char_type>,
			"fast_io format: output stream and format literal use different character types");
		if constexpr (::fast_io::fmt::details::format_output_for<
						  first_type, expected_char_type>)
		{
			::fast_io::fmt::details::print_with_rule<format_literal>(
				grammar, ::std::forward<first_type>(first),
				::std::forward<argument_types>(arguments)...);
		}
	}
	else
	{
		::fast_io::fmt::details::print_with_rule<format_literal>(
			grammar,
			::fast_io::fmt::details::default_format_output<expected_char_type>(),
			::std::forward<first_type>(first),
			::std::forward<argument_types>(arguments)...);
	}
}

template <typename expected_char_type, basic_fixed_string format_literal,
		  format_grammar grammar_type>
inline constexpr void print_builtin_with_rule(grammar_type grammar)
{
	using literal_char_type = typename decltype(format_literal)::value_type;
	static_assert(::std::same_as<literal_char_type, expected_char_type>,
				  "fast_io format: the format literal character type does not match the selected print function");
	::fast_io::fmt::details::print_with_rule<format_literal>(
		grammar,
		::fast_io::fmt::details::default_format_output<expected_char_type>());
}

/**
 * Implements the unprefixed io::print-compatible facade.
 *
 * An explicit first output lets the literal itself select any supported
 * character domain. Without an output, the unprefixed spelling retains
 * io::print's narrow C-stdout meaning; the prefixed family below supplies
 * default outputs for the other four domains.
 */
template <basic_fixed_string format_literal, format_grammar grammar_type,
		  typename first_type, typename... argument_types>
inline constexpr void print_primary_with_rule(
	grammar_type grammar, first_type &&first,
	argument_types &&...arguments)
{
	using char_type = typename decltype(format_literal)::value_type;
	if constexpr (::fast_io::fmt::details::format_output<first_type>)
	{
		static_assert(
			::fast_io::fmt::details::format_output_for<first_type, char_type>,
			"fast_io format: output stream and format literal use different character types");
		if constexpr (::fast_io::fmt::details::format_output_for<
						  first_type, char_type>)
		{
			::fast_io::fmt::details::print_with_rule<format_literal>(
				grammar, ::std::forward<first_type>(first),
				::std::forward<argument_types>(arguments)...);
		}
	}
	else
	{
		static_assert(::std::same_as<char_type, char>,
					  "fast_io format: an unprefixed non-char format requires an explicit matching output stream");
		if constexpr (::std::same_as<char_type, char>)
		{
			::fast_io::fmt::details::print_with_rule<format_literal>(
				grammar,
				::fast_io::fmt::details::default_format_output<char>(),
				::std::forward<first_type>(first),
				::std::forward<argument_types>(arguments)...);
		}
	}
}

template <basic_fixed_string format_literal, format_grammar grammar_type>
inline constexpr void print_primary_with_rule(grammar_type grammar)
{
	using char_type = typename decltype(format_literal)::value_type;
	static_assert(::std::same_as<char_type, char>,
				  "fast_io format: an unprefixed non-char format requires an explicit matching output stream");
	if constexpr (::std::same_as<char_type, char>)
	{
		::fast_io::fmt::details::print_with_rule<format_literal>(
			grammar,
			::fast_io::fmt::details::default_format_output<char>());
	}
}

} // namespace fast_io::fmt::details

namespace fast_io::fmt
{

// Brace grammar. Function prefixes select only the code-unit domain; device
// selection remains the same output-first rule for every member of the family.
template <basic_fixed_string format_literal, typename... argument_types>
	requires (::fast_io::fmt::details::print_primary_passive_mixed_put_area_available<
		format_literal, brace_fmt_t, argument_types...>::value)
#if __has_cpp_attribute(__gnu__::__flatten__)
[[__gnu__::__flatten__]]
#endif
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void print(argument_types &&...arguments)
{
	::fast_io::fmt::details::print_passive_mixed_with_rule<format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
	requires (!::fast_io::fmt::details::print_primary_passive_mixed_put_area_available<
		format_literal, brace_fmt_t, argument_types...>::value)
inline constexpr void print(argument_types &&...arguments)
{
	::fast_io::fmt::details::print_primary_with_rule<format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
inline constexpr void wprint(argument_types &&...arguments)
{
	::fast_io::fmt::details::print_builtin_with_rule<wchar_t, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
inline constexpr void u8print(argument_types &&...arguments)
{
	::fast_io::fmt::details::print_builtin_with_rule<char8_t, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
inline constexpr void u16print(argument_types &&...arguments)
{
	::fast_io::fmt::details::print_builtin_with_rule<char16_t, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
inline constexpr void u32print(argument_types &&...arguments)
{
	::fast_io::fmt::details::print_builtin_with_rule<char32_t, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

// Percent grammar. The `f` suffix authorizes percent conversions; it is not a
// runtime-formatting mode.
template <basic_fixed_string format_literal, typename... argument_types>
inline constexpr void printf(argument_types &&...arguments)
{
	::fast_io::fmt::details::print_primary_with_rule<format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
inline constexpr void wprintf(argument_types &&...arguments)
{
	::fast_io::fmt::details::print_builtin_with_rule<wchar_t, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
inline constexpr void u8printf(argument_types &&...arguments)
{
	::fast_io::fmt::details::print_builtin_with_rule<char8_t, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
inline constexpr void u16printf(argument_types &&...arguments)
{
	::fast_io::fmt::details::print_builtin_with_rule<char16_t, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
inline constexpr void u32printf(argument_types &&...arguments)
{
	::fast_io::fmt::details::print_builtin_with_rule<char32_t, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

// Ordinary array format arguments cannot expose their contents as a template
// specialization. Every runtime-array spelling is therefore deleted instead
// of falling back to a runtime parser.
template <typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void print(char_type const (&)[extent], argument_types &&...) = delete;
template <typename output_type, typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void print(output_type &&, char_type const (&)[extent],
				  argument_types &&...) = delete;

template <typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void wprint(char_type const (&)[extent], argument_types &&...) = delete;
template <typename output_type, typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void wprint(output_type &&, char_type const (&)[extent],
				   argument_types &&...) = delete;

template <typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u8print(char_type const (&)[extent], argument_types &&...) = delete;
template <typename output_type, typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u8print(output_type &&, char_type const (&)[extent],
					argument_types &&...) = delete;

template <typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u16print(char_type const (&)[extent], argument_types &&...) = delete;
template <typename output_type, typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u16print(output_type &&, char_type const (&)[extent],
					 argument_types &&...) = delete;

template <typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u32print(char_type const (&)[extent], argument_types &&...) = delete;
template <typename output_type, typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u32print(output_type &&, char_type const (&)[extent],
					 argument_types &&...) = delete;

template <typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void printf(char_type const (&)[extent], argument_types &&...) = delete;
template <typename output_type, typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void printf(output_type &&, char_type const (&)[extent],
				   argument_types &&...) = delete;

template <typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void wprintf(char_type const (&)[extent], argument_types &&...) = delete;
template <typename output_type, typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void wprintf(output_type &&, char_type const (&)[extent],
					argument_types &&...) = delete;

template <typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u8printf(char_type const (&)[extent], argument_types &&...) = delete;
template <typename output_type, typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u8printf(output_type &&, char_type const (&)[extent],
					 argument_types &&...) = delete;

template <typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u16printf(char_type const (&)[extent], argument_types &&...) = delete;
template <typename output_type, typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u16printf(output_type &&, char_type const (&)[extent],
					  argument_types &&...) = delete;

template <typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u32printf(char_type const (&)[extent], argument_types &&...) = delete;
template <typename output_type, typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u32printf(output_type &&, char_type const (&)[extent],
					  argument_types &&...) = delete;

} // namespace fast_io::fmt
