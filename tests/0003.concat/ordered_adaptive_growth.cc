#include <cstddef>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <utility>

#define FAST_IO_DISABLE_FLOATING_POINT
#include <fast_io_core.h>

namespace ordered_adaptive_growth
{

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

struct reserve_failure
{};

struct reserve_log
{
	::std::size_t requests[32u]{};
	::std::size_t published_prefixes[32u]{};
	::std::size_t attempts{};
	::std::size_t allocations{};
	::std::size_t deallocations{};
	::std::size_t live_owners{};
	::std::size_t destructions{};
	bool fail_next{};
};

inline reserve_log *active_log{};

/*
 * The exact-capacity provider deliberately has no geometric reserve policy.
 * This separates the adaptive adapter's growth guarantee from std::string
 * implementation heuristics and makes the regression portable to every target.
 * The only observable reserve hook runs after the caller publishes its prefix.
 */
struct counting_destination
{
	char *begin{};
	char *current{};
	char *end{};
	reserve_log *log{};

	counting_destination() noexcept : log(active_log)
	{
		require(log != nullptr);
		++log->live_owners;
	}

	counting_destination(counting_destination const &) = delete;
	counting_destination &operator=(counting_destination const &) = delete;
	counting_destination &operator=(counting_destination &&) = delete;

	counting_destination(counting_destination &&other) noexcept
		: begin(::std::exchange(other.begin, nullptr)),
		  current(::std::exchange(other.current, nullptr)),
		  end(::std::exchange(other.end, nullptr)), log(other.log)
	{
		++log->live_owners;
	}

	~counting_destination()
	{
		if (begin != nullptr)
		{
			delete[] begin;
			++log->deallocations;
		}
		--log->live_owners;
		++log->destructions;
	}
};

using destination_tag = ::fast_io::io_strlike_type_t<char, counting_destination>;

inline char *strlike_begin(destination_tag, counting_destination &value) noexcept
{
	return value.begin;
}

inline char *strlike_curr(destination_tag, counting_destination &value) noexcept
{
	return value.current;
}

inline char *strlike_end(destination_tag, counting_destination &value) noexcept
{
	return value.end;
}

inline void strlike_set_curr(destination_tag, counting_destination &value,
	char *current) noexcept
{
	value.current = current;
	if (current != nullptr)
	{
		*current = '\0';
	}
}

inline void strlike_reserve(destination_tag, counting_destination &value,
	::std::size_t requested)
{
	::std::size_t const capacity{value.begin == nullptr ? 0u :
		static_cast<::std::size_t>(value.end - value.begin)};
	if (requested <= capacity)
	{
		return;
	}
	::std::size_t const used{value.begin == nullptr ? 0u :
		static_cast<::std::size_t>(value.current - value.begin)};
	reserve_log &log{*value.log};
	require(log.attempts < 32u);
	log.requests[log.attempts] = requested;
	log.published_prefixes[log.attempts] = used;
	++log.attempts;
	if (::std::exchange(log.fail_next, false))
	{
		// Failure precedes allocation and leaves all previously published storage
		// intact. The adapter must not retain a cursor into an abandoned array.
		throw reserve_failure{};
	}
	require(requested < ::std::numeric_limits<::std::size_t>::max());
	char *replacement{new char[requested + 1u]};
	for (::std::size_t index{}; index != used; ++index)
	{
		replacement[index] = value.begin[index];
	}
	replacement[used] = '\0';
	if (value.begin != nullptr)
	{
		delete[] value.begin;
		++log.deallocations;
	}
	++log.allocations;
	value.begin = replacement;
	value.current = replacement + used;
	value.end = replacement + requested;
}

inline constexpr ::std::true_type strlike_deferred_obuffer_commit_safe(
	destination_tag) noexcept
{
	// Raw suffix writes cannot relocate the allocation. Destruction reads only
	// its owning pointer, including when the last terminator is unpublished.
	return {};
}

inline constexpr ::std::true_type concat_ordered_staging_adaptive_promotion_safe(
	destination_tag) noexcept
{
	// Default storage is independent, reserve preserves exactly the published
	// prefix, and the test accepts allocation failure at the direct-output edge.
	return {};
}

static_assert(::fast_io::details::decay::
				  basic_general_concat_ordered_staging_adaptive_destination<
					  char, counting_destination>);

using adaptive_buffer = ::fast_io::details::decay::
	basic_concat_ordered_adaptive_buffer<char, counting_destination>;

template <::std::integral char_type>
inline constexpr bool growth_capacity_boundaries() noexcept
{
	constexpr ::std::size_t contiguous_limit{
		::fast_io::details::decay::print_contiguous_char_extent_max_chars<char_type>()};
	constexpr ::std::size_t terminated_byte_limit{SIZE_MAX / sizeof(char_type) - 1u};
	constexpr ::std::size_t limit{contiguous_limit < terminated_byte_limit ?
		contiguous_limit : terminated_byte_limit};
	constexpr ::std::size_t half{limit / 2u};
	constexpr auto grow{
		::fast_io::details::decay::basic_concat_ordered_adaptive_growth_capacity<char_type>};
	// These are arithmetic-only proofs: no near-limit allocation is attempted.
	// Both pointer distance and storage for the terminal code unit participate.
	return grow(0u, 1u) == 1u && grow(4096u, 0u) == 0u &&
		grow(4096u, 4096u) == 4096u && grow(4096u, 4097u) == 8192u &&
		grow(4096u, 9000u) == 9000u && grow(half, half + 1u) == half * 2u &&
		grow(half + 1u, half + 2u) == half + 2u &&
		grow(limit - 1u, limit) == limit && grow(limit, limit) == limit;
}

static_assert(growth_capacity_boundaries<char>());
static_assert(growth_capacity_boundaries<char16_t>());
static_assert(growth_capacity_boundaries<char32_t>());

inline char expected_character(::std::size_t index) noexcept
{
	return static_cast<char>('A' + index % 23u);
}

template <bool amortize_growth = true>
inline void append_fragment(adaptive_buffer &buffer, ::std::size_t count)
{
	::std::size_t const used{static_cast<::std::size_t>(
		buffer.buffer_current - buffer.buffer_begin)};
	::fast_io::details::decay::basic_concat_ordered_adaptive_ensure<amortize_growth>(buffer, count);
	for (::std::size_t index{}; index != count; ++index)
	{
		buffer.buffer_current[index] = expected_character(used + index);
	}
	buffer.buffer_current += count;
}

template <::std::size_t bound>
struct terminal_reserve_leaf
{
	::std::size_t prefix_size;
	::std::size_t actual_size;
	unsigned *writer_calls;
};

// Selecting a pack's final type is not a formatting operation. In particular,
// it must not require a user-defined comma expression to be well-formed: only
// reserve/scatter protocol expressions belong to the ordered-leaf contract.
template <::std::size_t left_bound, ::std::size_t right_bound>
void operator,(terminal_reserve_leaf<left_bound> &,
	terminal_reserve_leaf<right_bound> &) = delete;

template <::std::size_t bound>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, terminal_reserve_leaf<bound>>) noexcept
{
	return bound;
}

