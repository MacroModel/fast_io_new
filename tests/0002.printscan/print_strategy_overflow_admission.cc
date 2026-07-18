#include <fast_io.h>

#include <cassert>
#include <cstddef>

namespace print_strategy_overflow_test
{

inline constexpr ::std::size_t large_extent{
	static_cast<::std::size_t>(PTRDIFF_MAX) / 2u};

struct large_exact
{};

inline constexpr ::std::size_t
print_reserve_size(::fast_io::io_reserve_type_t<char, large_exact>) noexcept
{
	return large_extent;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, large_exact>, char *destination, large_exact) noexcept
{
	// This loop specifies a semantically complete producer for any caller able to provide the advertised abstract
	// extent. The test never materializes that extent; it exercises only compile-time strategy admission.
	for (::std::size_t index{}; index != large_extent; ++index)
	{
		destination[index] = 'x';
	}
	return destination + large_extent;
}

inline constexpr ::std::size_t
print_reserve_static_precise_size(::fast_io::io_reserve_type_t<char, large_exact>) noexcept
{
	return large_extent;
}

struct large_dynamic_hint
{};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, large_dynamic_hint>, large_dynamic_hint) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, large_dynamic_hint>, char *destination,
	large_dynamic_hint) noexcept
{
	*destination = 'd';
	return destination + 1;
}

inline constexpr ::std::size_t
print_reserve_static_stack_size(::fast_io::io_reserve_type_t<char, large_dynamic_hint>) noexcept
{
	// The producer may expose an arbitrarily large preference; it remains only a cost hint, not a stack allocation.
	return SIZE_MAX - 8u;
}

struct context_anchor
{};

struct context_anchor_state
{
	inline constexpr ::fast_io::context_print_result<char *> print_context_define(
		context_anchor, char *first, char *) noexcept
	{
		return {first, true};
	}
};

inline constexpr ::fast_io::io_type_t<context_anchor_state>
print_context_type(::fast_io::io_reserve_type_t<char, context_anchor>) noexcept
{
	return {};
}

inline constexpr ::std::size_t
print_context_static_buffer_size(::fast_io::io_reserve_type_t<char, context_anchor>) noexcept
{
	return 1u;
}

struct repack_sink
{
	using output_char_type = char;
	bool *called;
};

inline constexpr repack_sink output_stream_ref_define(repack_sink sink) noexcept
{
	return sink;
}

inline void scatter_write_all_overflow_define(
	repack_sink sink, ::fast_io::basic_io_scatter_t<char> const *, ::std::size_t) noexcept
{
	*sink.called = true;
}

inline constexpr ::std::size_t
small_scatter_coalesce_threshold(::fast_io::io_reserve_type_t<char, repack_sink>) noexcept
{
	return SIZE_MAX;
}

inline constexpr ::std::size_t
scatter_repack_chunk_size(::fast_io::io_reserve_type_t<char, repack_sink>) noexcept
{
	return 4u;
}

inline constexpr ::std::size_t
scatter_repack_minimum_saved_scatter_count(::fast_io::io_reserve_type_t<char, repack_sink>) noexcept
{
	return 1u;
}

using large_exact_pack =
	::fast_io::manipulators::pack_t<large_exact, large_exact, large_exact>;
using precise_pack_strategy =
	::fast_io::details::decay::print_semantic_static_precise_size<char, large_exact_pack>;
using bounded_pack_strategy =
	::fast_io::details::decay::print_semantic_static_bounded_size<char, large_exact_pack>;

static_assert(::fast_io::reserve_printable<char, large_exact>);
static_assert(::fast_io::static_precise_reserve_printable<char, large_exact>);
static_assert(::fast_io::dynamic_reserve_with_possible_static_stack_size<char, large_dynamic_hint>);
static_assert(::fast_io::context_printable_with_static_buffer_size<char, context_anchor>);
static_assert(::fast_io::details::decay::print_has_direct_scatter_write_operations<repack_sink>);
static_assert(::fast_io::details::decay::print_has_direct_scatter_write_bytes_operations<repack_sink>);
static_assert(::fast_io::small_scatter_coalesce_threshold_stream<char, repack_sink>);
static_assert(::fast_io::scatter_repack_chunk_size_stream<char, repack_sink>);

