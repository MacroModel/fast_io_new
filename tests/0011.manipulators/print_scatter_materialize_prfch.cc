#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <fast_io.h>

#if defined(__GNUC__) || defined(__clang__)
#define FAST_IO_PRINT_PRFCH_TEST_ALWAYS_INLINE __attribute__((always_inline)) inline
#define FAST_IO_PRINT_PRFCH_TEST_NOINLINE __attribute__((noinline))
#else
#define FAST_IO_PRINT_PRFCH_TEST_ALWAYS_INLINE inline
#define FAST_IO_PRINT_PRFCH_TEST_NOINLINE
#endif

namespace print_prfch_test
{

inline constexpr ::std::size_t descriptor_count{32u};
inline constexpr ::std::size_t large_payload_size{
	::fast_io::print_scatter_materialize_read_prfch_minimum_payload_bytes};

struct cacheable_scatter
{
	char const *base{};
	::std::size_t len{};
};

inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, cacheable_scatter>, cacheable_scatter const &source) noexcept
{
	return {source.base, source.len};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, cacheable_scatter>) noexcept
{
	// The tests keep every backing allocation alive and unchanged through both the sizing and copy observations.
	return {};
}

inline constexpr ::std::true_type prfch_cacheable_read_provenance_define(
	::fast_io::io_type_t<cacheable_scatter>) noexcept
{
	// Every advertised range below belongs to ordinary vector/array storage, never a device or special mapping.
	return {};
}

struct x86_hybrid_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::x86};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::x86_intel_hybrid};
	inline static constexpr bool data_available{true};
	inline static constexpr bool instruction_available{false};
};

struct x86_core_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::x86};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::x86_intel_core};
	inline static constexpr bool data_available{true};
	inline static constexpr bool instruction_available{false};
};

struct x86_zen_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::x86};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::x86_amd_zen};
	inline static constexpr bool data_available{true};
	inline static constexpr bool instruction_available{false};
};

struct x86_generic_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::x86};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::generic};
	inline static constexpr bool data_available{true};
	inline static constexpr bool instruction_available{false};
};

struct aarch64_server_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::aarch64};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::arm_server};
	inline static constexpr bool data_available{true};
	inline static constexpr bool instruction_available{true};
};

struct apple_aarch64_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::aarch64};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::arm_apple};
	inline static constexpr bool data_available{true};
	inline static constexpr bool instruction_available{true};
};

template <typename platform_type, typename source_type, ::std::size_t... indices>
inline consteval bool repeated_source_strategy(::std::index_sequence<indices...>) noexcept
{
	return ::fast_io::print_scatter_materialize_read_prfch_strategy<
		platform_type, sizeof...(indices), sizeof...(indices),
		::std::conditional_t<(indices < sizeof...(indices)), source_type, source_type>...>;
}

template <::std::size_t... indices>
inline consteval bool alternating_null_strategy(::std::index_sequence<indices...>) noexcept
{
	return ::fast_io::print_scatter_materialize_read_prfch_strategy<
		x86_hybrid_platform, sizeof...(indices), sizeof...(indices) / 2u,
		::std::conditional_t<(indices % 2u == 0u), cacheable_scatter, ::fast_io::io_null_t>...>;
}

template <::std::size_t... indices>
inline consteval bool proved_prefix_with_unmarked_tail_strategy(
	::std::index_sequence<indices...>) noexcept
{
	return ::fast_io::print_scatter_materialize_read_prfch_strategy<
		x86_hybrid_platform, descriptor_count, descriptor_count,
		::std::conditional_t<(indices < descriptor_count), cacheable_scatter,
							 ::fast_io::basic_io_scatter_t<char>>...>;
}

