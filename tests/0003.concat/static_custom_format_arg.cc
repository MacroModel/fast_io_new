#include <fast_io_format.h>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace static_custom_probe
{

template <char8_t code, ::fast_io::fmt::format_character char_type>
inline constexpr char_type domain_character{
	::fast_io::arithmetic_char_literal_v<code, char_type>};

struct format_as_value
{
	unsigned value{};

	constexpr bool operator==(format_as_value const &) const noexcept = default;
};

inline unsigned format_as_runtime_calls{};

[[nodiscard]] inline constexpr unsigned format_as(
	format_as_value value) noexcept
{
	if (!::std::is_constant_evaluated())
	{
		++format_as_runtime_calls;
	}
	return value.value;
}

struct automatic_format_as_value
{
	unsigned value{};

	constexpr bool operator==(
		automatic_format_as_value const &) const noexcept = default;
};

inline unsigned automatic_format_as_runtime_calls{};

[[nodiscard]] inline constexpr unsigned format_as(
	automatic_format_as_value value) noexcept
{
	if (!::std::is_constant_evaluated())
	{
		++automatic_format_as_runtime_calls;
	}
	return value.value;
}

struct automatic_text_value
{
	constexpr bool operator==(
		automatic_text_value const &) const noexcept = default;
};

[[nodiscard]] inline constexpr ::std::string_view format_as(
	automatic_text_value) noexcept
{
	return "view";
}

struct runtime_backed_text_value
{
	constexpr bool operator==(
		runtime_backed_text_value const &) const noexcept = default;
};

inline char runtime_backed_text[]{'r', 'u', 'n'};
inline unsigned runtime_backed_text_calls{};

[[nodiscard]] inline constexpr ::std::string_view format_as(
	runtime_backed_text_value) noexcept
{
	if (!::std::is_constant_evaluated())
	{
		++runtime_backed_text_calls;
	}
	return {runtime_backed_text, 3u};
}

struct empty_value
{
	constexpr bool operator==(empty_value const &) const noexcept = default;
};

[[nodiscard]] inline constexpr auto format_as(empty_value) noexcept
{
	return ::fast_io::io_null;
}

struct custom_state
{
	bool alternate{};

	constexpr bool operator==(custom_state const &) const noexcept = default;
};

struct custom_value
{
	unsigned value{};

	constexpr bool operator==(custom_value const &) const noexcept = default;
};

template <typename context_type>
[[nodiscard]] constexpr custom_state format_parse_define(
	::fast_io::io_type_t<custom_value>, context_type context) noexcept
{
	using char_type = typename context_type::char_type;
	bool alternate{};
	for (::std::size_t index{}; index != context.size(); ++index)
	{
		if (context[index] == domain_character<u8'q', char_type>)
		{
			alternate = true;
		}
	}
	return {alternate};
}

template <::fast_io::fmt::format_character char_type, bool alternate>
struct custom_node
{
	custom_value const *pointer{};
};

inline unsigned custom_alias_runtime_calls{};

template <::fast_io::fmt::format_character char_type, auto state>
[[nodiscard]] inline constexpr auto format_alias_define(
	::fast_io::fmt::basic_custom_format_state_t<char_type, state>,
	custom_value const &value) noexcept
{
	if (!::std::is_constant_evaluated())
	{
		++custom_alias_runtime_calls;
	}
	return custom_node<char_type, state.alternate>{&value};
}

template <typename output_char_type, typename node_char_type, bool alternate>
[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<output_char_type,
								 custom_node<node_char_type, alternate>>) noexcept
{
	static_assert(::std::same_as<output_char_type, node_char_type>);
	return 3u;
}

template <typename output_char_type, typename node_char_type, bool alternate>
[[nodiscard]] inline constexpr output_char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<output_char_type,
								 custom_node<node_char_type, alternate>>,
	output_char_type *output,
	custom_node<node_char_type, alternate> node) noexcept
{
	static_assert(::std::same_as<output_char_type, node_char_type>);
	*output++ = alternate ? domain_character<u8'q', output_char_type>
						  : domain_character<u8'x', output_char_type>;
	*output++ = static_cast<output_char_type>(
		domain_character<u8'0', output_char_type> + node.pointer->value / 10u);
	*output++ = static_cast<output_char_type>(
		domain_character<u8'0', output_char_type> + node.pointer->value % 10u);
	return output;
}