static_assert(::fast_io::details::decay::print_strategy_saturating_add(SIZE_MAX - 1u, 2u) == SIZE_MAX);
static_assert(::fast_io::details::decay::print_strategy_extent_add_or_unavailable(
	static_cast<::std::size_t>(PTRDIFF_MAX) - 1u, 1u) == SIZE_MAX);
static_assert(::fast_io::details::decay::print_strategy_add_capped(SIZE_MAX - 4u, 16u, 4096u) == 4096u);

// A nested semantic pack whose children are independently valid is not one representable contiguous allocation.
// Both traits must mark the optional aggregate strategy unavailable instead of evaluating a terminating consteval add.
static_assert(!precise_pack_strategy::available);
static_assert(precise_pack_strategy::size == SIZE_MAX);
static_assert(!bounded_pack_strategy::available);
static_assert(bounded_pack_strategy::size == SIZE_MAX);
static_assert(::fast_io::operations::decay::print_semantic_static_precise_total_size<
	false, char, large_exact, large_exact, large_exact>() == SIZE_MAX);
static_assert(::fast_io::operations::decay::print_semantic_static_bounded_total_size<
	false, char, large_exact, large_exact, large_exact>() == SIZE_MAX);
inline constexpr large_exact exact_value{};
static_assert(::fast_io::operations::decay::print_semantic_precise_total_size<false, char>(
	exact_value, exact_value, exact_value) == SIZE_MAX);
static_assert(::fast_io::operations::decay::print_semantic_bounded_total_size<false, char>(
	exact_value, exact_value, exact_value) == SIZE_MAX);

inline constexpr auto scatter_run{
	::fast_io::details::decay::find_continuous_scatters_n<
		char, large_exact, large_exact, large_exact>()};
inline constexpr auto reserve_run{
	::fast_io::details::decay::find_continuous_scatters_reserve_n<
		false, char, large_exact, large_exact, large_exact>()};
inline constexpr auto context_run{
	::fast_io::details::decay::find_context_capture_run_n<
		char, large_exact, large_exact, large_exact, context_anchor>()};

static_assert(scatter_run.position == 0u);
static_assert(reserve_run.position == 0u);
static_assert(context_run.position == 0u && !context_run.has_context);

inline constexpr ::std::size_t stack_capacity{
	::fast_io::details::decay::print_stack_buffer_max_size<char>()};
static_assert(::fast_io::details::decay::ndynamic_print_reserve_static_stack_size<
	3u, char, large_dynamic_hint, large_dynamic_hint, large_dynamic_hint>() == stack_capacity);
static_assert(::fast_io::details::decay::dynamic_print_reserve_static_stack_size<
	true, char, large_dynamic_hint>() == stack_capacity);

} // namespace print_strategy_overflow_test

int main()
{
	using namespace print_strategy_overflow_test;
	static constexpr char text[]{"abcdefgh"};
	::fast_io::basic_io_scatter_t<char> scatters[]{
		{text, 8u},
		{text, 8u}};
	::fast_io::io_scatter_t byte_scatters[]{
		{text, 8u},
		{text, 8u}};
	bool called{};
	repack_sink sink{__builtin_addressof(called)};
	// Eligibility is intentionally larger than the four-character chunk. The strategy must classify these as native
	// scatters rather than subtracting an eight-character candidate from a four-character remaining capacity.
	bool const repacked{
		::fast_io::details::decay::print_scatter_write_all_try_repack_small(
			sink, scatters, 2u)};
	bool const bytes_repacked{
		::fast_io::details::decay::print_scatter_write_all_bytes_try_repack_small(
			sink, byte_scatters, 2u)};
	assert(!repacked);
	assert(!bytes_repacked);
	assert(!called);
}
