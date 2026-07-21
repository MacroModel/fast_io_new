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

struct scalar_only_sink
{
	using output_char_type = char;
	capture_state *state{};
};

struct scatter_only_sink
{
	using output_char_type = char;
	capture_state *state{};
};

struct large_provider_recipe
{
	using char_type = char;
	static inline constexpr std::size_t size{40000u};

	[[nodiscard]] inline static consteval char *emit(char *output) noexcept
	{
		for (std::size_t index{}; index != size; ++index)
		{
			output[index] = static_cast<char>('a' + index % 23u);
		}
		return output + size;
	}
};

using large_provider_node = ::fast_io::manipulators::static_provider_node<
	large_provider_recipe, 0u, large_provider_recipe::size>;

struct greedy_provider_recipe
{
	using char_type = char;
	static inline constexpr std::size_t size{22000u};

	[[nodiscard]] inline static consteval char *emit(char *output) noexcept
	{
		for (std::size_t index{}; index != size; ++index)
		{
			output[index] = static_cast<char>('A' + index % 17u);
		}
		return output + size;
	}
};

using greedy_provider_node = ::fast_io::manipulators::static_provider_node<
	greedy_provider_recipe, 0u, greedy_provider_recipe::size>;

struct large_capture_state
{
	std::size_t bytes{};
	std::size_t write_calls{};
	std::size_t scatter_calls{};
	std::size_t descriptor_count{};
	bool content_matches{true};
};

struct large_direct_sink
{
	using output_char_type = char;
	large_capture_state *state{};
};

struct greedy_capture_state
{
	std::array<char const *, 4u> sources{};
	std::array<std::size_t, 4u> sizes{};
	std::size_t bytes{};
	std::size_t write_calls{};
	std::size_t scatter_calls{};
	std::size_t descriptor_count{};
	bool content_matches{true};
};

struct greedy_direct_sink
{
	using output_char_type = char;
	greedy_capture_state *state{};
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

inline constexpr scalar_only_sink output_stream_ref_define(
	scalar_only_sink sink) noexcept
{
	return sink;
}

inline constexpr scatter_only_sink output_stream_ref_define(
	scatter_only_sink sink) noexcept
{
	return sink;
}

inline constexpr large_direct_sink output_stream_ref_define(
	large_direct_sink sink) noexcept
{
	return sink;
}

inline constexpr greedy_direct_sink output_stream_ref_define(
	greedy_direct_sink sink) noexcept
{
	return sink;
}

// This ADL hook is consumed by a concept check; the scalar-provider cases below
// intentionally need no runtime call to it.
[[maybe_unused]] inline constexpr std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char, direct_sink>) noexcept
{
	return {};
}

