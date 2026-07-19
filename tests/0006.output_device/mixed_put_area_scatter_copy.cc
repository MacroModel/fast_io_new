#include <array>
#include <cassert>
#include <cstddef>
#include <string>
#include <string_view>

#include <fast_io_core.h>

namespace
{

struct put_area_state
{
	::std::array<char, 256u> storage{};
	char *current{storage.data()};
	::std::size_t capacity{storage.size()};
	::std::size_t cursor_publications{};
	::std::string completed;
};

struct put_area_sink
{
	using output_char_type = char;
	put_area_state *state;
};

inline constexpr put_area_sink output_stream_ref_define(put_area_sink sink) noexcept
{
	return sink;
}

inline constexpr ::std::true_type print_deferred_obuffer_commit_safe(
	::fast_io::io_reserve_type_t<char, put_area_sink>) noexcept
{
	return {};
}

inline constexpr char *obuffer_begin(put_area_sink sink) noexcept
{
	return sink.state->storage.data();
}

inline constexpr char *obuffer_curr(put_area_sink sink) noexcept
{
	return sink.state->current;
}

inline constexpr char *obuffer_end(put_area_sink sink) noexcept
{
	return sink.state->storage.data() + sink.state->capacity;
}

inline constexpr void obuffer_set_curr(put_area_sink sink, char *position) noexcept
{
	sink.state->current = position;
	++sink.state->cursor_publications;
}

inline void write_all_overflow_define(
	put_area_sink sink, char const *first, char const *last)
{
	auto &state{*sink.state};
	state.completed.append(state.storage.data(), state.current);
	state.current = state.storage.data();
	state.completed.append(first, last);
}

[[nodiscard]] ::std::string complete_output(put_area_state const &state)
{
	::std::string result{state.completed};
	result.append(
		state.storage.data(),
		static_cast<::std::size_t>(state.current - state.storage.data()));
	return result;
}

[[nodiscard]] ::std::string expected_output(bool line)
{
	::std::string result{"a"};
	result.append(23u, 'b');
	result.push_back('c');
	result.append(23u, 'd');
	if (line)
	{
		result.push_back('\n');
	}
	return result;
}

void print_mixed(put_area_state &state, bool line)
{
	::std::string const first(23u, 'd');
	::std::string const second(23u, 'b');
	if (line)
	{
		::fast_io::operations::print_freestanding<true>(
			put_area_sink{__builtin_addressof(state)}, "a",
			::fast_io::basic_io_scatter_t<char>{second.data(), second.size()}, "c",
			::fast_io::basic_io_scatter_t<char>{first.data(), first.size()});
	}
	else
	{
		::fast_io::operations::print_freestanding<false>(
			put_area_sink{__builtin_addressof(state)}, "a",
			::fast_io::basic_io_scatter_t<char>{second.data(), second.size()}, "c",
			::fast_io::basic_io_scatter_t<char>{first.data(), first.size()});
	}
}

void test_single_preflight_and_publication()
{
	put_area_state state;
	print_mixed(state, false);
	assert(complete_output(state) == expected_output(false));
	assert(state.cursor_publications == 1u);
}

void test_line_is_owned_by_the_same_publication()
{
	put_area_state state;
	print_mixed(state, true);
	assert(complete_output(state) == expected_output(true));
	assert(state.cursor_publications == 1u);
}

void test_capacity_miss_uses_the_ordinary_fallback()
{
	put_area_state state;
	state.capacity = 7u;
	print_mixed(state, false);
	assert(complete_output(state) == expected_output(false));
}

void test_empty_dynamic_scatter_keeps_static_order()
{
	put_area_state state;
	::fast_io::operations::print_freestanding<false>(
		put_area_sink{__builtin_addressof(state)}, "a",
		::fast_io::basic_io_scatter_t<char>{nullptr, 0u}, "c",
		::fast_io::basic_io_scatter_t<char>{nullptr, 0u});
	assert(complete_output(state) == "ac");
	assert(state.cursor_publications == 1u);
}

} // namespace

int main()
{
	test_single_preflight_and_publication();
	test_line_is_owned_by_the_same_publication();
	test_capacity_miss_uses_the_ordinary_fallback();
	test_empty_dynamic_scatter_keeps_static_order();
}
