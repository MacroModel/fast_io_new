#include <array>
#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <ranges>
#include <string>
#include <string_view>

#include <fast_io.h>

namespace
{

struct counting_output
{
	using output_char_type = char;
	::std::string *storage;
	::std::size_t *write_calls;
};

struct short_obuffer_state
{
	::std::array<char, 8u> put_area{};
	char *current{put_area.data()};
	::std::string storage;
	::std::size_t write_calls{};
};

struct short_obuffer_output
{
	using output_char_type = char;
	short_obuffer_state *state;
};

struct marked_append_output
{
	using output_char_type = char;
	::std::string *storage;
	::std::size_t *write_calls;
};

inline constexpr counting_output output_stream_ref_define(counting_output out) noexcept
{
	return out;
}

inline constexpr short_obuffer_output output_stream_ref_define(short_obuffer_output out) noexcept
{
	return out;
}

inline constexpr marked_append_output output_stream_ref_define(marked_append_output out) noexcept
{
	return out;
}

inline constexpr char *obuffer_begin(short_obuffer_output out) noexcept
{
	return out.state->put_area.data();
}

inline constexpr char *obuffer_curr(short_obuffer_output out) noexcept
{
	return out.state->current;
}

inline constexpr char *obuffer_end(short_obuffer_output out) noexcept
{
	return out.state->put_area.data() + out.state->put_area.size();
}

inline constexpr void obuffer_set_curr(short_obuffer_output out, char *current) noexcept
{
	out.state->current = current;
}

inline void flush_short_obuffer(short_obuffer_output out)
{
	char *const first{out.state->put_area.data()};
	if (out.state->current != first)
	{
		out.state->storage.append(first, out.state->current);
		out.state->current = first;
		++out.state->write_calls;
	}
}

inline void output_stream_buffer_flush_define(short_obuffer_output out)
{
	flush_short_obuffer(out);
}

inline constexpr ::std::size_t
full_output_coalesce_threshold(::fast_io::io_reserve_type_t<char, counting_output>) noexcept
{
	return 4096u;
}

inline constexpr ::std::size_t
full_output_coalesce_threshold(::fast_io::io_reserve_type_t<char, short_obuffer_output>) noexcept
{
	return 4096u;
}

inline constexpr ::std::size_t
full_output_coalesce_threshold(::fast_io::io_reserve_type_t<char, marked_append_output>) noexcept
{
	return 4096u;
}

// This sink deliberately opts into heap-backed whole-run materialization.  Keeping
// the stack and dynamic policies separate makes the boundary cases below useful:
// they must not fall from one write to one 64-byte write per padding chunk.
inline constexpr ::std::size_t
full_output_dynamic_coalesce_threshold(::fast_io::io_reserve_type_t<char, counting_output>) noexcept
{
	return 1024u * 1024u;
}

inline constexpr ::std::size_t
full_output_dynamic_coalesce_threshold(::fast_io::io_reserve_type_t<char, short_obuffer_output>) noexcept
{
	return 1024u * 1024u;
}

inline constexpr ::std::size_t
full_output_dynamic_coalesce_threshold(::fast_io::io_reserve_type_t<char, marked_append_output>) noexcept
{
	return 1024u * 1024u;
}

inline constexpr ::std::true_type print_buffered_preferred_stream(
	::fast_io::io_reserve_type_t<char, marked_append_output>) noexcept
{
	// This cost marker used to outrank whole-output coalescing in the direct-width gate. The wide composite regression
	// below proves that the more specific complete-record policy must be considered first.
	return {};
}

inline void write_all_overflow_define(counting_output out, char const *first, char const *last)
{
	++*out.write_calls;
	out.storage->append(first, last);
}

inline void write_all_overflow_define(short_obuffer_output out, char const *first, char const *last)
{
	flush_short_obuffer(out);
	if (first != last)
	{
		out.state->storage.append(first, last);
		++out.state->write_calls;
	}
}

inline void write_all_overflow_define(marked_append_output out, char const *first, char const *last)
{
	++*out.write_calls;
	out.storage->append(first, last);
}

struct internal_token
{};

struct invalid_internal_token
{};

inline constexpr ::std::size_t
print_reserve_size(::fast_io::io_reserve_type_t<char, internal_token>) noexcept
{
	return 2u;
}

inline constexpr ::std::size_t
print_reserve_size(::fast_io::io_reserve_type_t<char, invalid_internal_token>) noexcept
{
	return 2u;
}

inline constexpr char *print_reserve_define(::fast_io::io_reserve_type_t<char, internal_token>, char *iter,
											 internal_token) noexcept
{
	*iter++ = '-';
	*iter++ = 'x';
	return iter;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, invalid_internal_token>, char *iter,
	invalid_internal_token) noexcept
{
	*iter++ = '-';
	*iter++ = 'x';
	return iter;
}

inline constexpr ::std::size_t
print_define_internal_shift(::fast_io::io_reserve_type_t<char, internal_token>, internal_token) noexcept
{
	return 1u;
}

inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<char, invalid_internal_token>, invalid_internal_token) noexcept
{
	// The protocol is syntactically valid but reports a run-time insertion point outside its two-character output.
	// Width dispatch must recover through right placement instead of making the field narrower than requested.
	return 3u;
}

