#include <array>
#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>

#include <fast_io_dsal/string.h>

#if defined(__GNUC__) || defined(__clang__)
#define FAST_IO_CONCAT_PRFCH_TEST_NOINLINE __attribute__((noinline))
#else
#define FAST_IO_CONCAT_PRFCH_TEST_NOINLINE
#endif

/// Stable code-generation probe for the important all-small negative control.
/// The compile-time policy remains enabled, but the sizing pass has rejected the concrete chain. The resulting symbol
/// must retain the baseline copy loop without a prefetch or next-nonempty search.
extern "C" FAST_IO_CONCAT_PRFCH_TEST_NOINLINE char *fast_io_concat_prfch_runtime_disabled_copy(
	char *destination, ::fast_io::basic_io_scatter_t<char> const *first,
	::fast_io::basic_io_scatter_t<char> const *last) noexcept
{
	return ::fast_io::details::decay::copy_scatter_chain_to_buffer<true>(
		destination, first, last, false);
}

#undef FAST_IO_CONCAT_PRFCH_TEST_NOINLINE

namespace
{

inline constexpr ::std::size_t descriptor_count{32u};
inline constexpr ::std::size_t payload_size{
	::fast_io::concat_scatter_chain_read_prfch_minimum_payload_bytes};

struct x86_core_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::x86};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::x86_intel_core};
	inline static constexpr bool data_available{true};
	inline static constexpr bool instruction_available{false};
};

struct x86_hybrid_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::x86};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::x86_intel_hybrid};
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

struct unavailable_x86_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::x86};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::x86_intel_core};
	inline static constexpr bool data_available{false};
	inline static constexpr bool instruction_available{false};
};

struct cacheable_generic_scatter
{
	char const *base{};
	::std::size_t len{};
};

inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, cacheable_generic_scatter>,
	cacheable_generic_scatter const &source) noexcept
{
	return {source.base, source.len};
}

[[maybe_unused]] inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, cacheable_generic_scatter>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::true_type prfch_cacheable_read_provenance_define(
	::fast_io::io_type_t<cacheable_generic_scatter>) noexcept
{
	// Every instance used below points into fixed automatic arrays which outlive the complete concat operation.
	return {};
}

struct unmarked_generic_scatter
{
	char const *base{};
	::std::size_t len{};
};

struct cacheable_reserve_scatters
{
	char const *base{};
	::std::size_t len{};
};

inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, cacheable_reserve_scatters>) noexcept
{
	return {1u, 0u};
}

inline constexpr ::fast_io::basic_reserve_scatters_define_result<char> print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, cacheable_reserve_scatters>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	cacheable_reserve_scatters const &source) noexcept
{
	*scatters = {source.base, source.len};
	return {scatters + 1u, reserve};
}

