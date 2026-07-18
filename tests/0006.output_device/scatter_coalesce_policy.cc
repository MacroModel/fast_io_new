#include <cassert>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

#include <fast_io_core.h>

namespace
{

enum class sink_policy
{
	fallback,
	fallback_heap,
	direct,
	repack,
	repack_reject
};

struct sink_state
{
	std::string output;
	std::size_t write_calls{};
	std::size_t scatter_calls{};
};

template <std::size_t size>
struct fixed_scatter
{
	char const *base;
};

template <std::size_t size>
inline constexpr fast_io::basic_io_scatter_t<char>
print_scatter_define(fast_io::io_reserve_type_t<char, fixed_scatter<size>>, fixed_scatter<size> scatter) noexcept
{
	return {scatter.base, size};
}

template <std::size_t size>
inline constexpr std::true_type
print_borrowed_scatter_source(fast_io::io_reserve_type_t<char, fixed_scatter<size>>) noexcept
{
	// The descriptor points into immutable storage owned by the caller, and this CPO performs no mutation. Repeated
	// sizing/materialization observations are therefore identical and every descriptor remains valid through emission.
	return {};
}

template <sink_policy policy>
struct test_sink
{
	using output_char_type = char;
	sink_state *state;
};

template <sink_policy policy>
inline constexpr test_sink<policy> output_stream_ref_define(test_sink<policy> sink) noexcept
{
	return sink;
}

template <sink_policy policy>
inline void write_all_overflow_define(test_sink<policy> sink, char const *first, char const *last)
{
	++sink.state->write_calls;
	sink.state->output.append(first, last);
}

template <sink_policy policy>
	requires(policy != sink_policy::fallback && policy != sink_policy::fallback_heap)
inline void scatter_write_all_overflow_define(test_sink<policy> sink,
											  fast_io::basic_io_scatter_t<char> const *scatters,
											  std::size_t count)
{
	++sink.state->scatter_calls;
	for (std::size_t i{}; i != count; ++i)
	{
		auto const [base, size] = scatters[i];
		sink.state->output.append(base, size);
	}
}

inline constexpr std::size_t
scatter_fallback_full_output_threshold(fast_io::io_reserve_type_t<char, test_sink<sink_policy::fallback>>) noexcept
{
	return 8u;
}

inline constexpr std::size_t
scatter_fallback_full_output_threshold(fast_io::io_reserve_type_t<char,
																  test_sink<sink_policy::fallback_heap>>) noexcept
{
	return fast_io::details::decay::print_stack_buffer_max_size<char>() + 32u;
}

template <sink_policy policy>
	requires(policy != sink_policy::fallback)
inline constexpr std::size_t
scatter_direct_full_output_coalesce_threshold(fast_io::io_reserve_type_t<char, test_sink<policy>>) noexcept
{
	if constexpr (policy == sink_policy::direct)
	{
		return 5u;
	}
	else
	{
		return 0u;
	}
}

template <sink_policy policy>
	requires(policy == sink_policy::repack || policy == sink_policy::repack_reject)
inline constexpr std::size_t
small_scatter_coalesce_threshold(fast_io::io_reserve_type_t<char, test_sink<policy>>) noexcept
{
	return 2u;
}

template <sink_policy policy>
	requires(policy == sink_policy::repack || policy == sink_policy::repack_reject)
inline constexpr std::size_t scatter_repack_chunk_size(
	fast_io::io_reserve_type_t<char, test_sink<policy>>) noexcept
{
	return 8u;
}

template <sink_policy policy>
	requires(policy == sink_policy::repack || policy == sink_policy::repack_reject)
inline constexpr std::size_t scatter_repack_minimum_saved_scatter_count(
	fast_io::io_reserve_type_t<char, test_sink<policy>>) noexcept
{
	if constexpr (policy == sink_policy::repack)
	{
		return 2u;
	}
	else
	{
		return 4u;
	}
}

template <sink_policy policy, typename... Args>
sink_state emit(Args &&...args)
{
	sink_state state;
	fast_io::operations::print_freestanding<false>(test_sink<policy>{&state}, std::forward<Args>(args)...);
	return state;
}

} // namespace