template <auto specification, ::std::size_t depth,
		  ::fast_io::fmt::format_character char_type>
[[nodiscard]] constexpr ::std::size_t format_static_reserve_size(
	::fast_io::fmt::basic_static_format_context_t<specification, depth> context,
	::fast_io::fmt::basic_static_format_as_t<char_type>,
	format_as_value const &) noexcept
{
	static_assert(depth <
				  ::fast_io::fmt::details::static_format_recursion_limit);
	if (context.width.present || context.precision.present)
	{
		::fast_io::fast_terminate();
	}
	return 2u;
}

template <auto specification, ::std::size_t depth,
		  ::fast_io::fmt::format_character char_type>
[[nodiscard]] constexpr char_type *format_static_reserve_define(
	::fast_io::fmt::basic_static_format_context_t<specification, depth>,
	::fast_io::fmt::basic_static_format_as_t<char_type>, char_type *output,
	format_as_value const &value) noexcept
{
	auto const number{format_as(value)};
	*output++ = static_cast<char_type>(
		domain_character<u8'0', char_type> + number / 10u);
	*output++ = static_cast<char_type>(
		domain_character<u8'0', char_type> + number % 10u);
	return output;
}

template <auto specification, ::std::size_t depth,
		  ::fast_io::fmt::format_character char_type>
[[nodiscard]] constexpr ::std::size_t format_static_reserve_size(
	::fast_io::fmt::basic_static_format_context_t<specification, depth>,
	::fast_io::fmt::basic_static_format_as_t<char_type>,
	empty_value const &) noexcept
{
	return 0u;
}

template <auto specification, ::std::size_t depth,
		  ::fast_io::fmt::format_character char_type>
[[nodiscard]] constexpr char_type *format_static_reserve_define(
	::fast_io::fmt::basic_static_format_context_t<specification, depth>,
	::fast_io::fmt::basic_static_format_as_t<char_type>, char_type *output,
	empty_value const &) noexcept
{
	return output;
}

template <auto specification, ::std::size_t depth,
		  ::fast_io::fmt::format_character char_type, auto state>
[[nodiscard]] constexpr ::std::size_t format_static_reserve_size(
	::fast_io::fmt::basic_static_format_context_t<specification, depth> context,
	::fast_io::fmt::basic_custom_format_state_t<char_type, state>,
	custom_value const &) noexcept
{
	static_assert(depth <
				  ::fast_io::fmt::details::static_format_recursion_limit);
	if (context.width.present || context.precision.present)
	{
		::fast_io::fast_terminate();
	}
	return 3u;
}

template <auto specification, ::std::size_t depth,
		  ::fast_io::fmt::format_character char_type, auto state>
[[nodiscard]] constexpr char_type *format_static_reserve_define(
	::fast_io::fmt::basic_static_format_context_t<specification, depth>,
	::fast_io::fmt::basic_custom_format_state_t<char_type, state>,
	char_type *output, custom_value const &value) noexcept
{
	*output++ = state.alternate ? domain_character<u8'q', char_type>
								: domain_character<u8'x', char_type>;
	*output++ = static_cast<char_type>(
		domain_character<u8'0', char_type> + value.value / 10u);
	*output++ = static_cast<char_type>(
		domain_character<u8'0', char_type> + value.value % 10u);
	return output;
}

} // namespace static_custom_probe

namespace malformed_static_probe
{

struct value_type
{
	unsigned value{};

