#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <fast_io.h>

namespace
{

struct input_reader
{
	::std::uint8_t const *data;
	::std::size_t size;
	::std::size_t position{};

	inline ::std::uint8_t take() noexcept
	{
		if (position == size)
		{
			return 0u;
		}
		return data[position++];
	}

	inline ::std::uint32_t take_u32() noexcept
	{
		::std::uint32_t value{};
		for (unsigned shift{}; shift != 32u; shift += 8u)
		{
			value |= static_cast<::std::uint32_t>(take()) << shift;
		}
		return value;
	}

	inline ::std::string take_string(::std::size_t count)
	{
		::std::string result;
		result.reserve(count);
		for (::std::size_t i{}; i != count; ++i)
		{
			result.push_back(static_cast<char>(take()));
		}
		return result;
	}
};

struct counting_output
{
	using output_char_type = char;
	::std::string *storage;
	::std::size_t *write_calls;
};

inline constexpr counting_output output_stream_ref_define(counting_output out) noexcept
{
	return out;
}

inline constexpr ::std::size_t
full_output_coalesce_threshold(::fast_io::io_reserve_type_t<char, counting_output>) noexcept
{
	return 4096u;
}

inline constexpr ::std::size_t
full_output_dynamic_coalesce_threshold(::fast_io::io_reserve_type_t<char, counting_output>) noexcept
{
	return 1024u * 1024u;
}

inline void write_all_overflow_define(counting_output out, char const *first, char const *last)
{
	++*out.write_calls;
	out.storage->append(first, last);
}

// A deliberately nonnumeric leaf with an internal-padding insertion point.
struct text_token
{};

inline constexpr ::std::size_t
print_reserve_size(::fast_io::io_reserve_type_t<char, text_token>) noexcept
{
	return 5u;
}

inline constexpr char *
print_reserve_define(::fast_io::io_reserve_type_t<char, text_token>, char *iter, text_token) noexcept
{
	*iter++ = '@';
	*iter++ = 'l';
	*iter++ = 'e';
	*iter++ = 'a';
	*iter++ = 'f';
	return iter;
}

inline constexpr ::std::size_t
print_define_internal_shift(::fast_io::io_reserve_type_t<char, text_token>, text_token) noexcept
{
	return 1u;
}

enum class placement
{
	left,
	middle,
	right,
	internal
};

[[noreturn]] inline void fail() noexcept
{
	__builtin_trap();
}

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		fail();
	}
}

inline void append_width(::std::string &output, ::std::size_t width, placement where, char fill)
{
	constexpr ::std::string_view child{"@leaf"};
	if (width <= child.size())
	{
		output.append(child);
		return;
	}

	::std::size_t const padding{width - child.size()};
	switch (where)
	{
	case placement::left:
		output.append(child);
		output.append(padding, fill);
		return;
	case placement::middle:
	{
		::std::size_t const left_padding{padding >> 1u};
		output.append(left_padding, fill);
		output.append(child);
		output.append(padding - left_padding, fill);
		return;
	}
	case placement::right:
		output.append(padding, fill);
		output.append(child);
		return;
	case placement::internal:
		output.push_back(child.front());
		output.append(padding, fill);
		output.append(child.substr(1u));
		return;
	}
	fail();
}

inline ::std::string make_reference(bool select_left, bool include_middle, ::std::size_t width,
									char fill, ::std::vector<::std::string_view> const &values,
									::std::string_view separator)
{
	::std::string result{"record:{"};
	if (select_left)
	{
		result.append("left=");
		append_width(result, width, placement::left, fill);
	}
	else
	{
		result.append("right=");
		append_width(result, width, placement::right, fill);
	}

	if (include_middle)
	{
		result.append("|middle=");
		append_width(result, width, placement::middle, fill);
	}

	result.append("|internal=");
	append_width(result, width, placement::internal, fill);
	result.append("|range=");
	for (::std::size_t i{}; i != values.size(); ++i)
	{
		if (i != 0u)
		{
			result.append(separator);
		}
		result.append(values[i]);
	}
	result.append("}:end");
	return result;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const *data, ::std::size_t size)
{
	input_reader input{data, size};
	::std::size_t const width{input.take_u32() % 70001u};
	char const fill{static_cast<char>(input.take())};
	::std::uint8_t const predicates{input.take()};
	bool const select_left{(predicates & 1u) != 0u};
	bool const include_middle{(predicates & 2u) != 0u};
	::std::size_t const range_count{input.take() % 17u};
	::std::size_t const element_length{input.take() % 65u};
	::std::size_t const separator_length{input.take() % 17u};

	::std::string separator{input.take_string(separator_length)};
	::std::vector<::std::string> storage;
	storage.reserve(range_count);
	for (::std::size_t i{}; i != range_count; ++i)
	{
		storage.push_back(input.take_string(element_length));
	}

	::std::vector<::std::string_view> values;
	values.reserve(range_count);
	for (auto const &value : storage)
	{
		values.emplace_back(value);
	}

	auto range{::fast_io::mnp::rgvw(values, ::std::string_view{separator})};
	static_assert(::fast_io::dynamic_reserve_printable<char, decltype(range)>);

	text_token token;
	auto left{::fast_io::mnp::left(token, width, fill)};
	auto middle{::fast_io::mnp::middle(token, width, fill)};
	auto right{::fast_io::mnp::right(token, width, fill)};
	auto internal{::fast_io::mnp::internal(token, width, fill)};

	auto left_branch{::fast_io::mnp::pack("left=", left)};
	auto right_branch{::fast_io::mnp::pack("right=", right)};
	auto selected{::fast_io::mnp::cond(select_left, left_branch, right_branch)};
	auto middle_payload{::fast_io::mnp::pack("|middle=", middle)};
	auto optional_middle{::fast_io::mnp::cond(include_middle, middle_payload)};
	auto inner{::fast_io::mnp::pack("{", selected, optional_middle, "|internal=", internal,
									"|range=", range, "}")};
	auto record{::fast_io::mnp::pack("record:", inner, ":end")};

	::std::string const expected{
		make_reference(select_left, include_middle, width, fill, values, separator)};
	::std::string const concatenated{::fast_io::concat_std(record)};
	require(concatenated == expected);

	::std::string printed;
	::std::size_t write_calls{};
	counting_output sink{__builtin_addressof(printed), __builtin_addressof(write_calls)};
	::fast_io::print(sink, record);
	require(printed == expected);
	require(write_calls == 1u);
	return 0;
}