// Scalar and scatter pointer-lifetime promises are independent. These probes
// cover all four capability combinations so one protocol cannot accidentally
// become a proxy for the other during later dispatch refactoring.
[[maybe_unused]] inline constexpr std::true_type print_synchronous_direct_scalar_output(
	::fast_io::io_reserve_type_t<char, direct_sink>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr std::true_type print_synchronous_direct_scalar_output(
	::fast_io::io_reserve_type_t<char, scalar_only_sink>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char, scatter_only_sink>) noexcept
{
	return {};
}

// The large-record regression models an unbuffered synchronous endpoint. Only
// that lifetime contract permits provider pointers to survive into scatter IO.
[[maybe_unused]] inline constexpr std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char, large_direct_sink>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr std::true_type print_synchronous_direct_scalar_output(
	::fast_io::io_reserve_type_t<char, large_direct_sink>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char, greedy_direct_sink>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr std::true_type print_synchronous_direct_scalar_output(
	::fast_io::io_reserve_type_t<char, greedy_direct_sink>) noexcept
{
	return {};
}

inline constexpr std::size_t scatter_direct_full_output_coalesce_threshold(
	::fast_io::io_reserve_type_t<char, large_direct_sink>) noexcept
{
	return 64u;
}

inline constexpr std::size_t scatter_direct_full_output_coalesce_threshold(
	::fast_io::io_reserve_type_t<char, greedy_direct_sink>) noexcept
{
	return 64u;
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

inline void write_all_overflow_define(
	scalar_only_sink sink, char const *first, char const *last) noexcept
{
	capture_range(*sink.state, first, last);
}

inline void validate_large_range(
	large_capture_state &state, char const *first, char const *last) noexcept
{
	for (; first != last; ++first)
	{
		auto const record_offset{state.bytes % large_provider_recipe::size};
		state.content_matches &=
			*first == static_cast<char>('a' + record_offset % 23u);
		++state.bytes;
	}
}

// This scalar hook is intentionally retained as a negative counter: a record
// larger than the provider-merge cap must never select it.
[[maybe_unused]] inline void write_all_overflow_define(
	large_direct_sink sink, char const *first, char const *last) noexcept
{
	++sink.state->write_calls;
	validate_large_range(*sink.state, first, last);
}

[[maybe_unused]] inline void write_all_overflow_define(
	greedy_direct_sink sink, char const *, char const *) noexcept
{
	++sink.state->write_calls;
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

// Keep the direct-sink scatter endpoint available for the concept matrix even
// when the provider shortcut proves that a scalar write is sufficient.
[[maybe_unused]] inline void scatter_write_all_overflow_define(
	direct_sink sink, ::fast_io::basic_io_scatter_t<char> const *scatters,
	std::size_t count) noexcept
{
	capture_scatters(sink, scatters, count);
}

// The unmarked adapter is a negative lifetime-control case; some optimized
// instantiations materialize it through scalar writes and leave this ADL
// endpoint intentionally unevaluated.
[[maybe_unused]] inline void scatter_write_all_overflow_define(
	unmarked_scatter_adapter sink,
	::fast_io::basic_io_scatter_t<char> const *scatters,
	std::size_t count) noexcept
{
	capture_scatters(sink, scatters, count);
}

inline void scatter_write_all_overflow_define(
	scatter_only_sink sink,
	::fast_io::basic_io_scatter_t<char> const *scatters,
	std::size_t count) noexcept
{
	capture_scatters(sink, scatters, count);
}

inline void scatter_write_all_overflow_define(
	large_direct_sink sink,
	::fast_io::basic_io_scatter_t<char> const *scatters,
	std::size_t count) noexcept
{
	++sink.state->scatter_calls;
	sink.state->descriptor_count += count;
	for (std::size_t index{}; index != count; ++index)
	{
		auto const [base, len]{scatters[index]};
		validate_large_range(*sink.state, base, base + len);
	}
}

inline void scatter_write_all_overflow_define(
	greedy_direct_sink sink,
	::fast_io::basic_io_scatter_t<char> const *scatters,
	std::size_t count) noexcept
{
	++sink.state->scatter_calls;
	for (std::size_t index{}; index != count; ++index)
	{
		auto const [base, len]{scatters[index]};
		auto const descriptor{sink.state->descriptor_count++};
		sink.state->sources[descriptor] = base;
		sink.state->sizes[descriptor] = len;
		for (auto first{base}, last{base + len}; first != last; ++first)
		{
			if (sink.state->bytes < greedy_provider_recipe::size * 3u)
			{
				auto const offset{
					sink.state->bytes % greedy_provider_recipe::size};
				sink.state->content_matches &=
					*first == static_cast<char>('A' + offset % 17u);
			}
			else
			{
				sink.state->content_matches &=
					sink.state->bytes == greedy_provider_recipe::size * 3u &&
					*first == '|';
			}
			++sink.state->bytes;
		}
	}
}

static_assert(::fast_io::synchronous_direct_scalar_output<char, direct_sink>);
static_assert(::fast_io::synchronous_direct_scatter_output<char, direct_sink>);
static_assert(::fast_io::synchronous_direct_scalar_output<char, scalar_only_sink>);
static_assert(!::fast_io::synchronous_direct_scatter_output<char, scalar_only_sink>);
static_assert(!::fast_io::synchronous_direct_scalar_output<char, scatter_only_sink>);
static_assert(::fast_io::synchronous_direct_scatter_output<char, scatter_only_sink>);
static_assert(!::fast_io::synchronous_direct_scalar_output<
	char, unmarked_scatter_adapter>);
static_assert(!::fast_io::synchronous_direct_scatter_output<
			  char, unmarked_scatter_adapter>);
static_assert(::fast_io::details::decay::
			  print_output_accepts_static_provider_scalar<direct_sink>);
static_assert(::fast_io::details::decay::
			  print_output_accepts_static_provider_scalar<scalar_only_sink>);
static_assert(!::fast_io::details::decay::
			  print_output_accepts_static_provider_scalar<scatter_only_sink>);
static_assert(!::fast_io::details::decay::
			  print_output_accepts_static_provider_scalar<
				  unmarked_scatter_adapter>);
static_assert(!::fast_io::synchronous_direct_scatter_output<
			  char, ::fast_io::basic_obuffer_view_ref<char>>);
static_assert(
	::fast_io::details::decay::print_output_retains_static_scatter<direct_sink>);
static_assert(!::fast_io::details::decay::
			  print_output_retains_static_scatter<scalar_only_sink>);
static_assert(::fast_io::details::decay::
			  print_output_retains_static_scatter<scatter_only_sink>);
static_assert(!::fast_io::details::decay::print_output_retains_static_scatter<
			  unmarked_scatter_adapter>);
static_assert(!::fast_io::details::decay::print_output_retains_static_scatter<
			  ::fast_io::basic_obuffer_view_ref<char>>);
static_assert(::fast_io::synchronous_direct_scatter_output<
			  char, greedy_direct_sink>);
static_assert(::fast_io::details::decay::print_output_retains_static_scatter<
			  greedy_direct_sink>);
static_assert(::fast_io::operations::decay::
				  print_static_provider_mixed_static_component_v<
					  char, greedy_provider_node>);
static_assert(::fast_io::operations::decay::
				  print_static_provider_mixed_dynamic_component_v<
					  char, ::fast_io::basic_io_scatter_t<char>>);
static_assert(::fast_io::operations::decay::
				  print_static_provider_mixed_run_available<
					  false, greedy_direct_sink, greedy_provider_node,
					  greedy_provider_node, greedy_provider_node,
					  ::fast_io::basic_io_scatter_t<char>>());

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
	assert(direct.write_calls == 1u && direct.scatter_calls == 0u);
	assert(std::string_view(direct.bytes.data(), direct.size) == "direct");

	capture_state scalar_state{.expected_source = source};
	auto const scalar{print_static_source(
		scalar_only_sink{__builtin_addressof(scalar_state)}, source)};
	assert(scalar.observed_expected_source);
	assert(scalar.write_calls == 1u && scalar.scatter_calls == 0u);
	assert(std::string_view(scalar.bytes.data(), scalar.size) == "direct");

	capture_state scatter_state{.expected_source = source};
	auto const scatter{print_static_source(
		scatter_only_sink{__builtin_addressof(scatter_state)}, source)};
	assert(scatter.observed_expected_source);
	assert(scatter.write_calls == 0u && scatter.scatter_calls == 1u);
	assert(std::string_view(scatter.bytes.data(), scatter.size) == "direct");

	capture_state adapter_state{.expected_source = source};
	auto const adapted{print_static_source(
		unmarked_scatter_adapter{__builtin_addressof(adapter_state)}, source)};
	// A scatter-shaped adapter without the synchronous direct-output proof must
	// materialize even one fixed fragment into destination-owned scratch.
	assert(!adapted.observed_expected_source);
	assert(adapted.write_calls == 1u && adapted.scatter_calls == 0u);
	assert(std::string_view(adapted.bytes.data(), adapted.size) == "direct");
}

void test_static_argument_provider_gate()
{
	using static_source_type = ::std::remove_cvref_t<
		decltype(::fast_io::mnp::static_arg<42>)>;
	constexpr auto provider_node{
		::fast_io::manipulators::print_compiler_constant_materialize(
			::fast_io::io_reserve_type<char, static_source_type>,
			::fast_io::mnp::static_arg<42>)};
	using provider_node_type = ::std::remove_cvref_t<decltype(provider_node)>;
	constexpr auto provider{
		::fast_io::manipulators::print_compiler_constant_single_static_fragment(
			::fast_io::io_reserve_type<char, provider_node_type>,
			provider_node)};

	capture_state direct_state{.expected_source = provider.base};
	::fast_io::io::print(
		direct_sink{__builtin_addressof(direct_state)},
		::fast_io::mnp::static_arg<42>);
	assert(direct_state.observed_expected_source);
	assert(direct_state.write_calls == 1u && direct_state.scatter_calls == 0u);
	assert(std::string_view(direct_state.bytes.data(), direct_state.size) ==
		   "42");

	capture_state scalar_state{.expected_source = provider.base};
	::fast_io::io::print(
		scalar_only_sink{__builtin_addressof(scalar_state)},
		::fast_io::mnp::static_arg<42>);
	assert(scalar_state.observed_expected_source);
	assert(scalar_state.write_calls == 1u && scalar_state.scatter_calls == 0u);
	assert(std::string_view(scalar_state.bytes.data(), scalar_state.size) ==
		   "42");

	capture_state scatter_state{.expected_source = provider.base};
	::fast_io::io::print(
		scatter_only_sink{__builtin_addressof(scatter_state)},
		::fast_io::mnp::static_arg<42>);
	assert(scatter_state.write_calls == 0u && scatter_state.scatter_calls == 1u);
	assert(std::string_view(scatter_state.bytes.data(), scatter_state.size) ==
		   "42");

	// The compiler-constant facade may legally select an equivalent digit-table
	// slice. Feed its resolved provider node separately to prove that a
	// scatter-only endpoint retains that node's exact immutable address.
	capture_state provider_scatter_state{.expected_source = provider.base};
	::fast_io::io::print(
		scatter_only_sink{__builtin_addressof(provider_scatter_state)},
		provider_node);
	assert(provider_scatter_state.observed_expected_source);
	assert(provider_scatter_state.write_calls == 0u &&
		   provider_scatter_state.scatter_calls == 1u);

	capture_state adapter_state{.expected_source = provider.base};
	::fast_io::io::print(
		unmarked_scatter_adapter{__builtin_addressof(adapter_state)},
		::fast_io::mnp::static_arg<42>);
	assert(!adapter_state.observed_expected_source);
	assert(adapter_state.write_calls == 1u && adapter_state.scatter_calls == 0u);
	assert(std::string_view(adapter_state.bytes.data(), adapter_state.size) ==
		   "42");
}

void test_small_provider_merge_gate()
{
	using left_type = ::std::remove_cvref_t<
		decltype(::fast_io::mnp::static_arg<"hello">)>;
	using right_type = ::std::remove_cvref_t<
		decltype(::fast_io::mnp::static_arg<"world">)>;
	using merged_provider = ::fast_io::operations::decay::
		print_static_provider_merged_run_provider<
			false, char, left_type, right_type>;

	// Multiple language-level static providers form one final IO-owned record.
	// The pointer assertion prevents a regression to an automatic copy which
	// would preserve call count while losing the intended .rodata transport.
	capture_state state{
		.expected_source = merged_provider::storage.data()};
	::fast_io::io::print(
		direct_sink{__builtin_addressof(state)},
		::fast_io::mnp::static_arg<"hello">,
		::fast_io::mnp::static_arg<"world">);
	assert(state.observed_expected_source);
	assert(state.write_calls == 1u && state.scatter_calls == 0u);
	assert(std::string_view(state.bytes.data(), state.size) == "helloworld");
}

void test_provider_merge_cap_fallback()
{
	large_capture_state state{};
	::fast_io::io::print(
		large_direct_sink{__builtin_addressof(state)},
		large_provider_node{}, large_provider_node{});

	// Each 40,000-unit provider is independently legal, but their 80,000-unit
	// concatenation exceeds the shared provider-object cap. The unbuffered IO
	// policy must retain the two immutable slices instead of instantiating an
	// oversized merged COMDAT or copying them to automatic storage.
	assert(state.bytes == 80000u);
	assert(state.write_calls == 0u && state.scatter_calls == 1u);
	assert(state.descriptor_count == 2u);
	assert(state.content_matches);
}

void test_mixed_provider_greedy_cap_split()
{
	using merged_pair_provider = ::fast_io::operations::decay::
		print_static_provider_merged_run_provider<
			false, char, greedy_provider_node, greedy_provider_node>;
	constexpr std::string_view separator{"|"};
	constexpr ::fast_io::basic_io_scatter_t<char> separator_scatter{
		separator.data(), separator.size()};
	greedy_capture_state state{};
	::fast_io::io::print(
		greedy_direct_sink{__builtin_addressof(state)},
		greedy_provider_node{}, greedy_provider_node{},
		greedy_provider_node{}, separator_scatter);

	// The first two nodes fit and become one 44,000-unit provider. Adding the
	// third would exceed 65,536, so IO flushes that pair, preserves the third
	// node's original provider, and finally preserves the borrowed runtime view.
	// These exact addresses reject both an oversized 66,000-unit COMDAT and an
	// automatic copy that happens to retain the same descriptor count.
	assert(state.write_calls == 0u && state.scatter_calls == 1u);
	assert(state.descriptor_count == 3u && state.bytes == 66001u);
	assert(state.sources[0u] == merged_pair_provider::storage.data());
	assert(state.sizes[0u] == merged_pair_provider::size);
	assert(state.sources[1u] ==
		   ::fast_io::manipulators::static_provider_storage_t<
			   greedy_provider_recipe>::storage.data());
	assert(state.sizes[1u] == greedy_provider_recipe::size);
	assert(state.sources[2u] == separator.data() &&
		   state.sizes[2u] == separator.size());
	assert(state.content_matches);
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
	test_small_provider_merge_gate();
	test_provider_merge_cap_fallback();
	test_mixed_provider_greedy_cap_split();
	test_put_area_stays_destination_owned();
}
