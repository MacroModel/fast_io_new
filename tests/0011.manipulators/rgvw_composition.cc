#include <array>
#include <cassert>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <fast_io.h>

namespace
{

using lifetime_probe_range = ::std::vector<::std::string_view>;

template <typename range_type, typename separator_type>
concept range_view_admitted = requires(range_type &&range, separator_type &&separator) {
	::fast_io::mnp::rgvw(::std::forward<range_type>(range), ::std::forward<separator_type>(separator));
};

using comma_literal = char const (&)[2u];
static_assert(range_view_admitted<lifetime_probe_range &, comma_literal>);
static_assert(!range_view_admitted<lifetime_probe_range, comma_literal>);
static_assert(range_view_admitted<::std::span<::std::string_view>, comma_literal>);
static_assert(range_view_admitted<lifetime_probe_range &, ::std::string &>);
static_assert(!range_view_admitted<lifetime_probe_range &, ::std::string>);
static_assert(range_view_admitted<lifetime_probe_range &, ::std::string_view>);

struct counting_output
{
	using output_char_type = char;
	::std::string *storage;
	::std::size_t *write_calls;
};

struct variable_token
{
	::std::string_view value;
};

// A stable-reference alias verifies that range-view admission and implementation inspect the same cv/ref-normalized
// alias representation. Returning a value here would miss the body-only failure that motivated this regression.
struct referenced_separator
{
	::fast_io::basic_io_scatter_t<char> scatter{"::", 2u};

	inline constexpr char const *data() const noexcept
	{
		return scatter.base;
	}

	inline constexpr ::std::size_t length() const noexcept
	{
		return scatter.len;
	}

	inline constexpr ::std::string_view substr() const noexcept
	{
		return {scatter.base, scatter.len};
	}
};

inline constexpr ::fast_io::basic_io_scatter_t<char> &
print_alias_define(::fast_io::io_alias_t, referenced_separator &separator) noexcept
{
	return separator.scatter;
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, referenced_separator>) noexcept
{
	return {};
}

inline constexpr ::std::size_t
print_reserve_size(::fast_io::io_reserve_type_t<char, variable_token>) noexcept
{
	return 8u;
}

inline constexpr char *print_reserve_define(::fast_io::io_reserve_type_t<char, variable_token>, char *iter,
											 variable_token token) noexcept
{
	for (char ch : token.value)
	{
		*iter++ = ch;
	}
	return iter;
}

inline constexpr ::std::size_t
print_reserve_precise_size(::fast_io::io_reserve_type_t<char, variable_token>, variable_token token) noexcept
{
	return token.value.size();
}

[[maybe_unused]] inline constexpr char *
print_reserve_precise_define(::fast_io::io_reserve_type_t<char, variable_token> tag, char *iter,
							 ::std::size_t, variable_token token) noexcept
{
	return print_reserve_define(tag, iter, token);
}

inline constexpr counting_output output_stream_ref_define(counting_output out) noexcept
{
	return out;
}

inline constexpr ::std::size_t
full_output_coalesce_threshold(::fast_io::io_reserve_type_t<char, counting_output>) noexcept
{
	return 4096u;
}

inline void write_all_overflow_define(counting_output out, char const *first, char const *last)
{
	++*out.write_calls;
	out.storage->append(first, last);
}

} // namespace

int main()
{
	using namespace ::std::literals;
	::std::array tags{"alpha"sv, "bravo"sv, "cider"sv, "delta"sv};
	auto range{::fast_io::mnp::rgvw(tags, "::")};
	static_assert(::fast_io::dynamic_reserve_printable<char, decltype(range)>);

	::std::string const expected_range{"alpha::bravo::cider::delta"};
	assert(::fast_io::concat_std(range) == expected_range);
	::std::string separator{"::"};
	assert(::fast_io::concat_std(
			   ::fast_io::mnp::rgvw(tags, ::std::string_view{separator})) == expected_range);
	referenced_separator stable_separator;
	assert(::fast_io::concat_std(::fast_io::mnp::rgvw(tags, stable_separator)) == expected_range);

	::std::string output;
	::std::size_t write_calls{};
	counting_output sink{__builtin_addressof(output), __builtin_addressof(write_calls)};
	::fast_io::print(sink, range);
	assert(output == expected_range);
	assert(write_calls == 1u);

	output.clear();
	write_calls = 0;
	auto centered{::fast_io::mnp::middle(range, 32u, '.')};
	::fast_io::print(sink, centered);
	assert(output == "...alpha::bravo::cider::delta...");
	assert(write_calls == 1u);

	output.clear();
	write_calls = 0;
	auto note_pack{::fast_io::mnp::pack(" note=", "memo"sv)};
	auto optional_note{::fast_io::mnp::cond(true, note_pack)};
	auto record{::fast_io::mnp::pack("tags=", range, optional_note)};
	::fast_io::print(sink, record);
	assert(output == "tags=alpha::bravo::cider::delta note=memo");
	assert(write_calls == 1u);

	::std::array<::std::string_view, 0> empty_tags{};
	assert(::fast_io::concat_std(::fast_io::mnp::rgvw(empty_tags, "::")).empty());

	::std::array variable_values{variable_token{"1"sv}, variable_token{"22"sv}, variable_token{"333"sv}};
	auto variable_range{::fast_io::mnp::rgvw(variable_values, ",")};
	static_assert(::fast_io::precise_reserve_printable<char, decltype(variable_range)>);
	assert(::fast_io::concat_std(variable_range) == "1,22,333");

	output.clear();
	write_calls = 0;
	::fast_io::print(sink, ::fast_io::mnp::middle(variable_range, 15u, '.'));
	assert(output == "...1,22,333....");
	assert(write_calls == 1u);
}