[[maybe_unused]] inline constexpr ::std::true_type print_borrowed_reserve_scatters_source(
	::fast_io::io_reserve_type_t<char, cacheable_reserve_scatters>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::true_type prfch_cacheable_read_provenance_define(
	::fast_io::io_type_t<cacheable_reserve_scatters>) noexcept
{
	return {};
}

static_assert(!::fast_io::concat_scatter_chain_read_prfch_platform<x86_core_platform>);
static_assert(::fast_io::concat_scatter_chain_read_prfch_platform<x86_hybrid_platform>);
static_assert(!::fast_io::concat_scatter_chain_read_prfch_platform<x86_zen_platform>);
static_assert(!::fast_io::concat_scatter_chain_read_prfch_platform<x86_generic_platform>);
static_assert(!::fast_io::concat_scatter_chain_read_prfch_platform<aarch64_server_platform>);
static_assert(!::fast_io::concat_scatter_chain_read_prfch_platform<unavailable_x86_platform>);
static_assert(!::fast_io::concat_scatter_chain_write_prfch_platform<x86_core_platform>);

static_assert(::fast_io::concat_scatter_chain_read_prfch_strategy<
			  x86_hybrid_platform, descriptor_count, cacheable_generic_scatter>);
static_assert(::fast_io::concat_scatter_chain_read_prfch_strategy<
			  x86_hybrid_platform, descriptor_count, cacheable_generic_scatter, ::fast_io::io_null_t>);
static_assert(!::fast_io::concat_scatter_chain_read_prfch_strategy<
			  x86_hybrid_platform, descriptor_count - 1u, cacheable_generic_scatter>);
static_assert(!::fast_io::concat_scatter_chain_read_prfch_strategy<
			  x86_hybrid_platform, descriptor_count, cacheable_generic_scatter, unmarked_generic_scatter>);
static_assert(!::fast_io::concat_scatter_chain_read_prfch_strategy<
			  x86_hybrid_platform, descriptor_count, ::fast_io::basic_io_scatter_t<char>>);
static_assert(::fast_io::concat_scatter_chain_read_prfch_strategy<
			  x86_hybrid_platform, descriptor_count, ::fast_io::basic_prfch_cacheable_io_scatter_t<char>>);
static_assert(!::fast_io::concat_scatter_chain_write_prfch_strategy<
			  x86_core_platform, descriptor_count, cacheable_generic_scatter>);

static_assert(::fast_io::concat_scatter_chain_read_prfch_level == ::fast_io::prfch_level::L1);
static_assert(::fast_io::concat_scatter_chain_read_prfch_retention == ::fast_io::prfch_retention::keep);
static_assert(!::fast_io::details::decay::concat_scatter_payload_meets_read_prfch_threshold<char>(
	payload_size - 1u));
static_assert(::fast_io::details::decay::concat_scatter_payload_meets_read_prfch_threshold<char>(
	payload_size));
static_assert(!::fast_io::details::decay::concat_scatter_payload_meets_read_prfch_threshold<char16_t>(
	payload_size / sizeof(char16_t) - 1u));
static_assert(::fast_io::details::decay::concat_scatter_payload_meets_read_prfch_threshold<char16_t>(
	payload_size / sizeof(char16_t)));

inline constexpr bool constexpr_copy_keeps_the_single_traversal_semantics()
{
	::std::array<char, descriptor_count> source{};
	::std::array<char, descriptor_count> destination{};
	::std::array<::fast_io::basic_io_scatter_t<char>, descriptor_count> scatters{};
	for (::std::size_t index{}; index != descriptor_count; ++index)
	{
		source[index] = static_cast<char>('A' + index % 26u);
		scatters[index] = {source.data() + index, 1u};
	}
	char *const end{::fast_io::details::decay::copy_scatter_chain_to_buffer<true>(
		destination.data(), scatters.data(), scatters.data() + scatters.size(), true)};
	return end == destination.data() + destination.size() && source == destination;
}

static_assert(constexpr_copy_keeps_the_single_traversal_semantics());

template <typename source_type, ::std::size_t... indices>
inline ::fast_io::string concat_sources(
	::std::array<source_type, descriptor_count> &sources,
	::std::index_sequence<indices...>)
{
	return ::fast_io::concat_fast_io(sources[indices]...);
}

template <typename source_type, ::std::size_t extent>
inline void verify_materialized_concat()
{
	::std::array<::std::array<char, extent>, descriptor_count> storage{};
	::std::array<source_type, descriptor_count> sources{};
	for (::std::size_t descriptor{}; descriptor != descriptor_count; ++descriptor)
	{
		char const value{static_cast<char>('a' + descriptor % 26u)};
		storage[descriptor].fill(value);
		sources[descriptor] = {storage[descriptor].data(), storage[descriptor].size()};
	}
	auto result{concat_sources(sources, ::std::make_index_sequence<descriptor_count>{})};
	assert(result.size() == descriptor_count * extent);
	for (::std::size_t descriptor{}; descriptor != descriptor_count; ++descriptor)
	{
		char const expected{static_cast<char>('a' + descriptor % 26u)};
		for (::std::size_t offset{}; offset != extent; ++offset)
		{
			assert(result[descriptor * extent + offset] == expected);
		}
	}
}

inline void test_runtime_eligibility_is_folded_into_sizing()
{
	::std::array<char, descriptor_count * 64u> small_source{};
	::std::array<::fast_io::basic_io_scatter_t<char>, descriptor_count> small_scatters{};
	for (::std::size_t index{}; index != descriptor_count; ++index)
	{
		small_scatters[index] = {small_source.data() + index * 64u, 64u};
	}
	bool small_eligible{true};
	::std::size_t const small_total{::fast_io::details::decay::calculate_scatter_total_size<true>(
		small_scatters.data(), small_scatters.data() + small_scatters.size(), small_eligible)};
	assert(small_total == small_source.size());
	assert(!small_eligible);
	::std::array<char, descriptor_count * 64u> small_destination{};
	char *const small_end{fast_io_concat_prfch_runtime_disabled_copy(
		small_destination.data(), small_scatters.data(), small_scatters.data() + small_scatters.size())};
	assert(small_end == small_destination.data() + small_destination.size());
	assert(small_source == small_destination);

	::std::array<char, descriptor_count * payload_size> large_source{};
	::std::array<::fast_io::basic_io_scatter_t<char>, descriptor_count> large_scatters{};
	for (::std::size_t index{}; index != descriptor_count; ++index)
	{
		large_scatters[index] = {large_source.data() + index * payload_size, payload_size};
	}
	bool large_eligible{};
	::std::size_t const large_total{::fast_io::details::decay::calculate_scatter_total_size<true>(
		large_scatters.data(), large_scatters.data() + large_scatters.size(), large_eligible)};
	assert(large_total == large_source.size());
	assert(large_eligible);

	// Capacity is still 32, but the actual prefix has only 31 live payloads and must close at run time.
	large_scatters[descriptor_count - 1u] = {nullptr, 0u};
	bool sparse_eligible{true};
	(void)::fast_io::details::decay::calculate_scatter_total_size<true>(
		large_scatters.data(), large_scatters.data() + large_scatters.size(), sparse_eligible);
	assert(!sparse_eligible);
}

inline void test_empty_descriptors_never_supply_a_hint_address()
{
	::std::array<char, payload_size * descriptor_count> source{};
	for (::std::size_t index{}; index != source.size(); ++index)
	{
		source[index] = static_cast<char>('A' + index % 23u);
	}
	::std::array<::fast_io::basic_io_scatter_t<char>, descriptor_count + 2u> scatters{};
	for (::std::size_t live_index{}; live_index != descriptor_count; ++live_index)
	{
		// Leave slots zero and seventeen value-initialized. Their null bases must not be evaluated by either pass.
		::std::size_t const scatter_index{live_index + 1u + static_cast<::std::size_t>(16u <= live_index)};
		scatters[scatter_index] = {source.data() + live_index * payload_size, payload_size};
	}
	bool runtime_read_prfch{};
	::std::size_t const total{::fast_io::details::decay::calculate_scatter_total_size<true>(
		scatters.data(), scatters.data() + scatters.size(), runtime_read_prfch)};
	assert(total == source.size());
	assert(runtime_read_prfch);
	::std::array<char, payload_size * descriptor_count> destination{};
	char *const end{::fast_io::details::decay::copy_scatter_chain_to_buffer<true>(
		destination.data(), scatters.data(), scatters.data() + scatters.size(), runtime_read_prfch)};
	assert(end == destination.data() + destination.size());
	assert(source == destination);
}

} // namespace

int main()
{
	test_runtime_eligibility_is_folded_into_sizing();
	test_empty_descriptors_never_supply_a_hint_address();
	verify_materialized_concat<cacheable_generic_scatter, 64u>();
	verify_materialized_concat<cacheable_reserve_scatters, 64u>();
	verify_materialized_concat<cacheable_generic_scatter, payload_size>();
	verify_materialized_concat<cacheable_reserve_scatters, payload_size>();
}