enum class placement
{
	left,
	middle,
	right,
	internal
};

[[noreturn]] inline void test_failure() noexcept
{
	::std::abort();
}

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		test_failure();
	}
}

inline ::std::string expected_width(::std::size_t width, placement where, char fill)
{
	::std::string const child{where == placement::internal ? "-x" : "xy"};
	if (width <= child.size())
	{
		return child;
	}
	::std::size_t const padding{width - child.size()};
	switch (where)
	{
	case placement::left:
		return child + ::std::string(padding, fill);
	case placement::middle:
	{
		::std::size_t const left_padding{padding >> 1u};
		return ::std::string(left_padding, fill) + child + ::std::string(padding - left_padding, fill);
	}
	case placement::right:
		return ::std::string(padding, fill) + child;
	case placement::internal:
		return ::std::string{"-"} + ::std::string(padding, fill) + "x";
	}
	test_failure();
}

template <typename T>
inline void require_one_write(T const &formatted, ::std::string const &expected)
{
	::std::string output;
	::std::size_t write_calls{};
	counting_output sink{__builtin_addressof(output), __builtin_addressof(write_calls)};
	::fast_io::print(sink, formatted);
	require(output == expected);
	require(write_calls == 1u);
}

template <typename T>
inline void require_one_line_write(T const &formatted, ::std::string const &expected)
{
	::std::string output;
	::std::size_t write_calls{};
	counting_output sink{__builtin_addressof(output), __builtin_addressof(write_calls)};
	::fast_io::println(sink, formatted);
	require(output == expected + "\n");
	require(write_calls == 1u);
}

template <typename T>
inline void require_one_short_obuffer_write(T const &formatted, ::std::string const &expected)
{
	short_obuffer_state state;
	short_obuffer_output sink{__builtin_addressof(state)};
	::fast_io::print(sink, formatted);
	flush_short_obuffer(sink);
	require(state.storage == expected);
	require(state.write_calls == 1u);
}

template <typename T>
inline void require_one_marked_append_write(T const &formatted, ::std::string const &expected)
{
	::std::string output;
	::std::size_t write_calls{};
	marked_append_output sink{__builtin_addressof(output), __builtin_addressof(write_calls)};
	::fast_io::print(sink, formatted);
	require(output == expected);
	require(write_calls == 1u);
}

struct sized_input_range
{
	struct iterator
	{
		using value_type = ::std::string_view;
		using difference_type = ::std::ptrdiff_t;
		using iterator_concept = ::std::input_iterator_tag;
		using iterator_category = ::std::input_iterator_tag;

