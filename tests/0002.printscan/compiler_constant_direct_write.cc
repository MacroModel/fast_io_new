#include <array>
#include <cassert>
#include <cstddef>
#include <string_view>
#include <type_traits>

#include <fast_io.h>

namespace
{

struct capture_state
{
	std::array<char, 256u> bytes{};
	std::size_t size{};
	std::size_t write_calls{};
	std::size_t scatter_calls{};
};

struct direct_byte_sink
{
	using output_char_type = char;
	capture_state *state{};
};

inline constexpr direct_byte_sink output_stream_ref_define(
	direct_byte_sink sink) noexcept
{
	return sink;
}

inline constexpr std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char, direct_byte_sink>) noexcept
{
	return {};
}

// The sink consumes every scalar range before returning. This independent
// lifetime contract is required by the precise compact path; the scatter
// marker alone cannot prove that a scalar customization is synchronous.
inline constexpr std::true_type print_synchronous_direct_scalar_output(
	::fast_io::io_reserve_type_t<char, direct_byte_sink>) noexcept
{
	return {};
}

inline std::byte const *write_some_bytes_overflow_define(
	direct_byte_sink sink, std::byte const *first,
	std::byte const *last) noexcept
{
	++sink.state->write_calls;
	std::size_t const remaining{static_cast<std::size_t>(last - first)};
	std::size_t const count{remaining < 2u ? remaining : 2u};
	for (std::size_t index{}; index != count; ++index)
	{
		sink.state->bytes[sink.state->size++] =
			static_cast<char>(first[index]);
	}
	return first + count;
}

inline ::fast_io::io_scatter_status_t scatter_write_some_bytes_overflow_define(
	direct_byte_sink sink, ::fast_io::io_scatter_t const *scatters,
	std::size_t count) noexcept
{
	++sink.state->scatter_calls;
	for (std::size_t index{}; index != count; ++index)
	{
		auto const *first{
			static_cast<char const *>(scatters[index].base)};
		for (std::size_t offset{}; offset != scatters[index].len; ++offset)
		{
			sink.state->bytes[sink.state->size++] = first[offset];
		}
	}
	return {count, 0u};
}

static_assert(::fast_io::synchronous_direct_scatter_output<
			  char, direct_byte_sink>);
static_assert(::fast_io::synchronous_direct_scalar_output<
			  char, direct_byte_sink>);
static_assert(::fast_io::details::decay::
				  print_output_retains_static_scatter<direct_byte_sink>);
static_assert(::fast_io::details::decay::
				  print_has_direct_write_operations<direct_byte_sink>);
static_assert(::fast_io::details::decay::
				  print_has_preferred_direct_write_operations<direct_byte_sink>);

struct precise_audit_counts
{
	std::size_t eligible{};
	std::size_t materialize{};
	std::size_t precise_size{};
	std::size_t precise_define{};
	std::size_t ordinary_define{};
	std::size_t fragment_define{};
};

struct precise_audit_source
{
	precise_audit_counts *counts;
	std::size_t size;
};

struct precise_audit_proxy
{
	precise_audit_counts *counts;
	std::size_t size;
};

inline constexpr std::array<char, 80u> precise_audit_payload{[] {
	std::array<char, 80u> value{};
	value.fill('x');
	return value;
}()};

template <::std::integral char_type>
inline constexpr std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type, precise_audit_source>) noexcept
{
	return 80u;
}

template <::std::integral char_type>
inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type, precise_audit_source>,
	char_type *iter, precise_audit_source const &value) noexcept
{
	for (std::size_t index{}; index != value.size; ++index)
	{
		*iter++ = ::fast_io::char_literal_v<u8'x', char_type>;
	}
	return iter;
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr std::true_type
	print_compiler_constant_materialization_query_inline_safe(
		::fast_io::io_reserve_type_t<char_type, precise_audit_source>) noexcept
{
	return {};
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr std::true_type
	print_compiler_constant_pre_normalization_safe(
		::fast_io::io_reserve_type_t<char_type, precise_audit_source>) noexcept
{
	return {};
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr std::true_type
	print_compiler_constant_materialization_graph_proven(
		::fast_io::io_reserve_type_t<char_type, precise_audit_source>) noexcept
{
	// The test provider is explicitly classified so its counters audit the
	// exact-output consumer gate and proxy protocol rather than provider FCO.
	return {};
}

template <::std::integral char_type>
[[nodiscard]] inline bool print_compiler_constant_materialization_eligible(
	::fast_io::io_reserve_type_t<char_type, precise_audit_source>,
	precise_audit_source const &value) noexcept
{
	++value.counts->eligible;
	return true;
}

template <::std::integral char_type>
[[nodiscard]] inline precise_audit_proxy
print_compiler_constant_materialize(
	::fast_io::io_reserve_type_t<char_type, precise_audit_source>,
	precise_audit_source const &value) noexcept
{
	++value.counts->materialize;
	return {value.counts, value.size};
}

template <::std::integral char_type>
inline constexpr std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type, precise_audit_proxy>) noexcept
{
	return 80u;
}

// Deliberately adversarial: the ordinary reserve writer consumes its complete
// bound.  An exact compact destination must never call it after measuring a
// shorter spelling; ASan would diagnose that substitution as an overstore.
template <::std::integral char_type>
inline char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type, precise_audit_proxy>,
	char_type *iter, precise_audit_proxy const &value) noexcept
{
	++value.counts->ordinary_define;
	for (std::size_t index{}; index != 80u; ++index)
	{
		*iter++ = ::fast_io::char_literal_v<u8'!', char_type>;
	}
	return iter;
}

template <::std::integral char_type>
[[nodiscard]] inline std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char_type, precise_audit_proxy>,
	precise_audit_proxy const &value) noexcept
{
	++value.counts->precise_size;
	return value.size;
}