static_assert(::fast_io::print_scatter_materialize_read_prfch_platform<x86_hybrid_platform>);
static_assert(!::fast_io::print_scatter_materialize_read_prfch_platform<x86_core_platform>);
static_assert(!::fast_io::print_scatter_materialize_read_prfch_platform<x86_zen_platform>);
static_assert(!::fast_io::print_scatter_materialize_read_prfch_platform<x86_generic_platform>);
static_assert(!::fast_io::print_scatter_materialize_read_prfch_platform<aarch64_server_platform>);
static_assert(!::fast_io::print_scatter_materialize_read_prfch_platform<apple_aarch64_platform>);
static_assert(repeated_source_strategy<x86_hybrid_platform, cacheable_scatter>(
	::std::make_index_sequence<descriptor_count>{}));
static_assert(!repeated_source_strategy<x86_hybrid_platform, cacheable_scatter>(
	::std::make_index_sequence<descriptor_count - 1u>{}));
static_assert(!repeated_source_strategy<x86_hybrid_platform, ::fast_io::basic_io_scatter_t<char>>(
	::std::make_index_sequence<descriptor_count>{}));
static_assert(alternating_null_strategy(::std::make_index_sequence<descriptor_count * 2u>{}));
static_assert(proved_prefix_with_unmarked_tail_strategy(
	::std::make_index_sequence<descriptor_count + 1u>{}));

static_assert(::fast_io::print_scatter_materialize_read_prfch_level == ::fast_io::prfch_level::L1);
static_assert(::fast_io::print_scatter_materialize_read_prfch_retention ==
			  ::fast_io::prfch_retention::keep);
static_assert(!::fast_io::details::decay::print_scatter_materialize_payload_meets_read_prfch_threshold<char>(
	large_payload_size - 1u));
static_assert(::fast_io::details::decay::print_scatter_materialize_payload_meets_read_prfch_threshold<char>(
	large_payload_size));
static_assert(!::fast_io::details::decay::print_scatter_materialize_payload_meets_read_prfch_threshold<char16_t>(
	large_payload_size / sizeof(char16_t) - 1u));
static_assert(::fast_io::details::decay::print_scatter_materialize_payload_meets_read_prfch_threshold<char16_t>(
	large_payload_size / sizeof(char16_t)));

struct put_area_state
{
	explicit put_area_state(::std::size_t capacity)
		: storage(capacity), current(storage.data())
	{}

	::std::vector<char> storage;
	char *current;
	::std::string overflow;
	::std::size_t cursor_commits{};
};

struct put_area_output
{
	using output_char_type = char;
	put_area_state *state;
};

inline constexpr put_area_output output_stream_ref_define(put_area_output output) noexcept
{
	return output;
}

inline constexpr char *obuffer_begin(put_area_output output) noexcept
{
	return output.state->storage.data();
}

inline constexpr char *obuffer_curr(put_area_output output) noexcept
{
	return output.state->current;
}

inline constexpr char *obuffer_end(put_area_output output) noexcept
{
	return output.state->storage.data() + output.state->storage.size();
}

inline constexpr void obuffer_set_curr(put_area_output output, char *current) noexcept
{
	output.state->current = current;
	++output.state->cursor_commits;
}

inline void write_all_overflow_define(put_area_output output, char const *first, char const *last)
{
	// Every test provisions the exact full-output capacity. Keeping a real overflow operation makes this a complete
	// output-buffer protocol while recording any unexpected fallback rather than hiding it.
	output.state->overflow.append(first, last);
}

template <::std::size_t... indices>
FAST_IO_PRINT_PRFCH_TEST_ALWAYS_INLINE auto measure_sources(
	::std::array<cacheable_scatter, descriptor_count> &sources,
	::std::index_sequence<indices...>) noexcept
{
	return ::fast_io::details::decay::print_n_scatter_total_size_with_read_prfch<
		descriptor_count, char>(sources[indices]...);
}

template <bool runtime_read_prfch, ::std::size_t... indices>
FAST_IO_PRINT_PRFCH_TEST_ALWAYS_INLINE char *copy_sources(
	char *destination, ::std::array<cacheable_scatter, descriptor_count> &sources,
	::std::index_sequence<indices...>) noexcept
{
	return ::fast_io::details::decay::print_n_scatter_materialize_selected<
		true, descriptor_count, char>(destination, runtime_read_prfch, sources[indices]...);
}