template <::std::size_t bound>
inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, terminal_reserve_leaf<bound>>,
	char *destination, terminal_reserve_leaf<bound> &leaf) noexcept
{
	// The reserve bound is intentionally independent from the committed length.
	// Growth must honor the former without guessing the latter or replaying us.
	require(leaf.actual_size <= bound);
	++*leaf.writer_calls;
	for (::std::size_t index{}; index != leaf.actual_size; ++index)
	{
		destination[index] = expected_character(leaf.prefix_size + index);
	}
	return destination + leaf.actual_size;
}

static_assert(::fast_io::details::decay::basic_general_concat_ordered_one_shot_leaf_v<
	char, terminal_reserve_leaf<1u>>);

inline void verify_prefix(adaptive_buffer const &buffer, ::std::size_t expected_size)
{
	require(static_cast<::std::size_t>(buffer.buffer_current - buffer.buffer_begin) ==
		expected_size);
	for (::std::size_t index{}; index != expected_size; ++index)
	{
		require(buffer.buffer_begin[index] == expected_character(index));
	}
}

inline void verify_geometric_growth()
{
	reserve_log log{};
	active_log = __builtin_addressof(log);
	{
		adaptive_buffer buffer;
		append_fragment(buffer, 512u);
		::fast_io::details::decay::basic_concat_ordered_adaptive_promote_between_pairs(buffer);
		require(buffer.result_pointer != nullptr);
		require(log.attempts == 1u && log.requests[0u] == 4096u);
		append_fragment(buffer, 3584u);
		append_fragment(buffer, 1u);
		for (unsigned fragment{}; fragment != 8u; ++fragment)
		{
			append_fragment(buffer, 512u);
		}
		// Exact reserve providers expose linear per-fragment growth immediately:
		// only 4096 -> 8192 -> 16384 is valid for this bounded append sequence.
		require(log.attempts == 3u);
		require(log.requests[1u] == 8192u && log.requests[2u] == 16384u);
		require(log.published_prefixes[0u] == 0u);
		require(log.published_prefixes[1u] == 4096u);
		require(log.published_prefixes[2u] == 7681u);
		verify_prefix(buffer, 8193u);
		strlike_set_curr(destination_tag{}, *buffer.result_pointer, buffer.buffer_current);
		require(buffer.result_pointer->current == buffer.buffer_current);
		require(*buffer.result_pointer->current == '\0');
	}
	require(log.live_owners == 0u && log.destructions == 1u);
	require(log.allocations == log.deallocations);
	active_log = nullptr;
}