	constexpr bool operator==(value_type const &) const noexcept = default;
};

inline unsigned runtime_calls{};

[[nodiscard]] inline unsigned format_as(value_type value) noexcept
{
	++runtime_calls;
	return value.value;
}

// Both return types deliberately violate the exact terminal-output protocol.
template <auto specification, ::std::size_t depth>
[[nodiscard]] constexpr bool format_static_reserve_size(
	::fast_io::fmt::basic_static_format_context_t<specification, depth>,
	::fast_io::fmt::basic_static_format_as_t<char>,
	value_type const &) noexcept
{
	return true;
}

template <auto specification, ::std::size_t depth>
[[nodiscard]] constexpr char const *format_static_reserve_define(
	::fast_io::fmt::basic_static_format_context_t<specification, depth>,
	::fast_io::fmt::basic_static_format_as_t<char>, char *output,
	value_type const &) noexcept
{
	return output;
}

} // namespace malformed_static_probe

namespace narrow_only_static_probe
{

struct value_type
{
	unsigned value{};

	constexpr bool operator==(value_type const &) const noexcept = default;
};

inline unsigned runtime_calls{};

[[nodiscard]] inline unsigned format_as(value_type value) noexcept
{
	++runtime_calls;
	return value.value;
}

template <auto specification, ::std::size_t depth>
[[nodiscard]] constexpr ::std::size_t format_static_reserve_size(
	::fast_io::fmt::basic_static_format_context_t<specification, depth>,
	::fast_io::fmt::basic_static_format_as_t<char>,
	value_type const &) noexcept
{
	return 2u;
}

template <auto specification, ::std::size_t depth>
[[nodiscard]] constexpr char *format_static_reserve_define(
	::fast_io::fmt::basic_static_format_context_t<specification, depth>,
	::fast_io::fmt::basic_static_format_as_t<char>, char *output,
	value_type const &value) noexcept
{
	*output++ = static_cast<char>('0' + value.value / 10u);
	*output++ = static_cast<char>('0' + value.value % 10u);
	return output;
}

} // namespace narrow_only_static_probe

namespace nonconstant_static_probe
{

struct value_type
{};

// A correctly shaped but non-constant implementation is still an explicit
// opt-in.  The protocol concept must accept it; invoking the consteval wrapper
// is intentionally a compile-time contract error rather than a dynamic
// fallback.
template <auto specification, ::std::size_t depth>
[[nodiscard]] inline ::std::size_t format_static_reserve_size(
	::fast_io::fmt::basic_static_format_context_t<specification, depth>,
	::fast_io::fmt::basic_static_format_as_t<char>,
	value_type const &) noexcept
{
	return 0u;
}

template <auto specification, ::std::size_t depth>
[[nodiscard]] inline char *format_static_reserve_define(
	::fast_io::fmt::basic_static_format_context_t<specification, depth>,
	::fast_io::fmt::basic_static_format_as_t<char>, char *output,
	value_type const &) noexcept
{
	return output;
}

} // namespace nonconstant_static_probe

namespace custom_tuple_fallback_probe
{

struct value_type
{
	unsigned value{};

	constexpr bool operator==(value_type const &) const noexcept = default;
};

struct leaf_type
{
	unsigned value{};