		::std::string_view const *current;

		inline constexpr ::std::string_view operator*() const noexcept
		{
			return *current;
		}

		inline constexpr iterator &operator++() noexcept
		{
			++current;
			return *this;
		}

		inline constexpr void operator++(int) noexcept
		{
			++current;
		}

		friend inline constexpr bool operator==(iterator, iterator) noexcept = default;
	};

	::std::string_view const *data;
	::std::size_t count;

	inline constexpr iterator begin() const noexcept
	{
		return {data};
	}

	inline constexpr iterator end() const noexcept
	{
		return {data + count};
	}

	inline constexpr ::std::size_t size() const noexcept
	{
		return count;
	}
};

static_assert(::std::ranges::input_range<sized_input_range>);
static_assert(::std::ranges::sized_range<sized_input_range>);
static_assert(!::std::ranges::forward_range<sized_input_range>);

inline void test_width_matrix()
{
	constexpr ::std::array widths{255u, 256u, 257u, 4095u, 4096u, 4097u,
									 65535u, 65536u, 65537u};
	constexpr char fill{'~'};
	for (::std::size_t const width : widths)
	{
		auto left{::fast_io::mnp::left(::std::string_view{"xy"}, width, fill)};
		require_one_write(left, expected_width(width, placement::left, fill));

		auto middle{::fast_io::mnp::middle(::std::string_view{"xy"}, width, fill)};
		require_one_write(middle, expected_width(width, placement::middle, fill));

		auto right{::fast_io::mnp::right(::std::string_view{"xy"}, width, fill)};
		require_one_write(right, expected_width(width, placement::right, fill));

		auto internal{::fast_io::mnp::internal(internal_token{}, width, fill)};
		require_one_write(internal, expected_width(width, placement::internal, fill));
	}
}

inline void test_nested_condition_pack()
{
	constexpr ::std::size_t wide_width{65537u};
	constexpr ::std::size_t inner_width{4097u};

	auto wide_left{::fast_io::mnp::left(::std::string_view{"xy"}, wide_width, 'L')};
	auto wide_right{::fast_io::mnp::right(::std::string_view{"xy"}, wide_width, 'R')};
	auto true_branch{::fast_io::mnp::pack("branch=", wide_left, ";")};
	auto false_branch{::fast_io::mnp::pack("branch=", wide_right, ";")};
	auto selected_true{::fast_io::mnp::cond(true, true_branch, false_branch)};
	auto selected_false{::fast_io::mnp::cond(false, true_branch, false_branch)};

	auto inner_internal{::fast_io::mnp::internal(internal_token{}, inner_width, 'I')};
	auto optional_payload{::fast_io::mnp::pack("optional=", inner_internal, ";")};
	auto optional_true{::fast_io::mnp::cond(true, optional_payload)};
	auto optional_false{::fast_io::mnp::cond(false, optional_payload)};

	auto true_record{::fast_io::mnp::pack("<", selected_true, optional_true, ">")};
	::std::string const expected_true{
		::std::string{"<branch="} + expected_width(wide_width, placement::left, 'L') + ";optional=" +
		expected_width(inner_width, placement::internal, 'I') + ";>"};
	require_one_write(true_record, expected_true);
	require(::fast_io::concat_std(true_record) == expected_true);

	auto false_record{::fast_io::mnp::pack("<", selected_false, optional_false, ">")};
	::std::string const expected_false{
		::std::string{"<branch="} + expected_width(wide_width, placement::right, 'R') + ";>"};
	require_one_write(false_record, expected_false);
	require(::fast_io::concat_std(false_record) == expected_false);
}

