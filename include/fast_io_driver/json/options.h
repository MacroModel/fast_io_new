#pragma once

#include <cstddef>
#include <type_traits>

namespace fast_io::json
{

enum class json_duplicate_key_policy : unsigned char
{
	reject,
	keep_first,
	keep_last
};

enum class json_integer_preference : unsigned char
{
	/**
	 * Negative tokens use the signed category.  A non-negative token uses the
	 * signed category when it is losslessly representable there, and otherwise
	 * uses the unsigned category.  It falls back to the configured floating
	 * category only when the corresponding integral category is disabled.
	 */
	preserve,
	prefer_signed,
	prefer_unsigned,
	prefer_floating
};

enum class json_escape_policy : unsigned char
{
	/** Escape only quotation mark, reverse solidus, and control characters. */
	minimal,
	/** Additionally emit every non-ASCII scalar as a Unicode escape. */
	ascii,
	/** Also escape JavaScript-sensitive U+2028 and U+2029. */
	javascript_safe
};

enum class json_undefined_policy : unsigned char
{
	error,
	as_null,
	/** Debug-only extension that emits the non-JSON token "undefined". */
	as_literal
};

struct json_parse_options
{
	::std::size_t max_depth{1024u};
	json_duplicate_key_policy duplicate_keys{json_duplicate_key_policy::reject};
	json_integer_preference integer_preference{json_integer_preference::preserve};
};

struct json_serialize_options
{
	::std::size_t max_depth{1024u};
	bool pretty{};
	::std::size_t indent_width{2u};
	char indent_char{' '};
	json_escape_policy escape{json_escape_policy::minimal};
	bool escape_solidus{};
	json_undefined_policy undefined{json_undefined_policy::error};
};

/*
 * Compatibility bitmask retained from the DOM prototype.  New code should
 * prefer json_parse_options/json_serialize_options because several policies
 * are mutually exclusive and cannot be represented safely by independent
 * bits.
 */
enum class json_option : unsigned int
{
	allow_null = 0x0u,
	allow_comment = 0x1u,
	allow_overflow_number = 0x2u,
	allow_underflow_number = 0x4u,
	allow_overflow_integer = 0x8u,
	allow_underflow_integer = 0x10u,
	allow_overflow_uinteger = 0x20u,
	treat_overflow_integer_as_number = 0x40u,
	treat_null_as_defaulted = 0x80u,
	treat_undefined_as_defaulted = 0x100u,
	treat_undefined_as_null = 0x200u,
	treat_undefined_as_undefined = 0x400u,
	treat_undefined_as_literal = 0x800u
};

[[nodiscard]] inline constexpr json_option operator|(json_option left, json_option right) noexcept
{
	using underlying_type = ::std::underlying_type_t<json_option>;
	return static_cast<json_option>(static_cast<underlying_type>(left) | static_cast<underlying_type>(right));
}

[[nodiscard]] inline constexpr json_option operator&(json_option left, json_option right) noexcept
{
	using underlying_type = ::std::underlying_type_t<json_option>;
	return static_cast<json_option>(static_cast<underlying_type>(left) & static_cast<underlying_type>(right));
}

inline constexpr json_option &operator|=(json_option &left, json_option right) noexcept
{
	return left = left | right;
}

inline constexpr json_option &operator&=(json_option &left, json_option right) noexcept
{
	return left = left & right;
}

[[nodiscard]] inline constexpr bool has_option(json_option options, json_option option) noexcept
{
	using underlying_type = ::std::underlying_type_t<json_option>;
	return (static_cast<underlying_type>(options) & static_cast<underlying_type>(option)) != 0u;
}

} // namespace fast_io::json
