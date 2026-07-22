#pragma once

#include <cstddef>

#include "../../fast_io_core.h"

namespace fast_io::json
{

enum class json_errc
{
	none = 0,
	// DOM accessors
	is_undefined = 1,
	not_null,
	not_boolean,
	not_number,
	not_integer,
	not_uinteger,
	not_string,
	not_array,
	not_object,
	nonarray_indexing,
	nonobject_indexing,
	index_out_of_range,
	key_not_found,

	// DOM modifiers
	is_empty,
	not_undefined_or_null,
	not_undefined_or_boolean,
	not_undefined_or_number,
	not_undefined_or_integer,
	not_undefined_or_uinteger,
	not_undefined_or_string,
	not_undefined_or_array,
	not_undefined_or_object,

	// Parser
	syntax_error,
	number_overflow,
	integer_overflow,
	uinteger_overflow,
	unexpected_end,
	unexpected_token,
	invalid_literal,
	invalid_number,
	invalid_escape,
	unescaped_control_character,
	invalid_unicode_escape,
	invalid_unicode,
	invalid_utf8,
	trailing_data,
	depth_exceeded,
	duplicate_key,
	expected_colon,
	expected_comma_or_end,

	// Serializer
	number_nan,
	number_inf,

	// Ownership safety
	cyclic_reference,
	null_pointer,