inline void test_compact_composite_width()
{
	using namespace ::std::literals;
	auto first{::fast_io::mnp::pack("a"sv, "b"sv)};
	auto second{::fast_io::mnp::pack("long"sv)};
	auto selected_true{::fast_io::mnp::cond(true, first, second)};
	auto selected_false{::fast_io::mnp::cond(false, first, second)};

	auto check = []<typename selected_type>(selected_type const &selected, ::std::string_view child) {
		constexpr ::std::size_t width{9u};
		::std::size_t const padding{width - child.size()};
		::std::size_t const middle_left{padding >> 1u};

		auto left{::fast_io::mnp::left(selected, width, '.')};
		auto middle{::fast_io::mnp::middle(selected, width, '.')};
		auto right{::fast_io::mnp::right(selected, width, '.')};
		auto internal{::fast_io::mnp::internal(selected, width, '.')};
		require_one_write(left, ::std::string(child) + ::std::string(padding, '.'));
		require_one_write(middle, ::std::string(middle_left, '.') + ::std::string(child) +
								 ::std::string(padding - middle_left, '.'));
		::std::string const expected_right{::std::string(padding, '.') + ::std::string(child)};
		require_one_write(right, expected_right);
		// Packs expose no unique sign/prefix boundary, so internal placement follows right placement.
		require_one_write(internal, expected_right);
		require_one_line_write(left, ::std::string(child) + ::std::string(padding, '.'));

		// A direct-write sink with an explicit dynamic whole-output policy must retain that policy for a top-level
		// composite width. The semantic child owns branch traversal, but that fact alone cannot justify fragmenting a
		// 64-KiB field into child and repeated-fill writes when one exact heap materialization is declared cheaper.
		constexpr ::std::size_t wide_width{65537u};
		auto wide_left{::fast_io::mnp::left(selected, wide_width, '#')};
		::std::string const expected_wide{
			::std::string(child) + ::std::string(wide_width - child.size(), '#')};
		require_one_write(wide_left, expected_wide);
		require_one_line_write(wide_left, expected_wide);
		require_one_short_obuffer_write(wide_left, expected_wide);
		require_one_marked_append_write(wide_left, expected_wide);
	};

	check(selected_true, "ab"sv);
	check(selected_false, "long"sv);
}

inline void test_invalid_internal_shift_falls_back_consistently()
{
	auto formatted{::fast_io::mnp::internal(invalid_internal_token{}, 5u, '_')};
	::std::string const expected{"___-x"};
	require_one_write(formatted, expected);
	require(::fast_io::concat_std(formatted) == expected);

	// A large existing put area selects the direct-obuffer branch, whereas `counting_output` above selects the
	// bounded materialization branch. Both must implement the same observable fallback semantics.
	::std::array<char, 16u> storage{};
	::fast_io::basic_obuffer_view<char> output(storage);
	::fast_io::print(output, formatted);
	require(::std::string_view(storage.data(), output.size()) == expected);
}

inline void test_range_category_guard()
{
	using namespace ::std::literals;
	::std::array values{"one"sv, "two"sv, "three"sv};

	sized_input_range single_pass{values.data(), values.size()};
	auto input_view{::fast_io::mnp::rgvw(single_pass, "|")};
	static_assert(!::fast_io::dynamic_reserve_printable<char, decltype(input_view)>);
	static_assert(::std::same_as<decltype(input_view),
								 ::fast_io::range_view_t<char, sized_input_range::iterator>>);

	::std::string input_output;
	::std::size_t input_calls{};
	counting_output input_sink{__builtin_addressof(input_output), __builtin_addressof(input_calls)};
	::fast_io::print(input_sink, input_view);
	require(input_output == "one|two|three");

	auto forward_view{::fast_io::mnp::rgvw(values, "|")};
	static_assert(::std::ranges::forward_range<decltype((values))>);
	static_assert(::fast_io::dynamic_reserve_printable<char, decltype(forward_view)>);
	require_one_write(forward_view, "one|two|three");
}

} // namespace

int main()
{
	test_width_matrix();
	test_nested_condition_pack();
	test_compact_composite_width();
	test_invalid_internal_shift_falls_back_consistently();
	test_range_category_guard();
}
