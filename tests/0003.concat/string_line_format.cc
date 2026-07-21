#include <fast_io_format.h>

#include <cstddef>
#include <cstdlib>
#include <string>
#include <string_view>

namespace
{

template <typename char_type>
struct line_formatter;

template <>
struct line_formatter<char>
{
	[[nodiscard]] static auto render(::std::string_view value)
	{
		return ::fast_io::fmt::concat_std<"{}\n">(value);
	}
};

template <>
struct line_formatter<wchar_t>
{
	[[nodiscard]] static auto render(::std::wstring_view value)
	{
		return ::fast_io::fmt::wconcat_std<L"{}\n">(value);
	}
};

template <>
struct line_formatter<char8_t>
{
	[[nodiscard]] static auto render(::std::u8string_view value)
	{
		return ::fast_io::fmt::u8concat_std<u8"{}\n">(value);
	}
};

template <>
struct line_formatter<char16_t>
{
	[[nodiscard]] static auto render(::std::u16string_view value)
	{
		return ::fast_io::fmt::u16concat_std<u"{}\n">(value);
	}
};

template <>
struct line_formatter<char32_t>
{
	[[nodiscard]] static auto render(::std::u32string_view value)
	{
		return ::fast_io::fmt::u32concat_std<U"{}\n">(value);
	}
};

template <typename char_type>
void test_domain()
{
	constexpr ::std::size_t sizes[]{0u, 1u, 14u, 15u, 16u, 22u, 23u, 31u, 32u, 8192u};
	for (::std::size_t size : sizes)
	{
		::std::basic_string<char_type> source(size, static_cast<char_type>('x'));
		auto const before{source};
		auto const result{line_formatter<char_type>::render(
			::std::basic_string_view<char_type>{source})};
		if (source != before || result.size() != size + 1u ||
			result.back() != static_cast<char_type>('\n') ||
			result.compare(0u, size, source) != 0)
		{
			::std::abort();
		}
	}
}

} // namespace

inline constexpr ::fast_io::fmt::basic_fixed_string full_static_line_format{
	"A{}B\n"};
using full_static_line_argument =
	decltype(::fast_io::mnp::static_arg<42u>);

template <::fast_io::fmt::basic_fixed_string format_literal,
		  ::std::size_t field_index, typename... argument_types>
[[nodiscard]] consteval bool replacement_is_static() noexcept
{
	constexpr auto const &program{
		::fast_io::fmt::details::checked_program<
			format_literal, ::fast_io::fmt::brace_fmt_t>};
	static_assert(field_index < program.field_count);
	return ::fast_io::fmt::details::static_format_replacement<
		format_literal, program.fields[field_index],
		::fast_io::fmt::brace_fmt_t, argument_types...>();
}

static_assert(replacement_is_static<
			  full_static_line_format, 0u, full_static_line_argument>());

inline constexpr ::fast_io::fmt::basic_fixed_string partial_static_line_format{
	"A{}B{}C\n"};
using partial_static_line_argument =
	decltype(::fast_io::mnp::static_arg<42u>);
static_assert(replacement_is_static<
			  partial_static_line_format, 0u, partial_static_line_argument,
			  ::std::string_view>());
static_assert(!replacement_is_static<
			  partial_static_line_format, 1u, partial_static_line_argument,
			  ::std::string_view>());

consteval bool constexpr_line_format_is_correct()
{
	auto const result{::fast_io::fmt::concat_std<"{}\n">(
		::std::string_view{"constant"})};
	auto const pack9{::fast_io::fmt::concat_std<"{}{}{}{}{}{}{}{}{}\n">(
		"a", "b", "c", "d", "e", "f", "g", "h", "i")};
	auto const fully_static{::fast_io::fmt::concat_std<"A{}B\n">(
		::fast_io::mnp::static_arg<42u>)};
	auto const empty_static{::fast_io::fmt::concat_std<"{}\n">(
		::fast_io::mnp::static_arg<"">)};
	return ::fast_io::fmt::concat_std<"\n">() == "\n" &&
		   ::fast_io::fmt::concat_std<"a">() == "a" &&
		   ::fast_io::fmt::concat_std<"ab">() == "ab" &&
		   ::fast_io::fmt::concat_std<"x\n">() == "x\n" &&
		   result == "constant\n" && pack9 == "abcdefghi\n" &&
		   fully_static == "A42B\n" && empty_static == "\n";
}

static_assert(constexpr_line_format_is_correct());

int main()
{
	if (::fast_io::fmt::concat_std<"\n">() != "\n" ||
		::fast_io::fmt::concat_std<"a">() != "a" ||
		::fast_io::fmt::concat_std<"ab">() != "ab" ||
		::fast_io::fmt::concat_std<"x\n">() != "x\n" ||
		::fast_io::fmt::wconcat_std<L"x\n">() != L"x\n" ||
		::fast_io::fmt::u8concat_std<u8"x\n">() != u8"x\n" ||
		::fast_io::fmt::u16concat_std<u"x\n">() != u"x\n" ||
		::fast_io::fmt::u32concat_std<U"x\n">() != U"x\n" ||
		::fast_io::fmt::concat_std<"{}\n">(::std::string_view{}) != "\n")
	{
		::std::abort();
	}
	test_domain<char>();
	test_domain<wchar_t>();
	test_domain<char8_t>();
	test_domain<char16_t>();
	test_domain<char32_t>();

	::std::string_view const text{"abc"};
	if (::fast_io::fmt::concat_std<"{}!">(text) != "abc!" ||
		::fast_io::fmt::concat_std<"{}{}">(text, '\n') != "abc\n" ||
		::fast_io::fmt::concat_std<"[{}]\n">(text) != "[abc]\n" ||
		::fast_io::fmt::concat_std<"<{}:{}:{}>\n">(text, 42u, 'X') !=
			"<abc:42:X>\n" ||
		::fast_io::fmt::concat_std<"{}{}{}{}{}{}{}{}{}\n">(
			"a", "b", "c", "d", "e", "f", "g", "h", "i") !=
			"abcdefghi\n" ||
		::fast_io::fmt::concat_std<"A{}B{}C\n">(
			::fast_io::mnp::static_arg<42u>, text) != "A42BabcC\n" ||
		::fast_io::fmt::concat_std<"{}{}\n">(
			text, ::fast_io::mnp::static_arg<"">) != "abc\n" ||
		::fast_io::fmt::concat_std<"{name}:{fixed}\n">(
			::fast_io::fmt::arg<"name">(text),
			::fast_io::mnp::static_arg<"fixed", 42u>) != "abc:42\n" ||
		::fast_io::fmt::concat_std<"{{x}}={}\n">(42u) != "{x}=42\n" ||
		::fast_io::fmt::concatf_std<"[%s:%u]\n">(text, 42u) != "[abc:42]\n")
	{
		::std::abort();
	}
}