	constexpr bool operator==(leaf_type const &) const noexcept = default;
};

template <::std::size_t index>
[[nodiscard]] inline constexpr unsigned &get(value_type &value) noexcept
{
	static_assert(index == 0u);
	return value.value;
}

template <::std::size_t index>
[[nodiscard]] inline constexpr unsigned const &get(
	value_type const &value) noexcept
{
	static_assert(index == 0u);
	return value.value;
}

struct parse_state
{
	constexpr bool operator==(parse_state const &) const noexcept = default;
};

template <typename context_type>
[[nodiscard]] constexpr parse_state format_parse_define(
	::fast_io::io_type_t<value_type>, context_type) noexcept
{
	return {};
}

template <typename context_type>
[[nodiscard]] constexpr parse_state format_parse_define(
	::fast_io::io_type_t<leaf_type>, context_type) noexcept
{
	return {};
}

struct custom_node
{
	unsigned value{};
};

inline unsigned runtime_calls{};

template <typename format_tag>
[[nodiscard]] inline custom_node brace_range_format_element(
	format_tag, value_type const &value) noexcept
{
	++runtime_calls;
	return {value.value};
}

template <::fast_io::fmt::format_character char_type, auto state>
[[nodiscard]] inline custom_node format_alias_define(
	::fast_io::fmt::basic_custom_format_state_t<char_type, state>,
	value_type const &value) noexcept
{
	++runtime_calls;
	return {value.value};
}

template <::fast_io::fmt::format_character char_type, auto state>
[[nodiscard]] inline custom_node format_alias_define(
	::fast_io::fmt::basic_custom_format_state_t<char_type, state>,
	leaf_type const &value) noexcept
{
	++runtime_calls;
	return {value.value};
}

[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, custom_node>) noexcept
{
	return 1u;
}

[[nodiscard]] inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, custom_node>, char *output,
	custom_node value) noexcept
{
	*output++ = static_cast<char>('0' + value.value);
	return output;
}

} // namespace custom_tuple_fallback_probe

namespace recursive_tuple_fallback_probe
{

struct value_type
{
	unsigned value{};

	constexpr bool operator==(value_type const &) const noexcept = default;
};

template <::std::size_t index>
[[nodiscard]] inline constexpr value_type &get(value_type &value) noexcept
{
	static_assert(index == 0u);
	return value;
}

template <::std::size_t index>
[[nodiscard]] inline constexpr value_type const &get(
	value_type const &value) noexcept
{
	static_assert(index == 0u);
	return value;
}

struct custom_node
{
	unsigned value{};
};

inline unsigned runtime_calls{};

template <typename format_tag>
[[nodiscard]] inline custom_node brace_range_format_element(
	format_tag, value_type const &value) noexcept
{
	++runtime_calls;
	return {value.value};
}

[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, custom_node>) noexcept
{
	return 1u;
}

[[nodiscard]] inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, custom_node>, char *output,
	custom_node value) noexcept
{
	*output++ = static_cast<char>('0' + value.value);
	return output;
}

} // namespace recursive_tuple_fallback_probe

namespace std
{

template <>
struct tuple_size<::custom_tuple_fallback_probe::value_type>
	: integral_constant<size_t, 1u>
{};

template <>
struct tuple_element<0u, ::custom_tuple_fallback_probe::value_type>
{
	using type = unsigned;
};

template <>
struct tuple_size<::recursive_tuple_fallback_probe::value_type>
	: integral_constant<size_t, 1u>
{};

template <>
struct tuple_element<0u, ::recursive_tuple_fallback_probe::value_type>
{
	using type = ::recursive_tuple_fallback_probe::value_type;
};

} // namespace std

namespace rich_context_probe
{

struct value_type
{};

template <auto specification, ::std::size_t depth>
[[nodiscard]] constexpr ::std::size_t format_static_reserve_size(
	::fast_io::fmt::basic_static_format_context_t<specification, depth> context,
	::fast_io::fmt::basic_static_format_as_t<char>,
	value_type const &) noexcept
{
	if (!context.width.present || context.width.negative ||
		context.width.value != 7u || !context.precision.present ||
		context.precision.negative || context.precision.value != 3u)
	{
		::fast_io::fast_terminate();
	}
	return 1u;
}

template <auto specification, ::std::size_t depth>
[[nodiscard]] constexpr char *format_static_reserve_define(
	::fast_io::fmt::basic_static_format_context_t<specification, depth> context,
	::fast_io::fmt::basic_static_format_as_t<char>, char *output,
	value_type const &value) noexcept
{
	if (format_static_reserve_size(
			context, ::fast_io::fmt::basic_static_format_as_t<char>{}, value) !=
		1u)
	{
		::fast_io::fast_terminate();
	}
	*output++ = 'R';
	return output;
}

} // namespace rich_context_probe