	// Type-erased conversion fallback; never emitted by the JSON parser/writer.
	unknown
};

namespace details
{

inline constexpr ::fast_io::basic_io_scatter_t<char> json_error_messages[]{
	::fast_io::details::tsc("No JSON error."),
	::fast_io::details::tsc("JSON error: value is undefined."),
	::fast_io::details::tsc("JSON error: value is not null."),
	::fast_io::details::tsc("JSON error: value is not a boolean."),
	::fast_io::details::tsc("JSON error: value is not a number."),
	::fast_io::details::tsc("JSON error: value is not a signed integer."),
	::fast_io::details::tsc("JSON error: value is not an unsigned integer."),
	::fast_io::details::tsc("JSON error: value is not a string."),
	::fast_io::details::tsc("JSON error: value is not an array."),
	::fast_io::details::tsc("JSON error: value is not an object."),
	::fast_io::details::tsc("JSON error: array indexing was applied to a non-array value."),
	::fast_io::details::tsc("JSON error: object indexing was applied to a non-object value."),
	::fast_io::details::tsc("JSON error: array index is out of range."),
	::fast_io::details::tsc("JSON error: object key was not found."),
	::fast_io::details::tsc("JSON error: slice does not refer to a node."),
	::fast_io::details::tsc("JSON error: value is neither undefined nor null."),
	::fast_io::details::tsc("JSON error: value is neither undefined nor boolean."),
	::fast_io::details::tsc("JSON error: value is neither undefined nor a number."),
	::fast_io::details::tsc("JSON error: value is neither undefined nor a signed integer."),
	::fast_io::details::tsc("JSON error: value is neither undefined nor an unsigned integer."),
	::fast_io::details::tsc("JSON error: value is neither undefined nor a string."),
	::fast_io::details::tsc("JSON error: value is neither undefined nor an array."),
	::fast_io::details::tsc("JSON error: value is neither undefined nor an object."),
	::fast_io::details::tsc("JSON parse error: invalid syntax."),
	::fast_io::details::tsc("JSON parse error: floating-point value is out of range."),
	::fast_io::details::tsc("JSON parse error: signed integer is out of range."),
	::fast_io::details::tsc("JSON parse error: unsigned integer is out of range."),
	::fast_io::details::tsc("JSON parse error: unexpected end of input."),
	::fast_io::details::tsc("JSON parse error: unexpected token."),
	::fast_io::details::tsc("JSON parse error: invalid literal."),
	::fast_io::details::tsc("JSON parse error: invalid number."),
	::fast_io::details::tsc("JSON parse error: invalid string escape."),
	::fast_io::details::tsc("JSON parse error: unescaped control character in a string."),
	::fast_io::details::tsc("JSON parse error: invalid Unicode escape."),
	::fast_io::details::tsc("JSON error: invalid Unicode scalar value."),
	::fast_io::details::tsc("JSON parse error: invalid UTF-8 sequence."),
	::fast_io::details::tsc("JSON parse error: trailing data after the root value."),
	::fast_io::details::tsc("JSON error: maximum nesting depth exceeded."),
	::fast_io::details::tsc("JSON parse error: duplicate object key."),
	::fast_io::details::tsc("JSON parse error: expected ':' after an object key."),
	::fast_io::details::tsc("JSON parse error: expected ',' or the end of a container."),
	::fast_io::details::tsc("JSON serialization error: NaN is not a JSON number."),
	::fast_io::details::tsc("JSON serialization error: infinity is not a JSON number."),
	::fast_io::details::tsc("JSON DOM error: moving an owning value into its own descendant would create a cycle."),
	::fast_io::details::tsc("JSON DOM error: a null character pointer is not a JSON string or object key."),
	::fast_io::details::tsc("JSON error: unknown error code.")};

static_assert(sizeof(json_error_messages) / sizeof(*json_error_messages) ==
			  static_cast<::std::size_t>(json_errc::unknown) + 1u);

} // namespace details

[[nodiscard]] inline constexpr ::fast_io::basic_io_scatter_t<char>
json_error_message_scatter(json_errc code) noexcept
{
	auto const index{static_cast<::std::size_t>(code)};
	if (index < sizeof(details::json_error_messages) /
					sizeof(*details::json_error_messages))
	{
		return details::json_error_messages[index];
	}
	return details::json_error_messages[
		static_cast<::std::size_t>(json_errc::unknown)];
}

[[nodiscard]] inline constexpr char const *json_error_message(
	json_errc code) noexcept
{
	return json_error_message_scatter(code).base;
}

inline constexpr ::std::size_t domain_define(
	::fast_io::error_type_t<json_errc>) noexcept
{
	if constexpr (sizeof(::std::size_t) <= sizeof(::std::uint_least16_t))
	{
		return 43802u;
	}
	else if constexpr (sizeof(::std::size_t) <= sizeof(::std::uint_least32_t))
	{
		return 2528050061u;
	}
	else
	{
		return 14242334204606106761ULL;
	}
}

inline constexpr ::std::size_t json_domain_value{
	domain_define(::fast_io::error_type<json_errc>)};

inline constexpr bool equivalent_define(
	::fast_io::error_type_t<json_errc>, ::fast_io::error error,
	json_errc code) noexcept
{
	return error.domain == json_domain_value &&
		   error.code == static_cast<::std::size_t>(code);
}

inline constexpr json_errc to_code_define(
	::fast_io::error_type_t<json_errc>, ::fast_io::error error) noexcept
{
	if (error.domain != json_domain_value ||
		error.code > static_cast<::std::size_t>(json_errc::null_pointer))
	{
		return json_errc::unknown;
	}
	return static_cast<json_errc>(error.code);
}

[[noreturn]] inline void throw_json_error([[maybe_unused]] json_errc code)
{
#ifdef __cpp_exceptions
#if defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0)
	::fast_io::fast_terminate();
#else
	throw ::fast_io::error{json_domain_value,
		static_cast<::std::size_t>(code)};
#endif
#else
	::fast_io::fast_terminate();
#endif
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr auto status_io_print_forward(
	::fast_io::io_alias_type_t<char_type>, json_errc code) noexcept
{
	auto const message{json_error_message_scatter(code)};
	if constexpr (::std::same_as<char_type, char>)
	{
		return message;
	}
	else
	{
		return ::fast_io::manipulators::code_cvt_t<
			::fast_io::encoding_scheme::execution_charset,
			::fast_io::encoding_scheme::execution_charset, char>{message};
	}
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr ::std::true_type
print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char_type, json_errc>) noexcept
{
	return {};
}

} // namespace fast_io::json