inline void verify_terminal_exact_growth(::std::size_t terminal_size)
{
	reserve_log log{};
	active_log = __builtin_addressof(log);
	{
		adaptive_buffer buffer;
		append_fragment(buffer, 512u);
		::fast_io::details::decay::basic_concat_ordered_adaptive_promote_between_pairs(buffer);
		append_fragment(buffer, 3584u);
		// With no later source, even a one-byte overflow cannot amortize doubling
		// the result. Preserve the exact terminal request at this separate edge.
		append_fragment<false>(buffer, terminal_size);
		require(log.attempts == 2u);
		require(log.requests[0u] == 4096u);
		require(log.requests[1u] == 4096u + terminal_size);
		require(log.published_prefixes[1u] == 4096u);
		verify_prefix(buffer, 4096u + terminal_size);
	}
	require(log.live_owners == 0u && log.destructions == 1u);
	require(log.allocations == log.deallocations);
	active_log = nullptr;
}

template <::std::size_t bound, ::std::size_t actual_size>
inline void verify_terminal_one_shot_line(::std::size_t prefix_size)
{
	static_assert(actual_size <= bound);
	reserve_log log{};
	active_log = __builtin_addressof(log);
	{
		adaptive_buffer buffer;
		append_fragment(buffer, 512u);
		::fast_io::details::decay::basic_concat_ordered_adaptive_promote_between_pairs(buffer);
		append_fragment(buffer, prefix_size - 512u);
		::fast_io::io_strlike_reference_wrapper<char, adaptive_buffer> destination{
			__builtin_addressof(buffer)};
		unsigned writer_calls{};
		terminal_reserve_leaf<bound> leaf{
			prefix_size, actual_size, __builtin_addressof(writer_calls)};
		::fast_io::details::decay::basic_general_concat_ordered_emit_one<false>(
			destination, leaf);
		::std::size_t const leaf_request{prefix_size + bound};
		bool const leaf_allocates{leaf_request > 4096u};
		::std::size_t const leaf_capacity{leaf_allocates ? leaf_request : 4096u};
		::std::size_t const leaf_attempts{1u + static_cast<::std::size_t>(leaf_allocates)};
		require(writer_calls == 1u && log.attempts == leaf_attempts);
		if (leaf_allocates)
		{
			require(log.requests[1u] == leaf_request);
			require(log.published_prefixes[1u] == prefix_size);
		}
		verify_prefix(buffer, prefix_size + actual_size);
		// Newline allocation belongs after the source writer. It may use genuine
		// unused bound capacity, but must not reserve ahead of that source's call.
		::fast_io::details::decay::basic_general_concat_ordered_emit_line<char>(destination);
		::std::size_t const result_size{prefix_size + actual_size + 1u};
		bool const line_allocates{result_size > leaf_capacity};
		require(log.attempts == leaf_attempts + static_cast<::std::size_t>(line_allocates));
		require(writer_calls == 1u);
		if (line_allocates)
		{
			// The newline owns an ordinary overflow transition. Its default range
			// fallback doubles capacity; concat must not bypass that CPO by forcing
			// an exact reserve, even though no subsequent source needs the slack.
			require(log.requests[leaf_attempts] == leaf_capacity * 2u);
			require(log.published_prefixes[leaf_attempts] == prefix_size + actual_size);
		}
		require(static_cast<::std::size_t>(buffer.buffer_current - buffer.buffer_begin) ==
			result_size);
		require(buffer.buffer_begin[prefix_size + actual_size] == '\n');
		for (::std::size_t index{}; index != prefix_size + actual_size; ++index)
		{
			require(buffer.buffer_begin[index] == expected_character(index));
		}
		strlike_set_curr(destination_tag{}, *buffer.result_pointer, buffer.buffer_current);
		require(*buffer.result_pointer->current == '\0');
	}
	require(log.live_owners == 0u && log.destructions == 1u);
	require(log.allocations == log.deallocations);
	active_log = nullptr;
}