inline constexpr auto empty_program{
	::fast_io::fmt::details::checked_program<
		::fast_io::fmt::basic_fixed_string{"{}"},
		::fast_io::fmt::brace_fmt_t>};
inline constexpr auto empty_specification{
	empty_program.fields[0u].specification};

inline constexpr auto rich_program{
	::fast_io::fmt::details::checked_program<
		::fast_io::fmt::basic_fixed_string{"{:*^7.3x}"},
		::fast_io::fmt::brace_fmt_t>};
inline constexpr auto rich_specification{
	rich_program.fields[0u].specification};

using empty_context = ::fast_io::fmt::basic_static_format_context_t<
	empty_specification, 0u>;
using depth_limit_context = ::fast_io::fmt::basic_static_format_context_t<
	empty_specification,
	::fast_io::fmt::details::static_format_recursion_limit>;
using rich_context = ::fast_io::fmt::basic_static_format_context_t<
	rich_specification, 3u>;
using narrow_formatter = ::fast_io::fmt::basic_static_format_as_t<char>;
using wide_formatter = ::fast_io::fmt::basic_static_format_as_t<wchar_t>;

static_assert(::std::same_as<typename rich_context::char_type, char>);
static_assert(rich_context::depth == 3u);
static_assert(rich_context::specification.has_fill);
static_assert(rich_context::specification.width.value == 7u);
static_assert(rich_context::specification.precision.value == 3u);

static_assert(::fast_io::fmt::details::static_format_output_adl::expression<
			  empty_context, narrow_formatter,
			  static_custom_probe::format_as_value const &>);
static_assert(!::fast_io::fmt::details::static_format_output_adl::expression<
			  empty_context, narrow_formatter,
			  malformed_static_probe::value_type const &>);
static_assert(!::fast_io::fmt::details::static_format_output_adl::expression<
			  empty_context, wide_formatter,
			  narrow_only_static_probe::value_type const &>);
static_assert(::fast_io::fmt::details::static_format_output_adl::expression<
			  empty_context, narrow_formatter,
			  nonconstant_static_probe::value_type const &>);
static_assert(!::fast_io::fmt::details::static_format_output_adl::expression<
			  depth_limit_context, narrow_formatter,
			  static_custom_probe::format_as_value const &>);

[[nodiscard]] consteval bool rich_context_round_trip()
{
	rich_context context{{7u, true, false}, {3u, true, false}};
	rich_context_probe::value_type const value{};
	char output{};
	auto const size{
		::fast_io::fmt::details::static_format_output_adl::size<
			rich_context,
			::fast_io::fmt::basic_static_format_as_t<char>>(context, value)};
	auto const end{
		::fast_io::fmt::details::static_format_output_adl::define<
			rich_context,
			::fast_io::fmt::basic_static_format_as_t<char>>(
			context, &output, value)};
	return size == 1u && end == &output + 1u && output == 'R';
}

static_assert(rich_context_round_trip());

inline constexpr ::fast_io::fmt::basic_fixed_string static_custom_format{
	"A{}B{:x}C"};

consteval auto render_static_custom_record()
{
	::std::array<char, 8u> result{};
	::fast_io::obuffer_view buffer{result};
	::fast_io::fmt::print<static_custom_format>(
		buffer,
		::fast_io::mnp::static_arg<
			static_custom_probe::format_as_value{42u}>,
		::fast_io::mnp::static_arg<static_custom_probe::custom_value{7u}>);
	return result;
}

