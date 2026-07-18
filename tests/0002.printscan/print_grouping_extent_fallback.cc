#include <fast_io_core.h>

#include <cassert>
#include <cstddef>

namespace print_grouping_extent_fallback
{

inline constexpr ::std::size_t one_buffer_limit{
	::fast_io::details::decay::print_contiguous_char_extent_max_chars<char>()};
inline constexpr ::std::size_t oversized_part{one_buffer_limit / 2u + 1u};

struct enormous_scatter
{};

inline constexpr ::fast_io::basic_io_scatter_t<char>
	print_scatter_define(::fast_io::io_reserve_type_t<char, enormous_scatter>, enormous_scatter) noexcept
{
	// The regression exercises aggregate admission only. A native scatter sink below deliberately never dereferences
	// this abstract range, so no giant object has to be materialized in order to prove the fallback.
	return {nullptr, oversized_part};
}

struct extent_limit_scatter
{};

inline constexpr ::fast_io::basic_io_scatter_t<char>
	print_scatter_define(::fast_io::io_reserve_type_t<char, extent_limit_scatter>, extent_limit_scatter) noexcept
{
	return {nullptr, one_buffer_limit};
}

inline constexpr ::std::true_type
	print_borrowed_scatter_source(::fast_io::io_reserve_type_t<char, enormous_scatter>) noexcept
{
	return {};
}

inline constexpr ::std::true_type
	print_borrowed_scatter_source(::fast_io::io_reserve_type_t<char, extent_limit_scatter>) noexcept
{
	return {};
}

struct enormous_dynamic
{};

inline constexpr ::std::size_t
	print_reserve_size(::fast_io::io_reserve_type_t<char, enormous_dynamic>, enormous_dynamic) noexcept
{
	return oversized_part;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, enormous_dynamic>, char *destination, enormous_dynamic) noexcept
{
	return destination;
}

struct native_scatter_sink
{
	using output_char_type = char;
	::std::size_t *scatter_calls;
	::std::size_t *last_count;
};

inline constexpr native_scatter_sink output_stream_ref_define(native_scatter_sink sink) noexcept
{
	return sink;
}

inline void scatter_write_all_overflow_define(native_scatter_sink sink,
											  ::fast_io::basic_io_scatter_t<char> const *, ::std::size_t count) noexcept
{
	++*sink.scatter_calls;
	*sink.last_count = count;
}

inline constexpr ::std::size_t scatter_direct_full_output_coalesce_threshold(
	::fast_io::io_reserve_type_t<char, native_scatter_sink>) noexcept
{
	return SIZE_MAX;
}

struct fallback_policy_sink
{
	using output_char_type = char;
};

inline constexpr ::std::size_t scatter_fallback_full_output_threshold(
	::fast_io::io_reserve_type_t<char, fallback_policy_sink>) noexcept
{
	return SIZE_MAX;
}

struct dynamic_policy_sink
{
	using output_char_type = char;
};

inline constexpr ::std::size_t full_output_dynamic_coalesce_threshold(
	::fast_io::io_reserve_type_t<char, dynamic_policy_sink>) noexcept
{
	return SIZE_MAX;
}

static_assert(one_buffer_limit < static_cast<::std::size_t>(PTRDIFF_MAX));
static_assert(one_buffer_limit <= SIZE_MAX / sizeof(char));
static_assert(::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<char>(
				  oversized_part, oversized_part) == SIZE_MAX);
static_assert(::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<char>(
				  one_buffer_limit, 1u) == SIZE_MAX);
static_assert(::fast_io::details::decay::print_scatter_fallback_full_output_threshold<char, fallback_policy_sink>() ==
			  one_buffer_limit);
static_assert(::fast_io::details::decay::print_full_output_dynamic_coalesce_threshold<char, dynamic_policy_sink>() ==
			  one_buffer_limit);

} // namespace print_grouping_extent_fallback

int main()
{
	using namespace print_grouping_extent_fallback;

	enormous_scatter first;
	enormous_scatter second;
	extent_limit_scatter at_extent_limit;
	assert((::fast_io::details::decay::print_n_scatter_total_size<2u, char>(first, second) == SIZE_MAX));

	// Adding println's retained newline is checked in the same optional-contiguous domain.
	assert((::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<char>(
				::fast_io::details::decay::print_n_scatter_total_size<1u, char>(at_extent_limit), 1u) == SIZE_MAX));

	enormous_dynamic dynamic_first;
	enormous_dynamic dynamic_second;
	assert((::fast_io::details::decay::ndynamic_print_reserve_size<2u, char>(dynamic_first, dynamic_second) ==
			SIZE_MAX));

	::std::size_t scatter_calls{};
	::std::size_t last_count{};
	// The impossible complete extent returns to the original native-scatter batch. println retains the extra descriptor
	// rather than trying to append a newline to an unrepresentable contiguous allocation.
	::fast_io::operations::print_freestanding<true>(
		native_scatter_sink{__builtin_addressof(scatter_calls), __builtin_addressof(last_count)}, first, second);
	assert(scatter_calls == 1u);
	assert(last_count == 3u);
}