template <bool line, ::std::size_t... indices>
inline void verify_homogeneous_terminal_peel(::std::index_sequence<indices...>)
{
	constexpr ::std::size_t count{sizeof...(indices)};
	constexpr ::std::size_t prefix_size{4088u};
	constexpr ::std::size_t payload_size{prefix_size + count};
	constexpr ::std::size_t result_size{payload_size + static_cast<::std::size_t>(line)};
	reserve_log log{};
	active_log = __builtin_addressof(log);
	{
		adaptive_buffer buffer;
		append_fragment(buffer, 512u);
		::fast_io::details::decay::basic_concat_ordered_adaptive_promote_between_pairs(buffer);
		append_fragment(buffer, prefix_size - 512u);
		::fast_io::io_strlike_reference_wrapper<char, adaptive_buffer> destination{
			__builtin_addressof(buffer)};
		unsigned writer_calls{};
		::std::size_t checkpoints{};
		terminal_reserve_leaf<1u> leaves[count]{
			{prefix_size + indices, 1u, __builtin_addressof(writer_calls)}...};
		auto after_pair{[&] {
			++checkpoints;
			// Each checkpoint follows exactly two completed source calls and
			// precedes the next source; the terminal pair has no checkpoint.
			require(writer_calls == checkpoints * 2u);
			::fast_io::details::decay::basic_concat_ordered_adaptive_promote_between_pairs(buffer);
		}};
		::fast_io::details::decay::basic_general_concat_ordered_emit<line, char>(
			destination, after_pair, leaves[indices]...);
		// N=8 leaves an ordinary penultimate source after the pair loop; N=9
		// leaves only the terminal source. Both must preserve byte order and the
		// same floor((N-1)/2) checkpoint schedule, with one optional final newline.
		require(writer_calls == count && checkpoints == (count - 1u) / 2u);
		require(static_cast<::std::size_t>(buffer.buffer_current - buffer.buffer_begin) == result_size);
		for (::std::size_t index{}; index != payload_size; ++index)
		{
			require(buffer.buffer_begin[index] == expected_character(index));
		}
		if constexpr (line)
		{
			require(buffer.buffer_begin[payload_size] == '\n');
		}
		constexpr ::std::size_t leaf_attempts{1u + static_cast<::std::size_t>(payload_size > 4096u)};
		if constexpr (payload_size > 4096u)
		{
			require(log.requests[1u] == payload_size);
		}
		if constexpr (line && result_size > 4096u)
		{
			// A terminal source may allocate exactly, but the following newline
			// retains its ordinary overflow CPO and independent failure boundary.
			require(log.attempts == leaf_attempts + 1u);
			require(log.requests[leaf_attempts] == payload_size * 2u);
		}
		else
		{
			require(log.attempts == leaf_attempts);
		}
		strlike_set_curr(destination_tag{}, *buffer.result_pointer, buffer.buffer_current);
		require(*buffer.result_pointer->current == '\0');
	}
	require(log.live_owners == 0u && log.destructions == 1u);
	require(log.allocations == log.deallocations);
	active_log = nullptr;
}

inline void verify_terminal_line_failure_order()
{
	reserve_log log{};
	active_log = __builtin_addressof(log);
	{
		adaptive_buffer buffer;
		append_fragment(buffer, 512u);
		::fast_io::details::decay::basic_concat_ordered_adaptive_promote_between_pairs(buffer);
		append_fragment(buffer, 3583u);
		::fast_io::io_strlike_reference_wrapper<char, adaptive_buffer> destination{
			__builtin_addressof(buffer)};
		unsigned writer_calls{};
		terminal_reserve_leaf<1u> leaf{
			4095u, 1u, __builtin_addressof(writer_calls)};
		auto after_pair{[] { require(false); }};
		log.fail_next = true;
		bool caught{};
		try
		{
			::fast_io::details::decay::basic_general_concat_ordered_emit<true, char>(
				destination, after_pair, leaf);
		}
		catch (reserve_failure const &)
		{
			caught = true;
		}
		// The complete leaf fits the existing put area, so its observable writer
		// must finish before the independently owned newline requests allocation.
		// Reserving leaf+newline together would throw early and suppress the writer.
		require(caught && writer_calls == 1u);
		require(log.attempts == 2u && log.requests[1u] == 8192u);
		require(log.published_prefixes[1u] == 4096u);
		verify_prefix(buffer, 4096u);
		require(buffer.result_pointer->current == buffer.buffer_current);
		require(*buffer.buffer_current == '\0');
		require(log.allocations == 1u && log.live_owners == 1u);
	}
	require(log.live_owners == 0u && log.destructions == 1u);
	require(log.allocations == log.deallocations);
	active_log = nullptr;
}

