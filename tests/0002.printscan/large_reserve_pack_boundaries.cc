#include <fast_io.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace large_reserve_pack_boundaries_test
{

inline constexpr ::std::size_t maximum_field_count{224u};

struct one_char
{
	char value{};
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, one_char>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, one_char>, char *destination,
	one_char value) noexcept
{
	*destination = value.value;
	return destination + 1u;
}

struct capture_state
{
	::std::array<char, maximum_field_count> bytes{};
	::std::size_t size{};
	::std::size_t write_calls{};
	::std::size_t scatter_calls{};
};

struct direct_sink
{
	using output_char_type = char;
	capture_state *state{};
};

inline constexpr direct_sink output_stream_ref_define(direct_sink sink) noexcept
{
	return sink;
}

inline constexpr ::std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char, direct_sink>) noexcept
{
	return {};
}

inline void write_all_overflow_define(
	direct_sink sink, char const *first, char const *last) noexcept
{
	auto &state{*sink.state};
	++state.write_calls;
	for (; first != last; ++first)
	{
		state.bytes[state.size++] = *first;
	}
}

inline void scatter_write_all_overflow_define(
	direct_sink sink, ::fast_io::basic_io_scatter_t<char> const *scatters,
	::std::size_t count) noexcept
{
	auto &state{*sink.state};
	++state.scatter_calls;
	for (::std::size_t index{}; index != count; ++index)
	{
		for (auto first{scatters[index].base}, last{first + scatters[index].len};
			 first != last; ++first)
		{
			state.bytes[state.size++] = *first;
		}
	}
}

template <::std::size_t count, ::std::size_t... indexes>
consteval auto scan_large_pack(::std::index_sequence<indexes...>)
{
	return ::fast_io::details::decay::find_continuous_scatters_reserve_n<
		false, char,
		::std::conditional_t<(indexes < count), one_char, void>...>();
}

template <::std::size_t count>
inline constexpr auto scan_result{
	scan_large_pack<count>(::std::make_index_sequence<count>{})};

static_assert(scan_result<96u>.position == 96u &&
			  scan_result<96u>.neededspace == 96u &&
			  scan_result<96u>.null == 0u);
static_assert(scan_result<97u>.position == 97u &&
			  scan_result<97u>.neededspace == 97u &&
			  scan_result<97u>.null == 0u);
static_assert(scan_result<109u>.position == 109u &&
			  scan_result<109u>.neededspace == 109u &&
			  scan_result<109u>.null == 0u);
static_assert(scan_result<224u>.position == 224u &&
			  scan_result<224u>.neededspace == 224u &&
			  scan_result<224u>.null == 0u);

template <::std::size_t count, ::std::size_t... indexes>
inline void emit_large_pack(
	direct_sink sink, ::std::array<one_char, count> &values,
	::std::index_sequence<indexes...>)
{
	::fast_io::io::print(sink, values[indexes]...);
}

template <::std::size_t count>
inline void test_large_pack()
{
	::std::array<one_char, count> values{};
	for (::std::size_t index{}; index != count; ++index)
	{
		values[index].value = static_cast<char>('a' + index % 26u);
	}
	capture_state state{};
	emit_large_pack(direct_sink{__builtin_addressof(state)}, values,
					::std::make_index_sequence<count>{});
	assert(state.write_calls == 1u);
	assert(state.scatter_calls == 0u);
	assert(state.size == count);
	for (::std::size_t index{}; index != count; ++index)
	{
		assert(state.bytes[index] == static_cast<char>('a' + index % 26u));
	}
}

} // namespace large_reserve_pack_boundaries_test

int main()
{
	using namespace large_reserve_pack_boundaries_test;
	test_large_pack<96u>();
	test_large_pack<97u>();
	test_large_pack<109u>();
	test_large_pack<224u>();
}