// Custom static formatting is an IO endpoint contract. The format layer only
// lowers the grammar into core provider nodes and owns no rendered object.
inline constexpr auto static_custom_record{render_static_custom_record()};
static_assert(::std::string_view{static_custom_record.data(),
								 static_custom_record.size()} == "A42Bx07C");

inline constexpr ::fast_io::fmt::basic_fixed_string empty_static_format{
	"A{}B"};

consteval auto render_empty_static_record()
{
	::std::array<char, 2u> result{};
	::fast_io::obuffer_view buffer{result};
	::fast_io::fmt::print<empty_static_format>(
		buffer,
		::fast_io::mnp::static_arg<static_custom_probe::empty_value{}>);
	return result;
}

inline constexpr auto empty_static_record{render_empty_static_record()};
static_assert(::std::string_view{empty_static_record.data(),
								 empty_static_record.size()} == "AB");

inline constexpr ::fast_io::fmt::basic_fixed_string automatic_static_format{
	"auto={} text={}"};

consteval auto render_automatic_static_record()
{
	::std::array<char, 17u> result{};
	::fast_io::obuffer_view buffer{result};
	::fast_io::fmt::print<automatic_static_format>(
		buffer,
		::fast_io::mnp::static_arg<
			static_custom_probe::automatic_format_as_value{42u}>,
		::fast_io::mnp::static_arg<
			static_custom_probe::automatic_text_value{}>);
	return result;
}

inline constexpr auto automatic_static_record{
	render_automatic_static_record()};
static_assert(::std::string_view{automatic_static_record.data(),
								 automatic_static_record.size()} ==
			  "auto=42 text=view");

inline constexpr custom_tuple_fallback_probe::value_type
	custom_tuple_value{7u};
inline constexpr custom_tuple_fallback_probe::leaf_type
	custom_leaf_value{8u};

template <typename argument_type>
[[nodiscard]] consteval bool single_replacement_is_static()
{
	constexpr auto format_literal{
		::fast_io::fmt::basic_fixed_string{"{}"}};
	constexpr auto const &program{
		::fast_io::fmt::details::checked_program<
			format_literal, ::fast_io::fmt::brace_fmt_t>};
	constexpr auto operation{program.operations[0u]};
	static_assert(operation.kind ==
				  ::fast_io::fmt::details::format_operation_kind::replacement);
	constexpr auto field{program.fields[operation.payload_index]};
	return ::fast_io::fmt::details::static_format_replacement<
		format_literal, field, ::fast_io::fmt::brace_fmt_t,
		argument_type>();
}

using custom_tuple_argument = decltype(::fast_io::mnp::static_arg<
									   custom_tuple_value>);
static_assert(!single_replacement_is_static<custom_tuple_argument>());
using custom_element_tuple_argument = decltype(::fast_io::fmt::static_tuple_arg<custom_tuple_value>());
static_assert(!single_replacement_is_static<custom_element_tuple_argument>());
static_assert(!::fast_io::fmt::details::
				  automatic_static_format_output_budget_exceeded<
					  ::fast_io::fmt::basic_fixed_string{"{}"},
					  ::fast_io::fmt::brace_fmt_t, custom_element_tuple_argument>());
using custom_leaf_tuple_argument = decltype(::fast_io::fmt::static_tuple_arg<custom_leaf_value>());
static_assert(!single_replacement_is_static<custom_leaf_tuple_argument>());
inline constexpr recursive_tuple_fallback_probe::value_type
	recursive_tuple_value{7u};
using recursive_tuple_argument = decltype(::fast_io::mnp::static_arg<
										  recursive_tuple_value>);
static_assert(!::fast_io::fmt::details::
				   make_static_format_aggregate_shape<
					   char, recursive_tuple_fallback_probe::value_type>()
					   .supported);
static_assert(!single_replacement_is_static<recursive_tuple_argument>());

template <typename string_type, typename char_type>
[[nodiscard]] bool text_equal(
	string_type const &value, char_type const *expected)
{
	return value == expected;
}