inline void verify_failed_growth_preserves_owner()
{
	reserve_log log{};
	active_log = __builtin_addressof(log);
	{
		adaptive_buffer buffer;
		append_fragment(buffer, 512u);
		::fast_io::details::decay::basic_concat_ordered_adaptive_promote_between_pairs(buffer);
		append_fragment(buffer, 3584u);
		char *const begin{buffer.buffer_begin};
		char *const current{buffer.buffer_current};
		char *const end{buffer.buffer_end};
		log.fail_next = true;
		bool caught{};
		try
		{
			append_fragment(buffer, 1u);
		}
		catch (reserve_failure const &)
		{
			caught = true;
		}
		require(caught && log.live_owners == 1u);
		require(buffer.buffer_begin == begin && buffer.buffer_current == current &&
			buffer.buffer_end == end);
		require(buffer.result_pointer->current == current);
		verify_prefix(buffer, 4096u);
		// A failed reserve has not executed the fragment writer. A subsequent
		// successful reserve therefore appends exactly one byte to the same owner.
		append_fragment(buffer, 1u);
		verify_prefix(buffer, 4097u);
		require(log.requests[1u] == 8192u && log.requests[2u] == 8192u);
	}
	require(log.live_owners == 0u && log.destructions == 1u);
	require(log.allocations == log.deallocations);
	active_log = nullptr;
}

inline void verify_failed_promotion_releases_constructed_result(bool physical_overflow)
{
	reserve_log log{};
	active_log = __builtin_addressof(log);
	bool caught{};
	try
	{
		adaptive_buffer buffer;
		append_fragment(buffer, physical_overflow ? adaptive_buffer::buffer_size : 512u);
		log.fail_next = true;
		if (physical_overflow)
		{
			append_fragment(buffer, 1u);
		}
		else
		{
			::fast_io::details::decay::basic_concat_ordered_adaptive_promote_between_pairs(buffer);
		}
		require(false);
	}
	catch (reserve_failure const &)
	{
		caught = true;
	}
	// First promotion constructs its final object before reserving storage. Both
	// entry edges must destroy that live object during unwinding, even though no
	// backing allocation or final-prefix publication ever completed. This is not
	// a promise that the failed internal adapter can be reused after the throw.
	require(caught && log.attempts == 1u && log.requests[0u] == 4096u);
	require(log.published_prefixes[0u] == 0u);
	require(log.live_owners == 0u && log.destructions == 1u);
	require(log.allocations == 0u && log.deallocations == 0u);
	active_log = nullptr;
}

} // namespace ordered_adaptive_growth

int main()
{
	::ordered_adaptive_growth::verify_geometric_growth();
	::ordered_adaptive_growth::verify_terminal_exact_growth(1u);
	::ordered_adaptive_growth::verify_terminal_exact_growth(7u);
	::ordered_adaptive_growth::verify_terminal_one_shot_line<1u, 1u>(4095u);
	::ordered_adaptive_growth::verify_terminal_one_shot_line<1u, 1u>(4096u);
	::ordered_adaptive_growth::verify_terminal_one_shot_line<8u, 1u>(4095u);
	::ordered_adaptive_growth::verify_terminal_one_shot_line<1u, 0u>(4096u);
	::ordered_adaptive_growth::verify_homogeneous_terminal_peel<false>(::std::make_index_sequence<2u>{});
	::ordered_adaptive_growth::verify_homogeneous_terminal_peel<true>(::std::make_index_sequence<2u>{});
	::ordered_adaptive_growth::verify_homogeneous_terminal_peel<false>(::std::make_index_sequence<8u>{});
	::ordered_adaptive_growth::verify_homogeneous_terminal_peel<true>(::std::make_index_sequence<8u>{});
	::ordered_adaptive_growth::verify_homogeneous_terminal_peel<false>(::std::make_index_sequence<9u>{});
	::ordered_adaptive_growth::verify_homogeneous_terminal_peel<true>(::std::make_index_sequence<9u>{});
	::ordered_adaptive_growth::verify_terminal_line_failure_order();
	::ordered_adaptive_growth::verify_failed_growth_preserves_owner();
	::ordered_adaptive_growth::verify_failed_promotion_releases_constructed_result(false);
	::ordered_adaptive_growth::verify_failed_promotion_releases_constructed_result(true);
}