template <::std::size_t... indices>
inline void print_sources(put_area_output output,
						  ::std::array<cacheable_scatter, descriptor_count> &sources,
						  ::std::index_sequence<indices...>)
{
	::fast_io::print(output, sources[indices]...);
}

template <::std::size_t... indices>
inline void println_sources_with_null_gaps(
	put_area_output output, ::std::array<cacheable_scatter, descriptor_count> &sources,
	::std::index_sequence<indices...>)
{
	auto arguments{::std::tuple_cat(
		::std::tuple<cacheable_scatter &, ::fast_io::io_null_t const &>{
			sources[indices], ::fast_io::io_null}...)};
	::std::apply([output](auto &...values) { ::fast_io::println(output, values...); }, arguments);
}

} // namespace print_prfch_test

/// Stable code-generation probe for the run-time-false branch of an otherwise enabled strategy.
/// `runtime_read_prfch` is a compile-time false at the wrapper boundary, so this symbol must contain the historical
/// materializer only: no prefetch mnemonic and no next-nonempty search are permitted in its disassembly.
extern "C" FAST_IO_PRINT_PRFCH_TEST_NOINLINE char *fast_io_print_prfch_runtime_disabled_copy(
	char *destination,
	::std::array<print_prfch_test::cacheable_scatter, print_prfch_test::descriptor_count> *sources) noexcept
{
	return print_prfch_test::copy_sources<false>(
		destination, *sources, ::std::make_index_sequence<print_prfch_test::descriptor_count>{});
}

/// Positive code-generation probe: 32 live sources have exactly 31 next-source relationships.
extern "C" FAST_IO_PRINT_PRFCH_TEST_NOINLINE char *fast_io_print_prfch_runtime_enabled_copy(
	char *destination,
	::std::array<print_prfch_test::cacheable_scatter, print_prfch_test::descriptor_count> *sources) noexcept
{
	return print_prfch_test::copy_sources<true>(
		destination, *sources, ::std::make_index_sequence<print_prfch_test::descriptor_count>{});
}

#undef FAST_IO_PRINT_PRFCH_TEST_ALWAYS_INLINE
#undef FAST_IO_PRINT_PRFCH_TEST_NOINLINE