template <::std::integral char_type>
inline char_type *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char_type, precise_audit_proxy>,
	char_type *iter, std::size_t size,
	precise_audit_proxy const &value) noexcept
{
	++value.counts->precise_define;
	for (std::size_t index{}; index != size; ++index)
	{
		*iter++ = ::fast_io::char_literal_v<u8'x', char_type>;
	}
	return iter;
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr std::true_type
	print_compiler_constant_prefer_precise_compact(
		::fast_io::io_reserve_type_t<char_type, precise_audit_proxy>) noexcept
{
	return {};
}

template <::std::integral char_type>
inline constexpr std::size_t print_compiler_constant_static_fragments_size(
	::fast_io::io_reserve_type_t<char_type, precise_audit_proxy>) noexcept
{
	return 2u;
}

template <::std::integral char_type>
inline ::fast_io::basic_io_scatter_t<char_type> *
print_compiler_constant_static_fragments_define(
	::fast_io::io_reserve_type_t<char_type, precise_audit_proxy>,
	::fast_io::basic_io_scatter_t<char_type> *iter,
	precise_audit_proxy const &value) noexcept
{
	static_assert(::std::same_as<char_type, char>);
	++value.counts->fragment_define;
	if (value.size != 0u)
	{
		*iter++ = {precise_audit_payload.data(), value.size};
	}
	return iter;
}

static_assert(::fast_io::compiler_constant_precise_compact_preferred<
			  char, precise_audit_proxy>);
inline constexpr bool precise_source_candidate{
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_candidate_v<
			char, precise_audit_source>};

void test_precise_first_protocol()
{
	precise_audit_counts short_counts{};
	capture_state short_state{};
	::fast_io::print(direct_byte_sink{__builtin_addressof(short_state)},
					 precise_audit_source{__builtin_addressof(short_counts), 3u},
					 precise_audit_source{__builtin_addressof(short_counts), 5u});
	assert(std::string_view(short_state.bytes.data(), short_state.size) ==
		   "xxxxxxxx");
	assert(short_state.scatter_calls == 0u);
	if constexpr (precise_source_candidate)
	{
		assert(short_counts.eligible == 2u && short_counts.materialize == 2u &&
			   short_counts.precise_size == 2u &&
			   short_counts.precise_define == 2u &&
			   short_counts.ordinary_define == 0u &&
			   short_counts.fragment_define == 0u);
	}
	else
	{
		// A fail-closed frontend must retain the historical source writer and
		// never instantiate or execute any replacement-proxy operation.
		assert(short_counts.eligible == 0u && short_counts.materialize == 0u &&
			   short_counts.precise_size == 0u &&
			   short_counts.precise_define == 0u &&
			   short_counts.ordinary_define == 0u &&
			   short_counts.fragment_define == 0u);
	}

	precise_audit_counts long_counts{};
	capture_state long_state{};
	::fast_io::print(direct_byte_sink{__builtin_addressof(long_state)},
					 precise_audit_source{__builtin_addressof(long_counts), 65u});
	assert(long_state.size == 65u &&
		   std::string_view(long_state.bytes.data(), long_state.size) ==
			   std::string_view(precise_audit_payload.data(), 65u));
	if constexpr (precise_source_candidate)
	{
		assert(long_counts.eligible == 1u && long_counts.materialize == 1u &&
			   long_counts.precise_size == 1u &&
			   long_counts.ordinary_define == 0u);
#if defined(__clang__) && 21 <= __clang_major__
		// Clang 21+ uses its proven bounded direct tier after exact measurement.
		// Other frontends retain the immutable provider fragment above 64 bytes.
		assert(long_counts.precise_define == 1u &&
			   long_counts.fragment_define == 0u);
#else
		assert(long_counts.precise_define == 0u &&
			   long_counts.fragment_define == 1u);
#endif
	}
	else
	{
		assert(long_counts.eligible == 0u && long_counts.materialize == 0u &&
			   long_counts.precise_size == 0u &&
			   long_counts.precise_define == 0u &&
			   long_counts.ordinary_define == 0u &&
			   long_counts.fragment_define == 0u);
	}
}

template <std::size_t extent>
void test_compact_tier(char const (&prefix)[extent],
					   std::string_view expected)
{
	static_assert(extent != 0u);
	capture_state state{};
	direct_byte_sink sink{__builtin_addressof(state)};
	::fast_io::manipulators::static_scatter_t<char, extent - 1u> literal{
		prefix};
	double value{3.2};
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_fragment_emit<false>(
			sink, literal, value);
	assert(std::string_view(state.bytes.data(), state.size) == expected);
	assert(state.write_calls == (expected.size() + 1u) / 2u);
	assert(state.scatter_calls == 0u);
}

void test_capacity_ladder_and_partial_write_retry()
{
	test_compact_tier("i=", "i=3.2");
	test_compact_tier("12345678", "123456783.2");
	test_compact_tier("1234567890123456", "12345678901234563.2");
	test_compact_tier(
		"12345678901234567890123456789012",
		"123456789012345678901234567890123.2");
}

} // namespace

int main()
{
	test_capacity_ladder_and_partial_write_retry();
	test_precise_first_protocol();
}
