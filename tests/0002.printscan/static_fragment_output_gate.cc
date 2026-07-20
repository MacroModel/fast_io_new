#include <array>
#include <cassert>
#include <cstddef>
#include <string_view>
#include <type_traits>

#include <fast_io.h>
#include <fast_io_format.h>

namespace
{

struct capture_state
{
	char const *expected_source{};
	std::array<char, 64u> bytes{};
	std::size_t size{};
	std::size_t write_calls{};
	std::size_t scatter_calls{};
	bool observed_expected_source{};
};

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline void capture_range(
	capture_state &state, char const *first, char const *last) noexcept
{
	++state.write_calls;
	state.observed_expected_source |= first == state.expected_source;
	for (; first != last; ++first)
	{
		state.bytes[state.size++] = *first;
	}
}

struct direct_sink
{
	using output_char_type = char;
	capture_state *state{};
};

struct unmarked_scatter_adapter
{
	using output_char_type = char;
	capture_state *state{};
};

inline constexpr direct_sink output_stream_ref_define(direct_sink sink) noexcept
{
	return sink;
}

inline constexpr unmarked_scatter_adapter output_stream_ref_define(
	unmarked_scatter_adapter sink) noexcept
{
	return sink;
}

inline constexpr std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char, direct_sink>) noexcept
{
	return {};
}

inline void write_all_overflow_define(
	direct_sink sink, char const *first, char const *last) noexcept
{
	capture_range(*sink.state, first, last);
}

inline void write_all_overflow_define(
	unmarked_scatter_adapter sink, char const *first, char const *last) noexcept
{
	capture_range(*sink.state, first, last);
}

template <typename sink_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline void capture_scatters(
	sink_type sink, ::fast_io::basic_io_scatter_t<char> const *scatters,
	std::size_t count) noexcept
{
	++sink.state->scatter_calls;
	for (std::size_t index{}; index != count; ++index)
	{
		auto const [base, len]{scatters[index]};
		sink.state->observed_expected_source |=
			base == sink.state->expected_source;
		for (auto first{base}, last{base + len}; first != last; ++first)
		{
			sink.state->bytes[sink.state->size++] = *first;
		}
	}
}

inline void scatter_write_all_overflow_define(
	direct_sink sink, ::fast_io::basic_io_scatter_t<char> const *scatters,
	std::size_t count) noexcept
{
	capture_scatters(sink, scatters, count);
}

inline void scatter_write_all_overflow_define(
	unmarked_scatter_adapter sink,
	::fast_io::basic_io_scatter_t<char> const *scatters,
	std::size_t count) noexcept
{
	capture_scatters(sink, scatters, count);
}

static_assert(::fast_io::synchronous_direct_scatter_output<char, direct_sink>);
static_assert(!::fast_io::synchronous_direct_scatter_output<
	char, unmarked_scatter_adapter>);
static_assert(!::fast_io::synchronous_direct_scatter_output<
	char, ::fast_io::basic_obuffer_view_ref<char>>);
static_assert(
	::fast_io::details::decay::print_output_retains_static_scatter<direct_sink>);
static_assert(!::fast_io::details::decay::print_output_retains_static_scatter<
	unmarked_scatter_adapter>);
static_assert(!::fast_io::details::decay::print_output_retains_static_scatter<
	::fast_io::basic_obuffer_view_ref<char>>);

template <typename sink_type>
capture_state print_static_source(sink_type sink, char const *source) noexcept
{
	static_cast<void>(source);
	::fast_io::print(sink,
		::fast_io::manipulators::static_scatter_t<char, 6u>{source});
	return *sink.state;
}

void test_direct_marker_and_unmarked_adapter()
{
	static constexpr char source[]{'d', 'i', 'r', 'e', 'c', 't'};

	capture_state direct_state{.expected_source = source};
	auto const direct{print_static_source(
		direct_sink{__builtin_addressof(direct_state)}, source)};
	assert(direct.observed_expected_source);
	assert(std::string_view(direct.bytes.data(), direct.size) == "direct");

	capture_state adapter_state{.expected_source = source};
	auto const adapted{print_static_source(
		unmarked_scatter_adapter{__builtin_addressof(adapter_state)}, source)};
	// A scatter-shaped adapter without the synchronous direct-output proof must
	// materialize even one fixed fragment into destination-owned scratch.
	assert(!adapted.observed_expected_source);
	assert(std::string_view(adapted.bytes.data(), adapted.size) == "direct");
}

void test_static_argument_provider_gate()
{
	constexpr auto provider{::fast_io::fmt::status_io_print_forward(
		::fast_io::io_alias_type<char>, ::fast_io::fmt::static_arg<42>)};

	capture_state direct_state{.expected_source = provider.base};
	::fast_io::io::print(
		direct_sink{__builtin_addressof(direct_state)},
		::fast_io::fmt::static_arg<42>);
	assert(direct_state.observed_expected_source);
	assert(std::string_view(direct_state.bytes.data(), direct_state.size) ==
		"42");

	capture_state adapter_state{.expected_source = provider.base};
	::fast_io::io::print(
		unmarked_scatter_adapter{__builtin_addressof(adapter_state)},
		::fast_io::fmt::static_arg<42>);
	assert(!adapter_state.observed_expected_source);
	assert(std::string_view(adapter_state.bytes.data(), adapter_state.size) ==
		"42");
}

void test_put_area_stays_destination_owned()
{
	std::array<char, 64u> storage{};
	::fast_io::basic_obuffer_view<char> output(storage);
	::fast_io::print(output, 2, ::fast_io::mnp::boolalpha(true),
		::fast_io::mnp::decimal(1.25));
	assert(std::string_view(output.data(), output.size()) == "2true1.25");

	std::array<char, 64u> format_storage{};
	::fast_io::basic_obuffer_view<char> format_output(format_storage);
	::fast_io::fmt::print<"{}|{}|{}">(format_output, 2, true, 1.25);
	assert(std::string_view(format_output.data(), format_output.size()) ==
		"2|1|1.25");
}

} // namespace

int main()
{
	test_direct_marker_and_unmarked_adapter();
	test_static_argument_provider_gate();
	test_put_area_stays_destination_owned();
}