namespace
{

template <::std::size_t extent>
inline void initialize_sources(
	::std::vector<::std::array<char, extent>> &storage,
	::std::array<print_prfch_test::cacheable_scatter, print_prfch_test::descriptor_count> &sources)
{
	for (::std::size_t index{}; index != sources.size(); ++index)
	{
		char const value{static_cast<char>('a' + index % 26u)};
		storage[index].fill(value);
		sources[index] = {storage[index].data(), storage[index].size()};
	}
}

template <::std::size_t extent>
inline void require_output(
	print_prfch_test::put_area_state const &state, bool line)
{
	::std::size_t const expected_size{
		print_prfch_test::descriptor_count * extent + static_cast<::std::size_t>(line)};
	assert(static_cast<::std::size_t>(state.current - state.storage.data()) == expected_size);
	assert(state.overflow.empty());
	assert(state.cursor_commits == 1u);
	for (::std::size_t descriptor{}; descriptor != print_prfch_test::descriptor_count; ++descriptor)
	{
		char const expected{static_cast<char>('a' + descriptor % 26u)};
		for (::std::size_t offset{}; offset != extent; ++offset)
		{
			assert(state.storage[descriptor * extent + offset] == expected);
		}
	}
	if (line)
	{
		assert(state.storage[expected_size - 1u] == '\n');
	}
}

inline void test_small_payload_rejects_prefetch_and_uses_historical_copy()
{
	constexpr ::std::size_t extent{64u};
	::std::vector<::std::array<char, extent>> storage(print_prfch_test::descriptor_count);
	::std::array<print_prfch_test::cacheable_scatter, print_prfch_test::descriptor_count> sources{};
	initialize_sources(storage, sources);

	auto const measurement{print_prfch_test::measure_sources(
		sources, ::std::make_index_sequence<print_prfch_test::descriptor_count>{})};
	assert(measurement.total_size == print_prfch_test::descriptor_count * extent);
	assert(measurement.nonempty_count == print_prfch_test::descriptor_count);
	assert(!measurement.every_nonempty_payload_is_large);

	::std::array<char, print_prfch_test::descriptor_count * extent> direct_destination{};
	char *const direct_end{fast_io_print_prfch_runtime_disabled_copy(
		direct_destination.data(), __builtin_addressof(sources))};
	assert(direct_end == direct_destination.data() + direct_destination.size());
	for (::std::size_t descriptor{}; descriptor != sources.size(); ++descriptor)
	{
		assert(::std::equal(
			storage[descriptor].begin(), storage[descriptor].end(),
			direct_destination.begin() + static_cast<::std::ptrdiff_t>(descriptor * extent)));
	}

	print_prfch_test::put_area_state state(print_prfch_test::descriptor_count * extent);
	print_prfch_test::print_sources(
		{__builtin_addressof(state)}, sources,
		::std::make_index_sequence<print_prfch_test::descriptor_count>{});
	require_output<extent>(state, false);
}

inline void test_large_payload_enables_prefetch_materializer()
{
	constexpr ::std::size_t extent{print_prfch_test::large_payload_size};
	::std::vector<::std::array<char, extent>> storage(print_prfch_test::descriptor_count);
	::std::array<print_prfch_test::cacheable_scatter, print_prfch_test::descriptor_count> sources{};
	initialize_sources(storage, sources);

	auto const measurement{print_prfch_test::measure_sources(
		sources, ::std::make_index_sequence<print_prfch_test::descriptor_count>{})};
	assert(measurement.total_size == print_prfch_test::descriptor_count * extent);
	assert(measurement.nonempty_count == print_prfch_test::descriptor_count);
	assert(measurement.every_nonempty_payload_is_large);

	::std::vector<char> hinted_destination(measurement.total_size);
	char *const hinted_end{fast_io_print_prfch_runtime_enabled_copy(
		hinted_destination.data(), __builtin_addressof(sources))};
	assert(hinted_end == hinted_destination.data() + hinted_destination.size());
	for (::std::size_t descriptor{}; descriptor != sources.size(); ++descriptor)
	{
		assert(::std::equal(
			storage[descriptor].begin(), storage[descriptor].end(),
			hinted_destination.begin() + static_cast<::std::ptrdiff_t>(descriptor * extent)));
	}

	print_prfch_test::put_area_state state(measurement.total_size);
	print_prfch_test::print_sources(
		{__builtin_addressof(state)}, sources,
		::std::make_index_sequence<print_prfch_test::descriptor_count>{});
	require_output<extent>(state, false);
}

inline void test_null_gaps_preserve_one_coalesced_println_commit()
{
	constexpr ::std::size_t extent{print_prfch_test::large_payload_size};
	::std::vector<::std::array<char, extent>> storage(print_prfch_test::descriptor_count);
	::std::array<print_prfch_test::cacheable_scatter, print_prfch_test::descriptor_count> sources{};
	initialize_sources(storage, sources);

	print_prfch_test::put_area_state state(print_prfch_test::descriptor_count * extent + 1u);
	print_prfch_test::println_sources_with_null_gaps(
		{__builtin_addressof(state)}, sources,
		::std::make_index_sequence<print_prfch_test::descriptor_count>{});
	require_output<extent>(state, true);
}

} // namespace

int main()
{
	test_small_payload_rejects_prefetch_and_uses_historical_copy();
	test_large_payload_enables_prefetch_materializer();
	test_null_gaps_preserve_one_coalesced_println_commit();
}
