#include <fast_io_format.h>

#include <string>

namespace
{

struct line_sensitive_result
{
	::std::string text;
};

[[maybe_unused]] inline line_sensitive_result strlike_construct_define(
	::fast_io::io_strlike_type_t<char, line_sensitive_result>,
	char const *first, char const *last)
{
	return {::std::string(first, last)};
}

struct line_sensitive_sink
{
	using output_char_type = char;
	line_sensitive_result *target{};
};

inline line_sensitive_sink io_strlike_ref(
	::fast_io::io_alias_t, line_sensitive_result &result) noexcept
{
	return {__builtin_addressof(result)};
}

[[maybe_unused]] inline constexpr line_sensitive_sink output_stream_ref_define(
	line_sensitive_sink sink) noexcept
{
	return sink;
}

template <bool line, typename... argument_types>
inline void status_print_define(
	line_sensitive_sink sink, argument_types const &...)
{
	// The marker makes concat's logical line ownership observable without
	// depending on its physical allocation or character-copying strategy.
	sink.target->text = line ? "LINE" : "NO_LINE";
}

struct opaque_leaf
{};

inline constexpr ::fast_io::fmt::basic_fixed_string terminal_line_format{
	"{}\n"};

} // namespace

int main()
{
	opaque_leaf leaf{};
	auto result{
		::fast_io::fmt::details::concat_with_rule<
			line_sensitive_result, terminal_line_format>(
			::fast_io::fmt::brace_fmt_t{}, leaf)};
	// A literal LF is one lowered data component. The fmt layer must not turn
	// it into concat's `line == true` operation, whose status semantics differ.
	return result.text == "NO_LINE" ? 0 : 1;
}