int main()
{
	using fallback_sink = test_sink<sink_policy::fallback>;
	using fallback_heap_sink = test_sink<sink_policy::fallback_heap>;
	using direct_sink = test_sink<sink_policy::direct>;
	using repack_sink = test_sink<sink_policy::repack>;

	static_assert(fast_io::borrowed_scatter_source<char, fixed_scatter<3u>>);
	static_assert(fast_io::scatter_fallback_full_output_threshold_stream<char, fallback_sink>);
	static_assert(fast_io::scatter_fallback_full_output_threshold_stream<char, fallback_heap_sink>);
	static_assert(!fast_io::scatter_direct_full_output_coalesce_threshold_stream<char, fallback_sink>);
	static_assert(!fast_io::full_output_coalesce_threshold_stream<char, fallback_sink>);
	static_assert(fast_io::details::decay::print_full_output_coalesce_threshold<char, fallback_sink>() == 0u);
	static_assert(fast_io::scatter_direct_full_output_coalesce_threshold_stream<char, direct_sink>);
	static_assert(fast_io::small_scatter_coalesce_threshold_stream<char, repack_sink>);
	static_assert(fast_io::scatter_repack_chunk_size_stream<char, repack_sink>);
	static_assert(fast_io::scatter_repack_minimum_saved_scatter_count_stream<char, repack_sink>);

	fixed_scatter<3u> const abc{"abc"};
	fixed_scatter<2u> const de{"de"};
	fixed_scatter<3u> const def{"def"};
	fixed_scatter<6u> const defghi{"defghi"};
	fixed_scatter<2u> const aa{"aa"};
	fixed_scatter<2u> const bb{"bb"};
	fixed_scatter<2u> const cc{"cc"};
	fixed_scatter<2u> const dd{"dd"};

	auto fallback_small{emit<sink_policy::fallback>(abc, de)};
	assert(fallback_small.output == "abcde");
	assert(fallback_small.write_calls == 1u);
	assert(fallback_small.scatter_calls == 0u);

	auto fallback_large{emit<sink_policy::fallback>(abc, defghi)};
	assert(fallback_large.output == "abcdefghi");
	assert(fallback_large.write_calls == 2u);
	assert(fallback_large.scatter_calls == 0u);

	constexpr std::size_t stack_characters{
		fast_io::details::decay::print_stack_buffer_max_size<char>()};
	static_assert(stack_characters != 0u && stack_characters < SIZE_MAX - 32u);
	constexpr std::size_t heap_first_size{stack_characters / 2u + 1u};
	constexpr std::size_t heap_second_size{stack_characters - heap_first_size + 1u};
	std::string heap_first_storage(heap_first_size, 'a');
	std::string heap_second_storage(heap_second_size, 'b');
	fixed_scatter<heap_first_size> const heap_first{heap_first_storage.data()};
	fixed_scatter<heap_second_size> const heap_second{heap_second_storage.data()};
	auto fallback_heap{emit<sink_policy::fallback_heap>(heap_first, heap_second)};
	assert(fallback_heap.output == heap_first_storage + heap_second_storage);
	assert(fallback_heap.write_calls == 1u);
	assert(fallback_heap.scatter_calls == 0u);

	auto direct_small{emit<sink_policy::direct>(abc, de)};
	assert(direct_small.output == "abcde");
	assert(direct_small.write_calls == 1u);
	assert(direct_small.scatter_calls == 0u);

	auto direct_large{emit<sink_policy::direct>(abc, def)};
	assert(direct_large.output == "abcdef");
	assert(direct_large.write_calls == 0u);
	assert(direct_large.scatter_calls == 1u);

	auto repacked{emit<sink_policy::repack>(aa, bb, cc, dd)};
	assert(repacked.output == "aabbccdd");
	assert(repacked.write_calls == 1u);
	assert(repacked.scatter_calls == 0u);

	auto rejected{emit<sink_policy::repack_reject>(aa, bb, cc, dd)};
	assert(rejected.output == "aabbccdd");
	assert(rejected.write_calls == 0u);
	assert(rejected.scatter_calls == 1u);
}
