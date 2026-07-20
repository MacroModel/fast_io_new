#include <fast_io_format.h>

#include <string>

namespace custom_format_adl_reference_test
{

struct state
{
	char marker{};

	constexpr bool operator==(state const &) const noexcept = default;
};

struct by_value
{
	char value{};
};

struct by_lvalue_reference
{
	char value{};
};

struct by_const_reference
{
	char value{};
};

template <typename value_type, typename context_type>
	requires(::std::same_as<value_type, by_value> ||
			 ::std::same_as<value_type, by_lvalue_reference> ||
			 ::std::same_as<value_type, by_const_reference>)
[[nodiscard]] consteval state format_parse_define(
	::fast_io::io_type_t<value_type>, context_type) noexcept
{
	return {'!'};
}

template <typename char_type, auto parsed_state>
[[nodiscard]] inline constexpr auto format_alias_define(
	::fast_io::fmt::basic_custom_format_state_t<char_type, parsed_state>,
	by_value value) noexcept
{
	static_assert(parsed_state.marker == '!');
	return ::fast_io::manipulators::chvw(static_cast<char_type>(value.value));
}

template <typename char_type, auto parsed_state>
[[nodiscard]] inline constexpr auto format_alias_define(
	::fast_io::fmt::basic_custom_format_state_t<char_type, parsed_state>,
	by_lvalue_reference &value) noexcept
{
	static_assert(parsed_state.marker == '!');
	return ::fast_io::manipulators::chvw(static_cast<char_type>(value.value));
}

template <typename char_type, auto parsed_state>
[[nodiscard]] inline constexpr auto format_alias_define(
	::fast_io::fmt::basic_custom_format_state_t<char_type, parsed_state>,
	by_const_reference const &value) noexcept
{
	static_assert(parsed_state.marker == '!');
	return ::fast_io::manipulators::chvw(static_cast<char_type>(value.value));
}

} // namespace custom_format_adl_reference_test

int main()
{
	using namespace custom_format_adl_reference_test;
	by_lvalue_reference lvalue{'b'};
	by_const_reference const const_lvalue{'c'};
	return ::fast_io::fmt::concat_std<"{}{}{}">(
			   by_value{'a'}, lvalue, const_lvalue) == "abc"
		? 0
		: 1;
}