[[nodiscard]] bool all_character_domains_fold()
{
	auto const narrow{::fast_io::fmt::concat_std<"A{}B{:x}C">(
		::fast_io::mnp::static_arg<
			static_custom_probe::format_as_value{42u}>,
		::fast_io::mnp::static_arg<
			static_custom_probe::custom_value{7u}>)};
	auto const wide{::fast_io::fmt::wconcat_std<L"A{}B{:q}C">(
		::fast_io::mnp::static_arg<
			static_custom_probe::format_as_value{42u}>,
		::fast_io::mnp::static_arg<
			static_custom_probe::custom_value{7u}>)};
	auto const utf8{::fast_io::fmt::u8concat_std<u8"A{}B{:x}C">(
		::fast_io::mnp::static_arg<
			static_custom_probe::format_as_value{42u}>,
		::fast_io::mnp::static_arg<
			static_custom_probe::custom_value{7u}>)};
	auto const utf16{::fast_io::fmt::u16concat_std<u"A{}B{:q}C">(
		::fast_io::mnp::static_arg<
			static_custom_probe::format_as_value{42u}>,
		::fast_io::mnp::static_arg<
			static_custom_probe::custom_value{7u}>)};
	auto const utf32{::fast_io::fmt::u32concat_std<U"A{}B{:x}C">(
		::fast_io::mnp::static_arg<
			static_custom_probe::format_as_value{42u}>,
		::fast_io::mnp::static_arg<
			static_custom_probe::custom_value{7u}>)};

	return text_equal(narrow, "A42Bx07C") &&
		   text_equal(wide, L"A42Bq07C") &&
		   text_equal(utf8, u8"A42Bx07C") &&
		   text_equal(utf16, u"A42Bq07C") &&
		   text_equal(utf32, U"A42Bx07C") &&
		   static_custom_probe::format_as_runtime_calls == 0u &&
		   static_custom_probe::custom_alias_runtime_calls == 0u;
}

[[nodiscard]] bool partial_and_named_folding()
{
	auto const partial{::fast_io::fmt::concat_std<
		"A{}B{:x}C{}D">(
		::fast_io::mnp::static_arg<
			static_custom_probe::format_as_value{42u}>,
		::fast_io::mnp::static_arg<
			static_custom_probe::custom_value{7u}>,
		9u)};
	auto const named{::fast_io::fmt::concat_std<"{left}-{1:q}">(
		::fast_io::mnp::static_arg<
			"left", static_custom_probe::format_as_value{13u}>,
		::fast_io::mnp::static_arg<
			static_custom_probe::custom_value{8u}>)};
	return partial == "A42Bx07C9D" && named == "13-q08" &&
		   static_custom_probe::format_as_runtime_calls == 0u &&
		   static_custom_probe::custom_alias_runtime_calls == 0u;
}

[[nodiscard]] bool automatic_format_as_folding()
{
	static_custom_probe::automatic_format_as_runtime_calls = 0u;
	auto const static_narrow{::fast_io::fmt::concat_std<"A{}B">(
		::fast_io::mnp::static_arg<
			static_custom_probe::automatic_format_as_value{42u}>)};
	auto const static_wide{::fast_io::fmt::u16concat_std<u"A{}B">(
		::fast_io::mnp::static_arg<
			static_custom_probe::automatic_format_as_value{13u}>)};
	if (static_narrow != "A42B" || static_wide != u"A13B" ||
		static_custom_probe::automatic_format_as_runtime_calls != 0u)
	{
		return false;
	}
	auto const dynamic{::fast_io::fmt::concat_std<"A{}B">(
		static_custom_probe::automatic_format_as_value{7u})};
	return dynamic == "A7B" &&
		   static_custom_probe::automatic_format_as_runtime_calls == 1u;
}

[[nodiscard]] bool dynamic_and_fail_closed_paths()
{
	static_custom_probe::format_as_runtime_calls = 0u;
	static_custom_probe::custom_alias_runtime_calls = 0u;
	auto const dynamic{::fast_io::fmt::concat_std<"{}:{:q}">(
		static_custom_probe::format_as_value{42u},
		static_custom_probe::custom_value{7u})};
	if (dynamic != "42:q07" ||
		static_custom_probe::format_as_runtime_calls != 1u ||
		static_custom_probe::custom_alias_runtime_calls != 1u)
	{
		return false;
	}

	malformed_static_probe::runtime_calls = 0u;
	auto const malformed{::fast_io::fmt::concat_std<"{}">(
		::fast_io::mnp::static_arg<
			malformed_static_probe::value_type{42u}>)};
	if (malformed != "42" || malformed_static_probe::runtime_calls != 1u)
	{
		return false;
	}

	narrow_only_static_probe::runtime_calls = 0u;
	auto const wrong_domain{::fast_io::fmt::u16concat_std<u"{}">(
		::fast_io::mnp::static_arg<
			narrow_only_static_probe::value_type{42u}>)};
	if (wrong_domain != u"42" ||
		narrow_only_static_probe::runtime_calls != 1u)
	{
		return false;
	}

	static_custom_probe::runtime_backed_text_calls = 0u;
	auto const runtime_backed{::fast_io::fmt::concat_std<"A{}B">(
		::fast_io::mnp::static_arg<
			static_custom_probe::runtime_backed_text_value{}>)};
	if (runtime_backed != "ArunB" ||
		static_custom_probe::runtime_backed_text_calls != 1u)
	{
		return false;
	}

	custom_tuple_fallback_probe::runtime_calls = 0u;
	auto const custom_tuple{::fast_io::fmt::concat_std<"{}">(
		::fast_io::mnp::static_arg<custom_tuple_value>)};
	if (custom_tuple != "7" ||
		custom_tuple_fallback_probe::runtime_calls != 1u)
	{
		return false;
	}
	custom_tuple_fallback_probe::runtime_calls = 0u;
	auto const custom_element_tuple{
		::fast_io::fmt::concat_std<"{}">(
			::fast_io::fmt::static_tuple_arg<custom_tuple_value>())};
	if (custom_element_tuple != "(7)" ||
		custom_tuple_fallback_probe::runtime_calls != 1u)
	{
		return false;
	}
	custom_tuple_fallback_probe::runtime_calls = 0u;
	auto const custom_leaf_tuple{
		::fast_io::fmt::concat_std<"{}">(
			::fast_io::fmt::static_tuple_arg<custom_leaf_value>())};
	if (custom_leaf_tuple != "(8)" ||
		custom_tuple_fallback_probe::runtime_calls != 1u)
	{
		return false;
	}
	recursive_tuple_fallback_probe::runtime_calls = 0u;
	auto const recursive_tuple{::fast_io::fmt::concat_std<"{}">(
		::fast_io::mnp::static_arg<recursive_tuple_value>)};
	if (recursive_tuple != "(7)" ||
		recursive_tuple_fallback_probe::runtime_calls != 1u)
	{
		return false;
	}

	// Locale-dependent output is deliberately excluded by lowering even though
	// the terminal CPO itself has the correct protocol shape.
	static_custom_probe::custom_alias_runtime_calls = 0u;
	auto const locale_specific{::fast_io::fmt::concat_std<"{:Lx}">(
		::fast_io::mnp::static_arg<
			static_custom_probe::custom_value{7u}>)};
	return locale_specific == "x07" &&
		   static_custom_probe::custom_alias_runtime_calls == 1u;
}

int main()
{
	if (!all_character_domains_fold())
	{
		return 1;
	}
	if (!partial_and_named_folding())
	{
		return 2;
	}
	if (!automatic_format_as_folding())
	{
		return 3;
	}
	return dynamic_and_fail_closed_paths() ? 0 : 4;
}
