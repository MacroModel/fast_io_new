#define FAST_IO_DISABLE_FLOATING_POINT

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include <fast_io.h>

#include "scan_concept_support.h"

namespace current_chunk_context_test
{

struct transactional_target;

struct transactional_proxy
{
	transactional_target *target;
};

struct transactional_target
{
	::std::array<char, 32u> value{};
	::std::size_t size{};
	::std::size_t current_chunk_commits{};
	::std::size_t context_calls{};
};

inline constexpr transactional_proxy
scan_alias_define(::fast_io::io_alias_t, transactional_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

struct transactional_state
{
	::std::array<char, 32u> value{};
	::std::size_t size{};
};

inline constexpr ::fast_io::io_type_t<transactional_state>
	scan_context_type(::fast_io::io_reserve_type_t<char, transactional_proxy>) noexcept
{
	return {};
}

inline constexpr ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, transactional_proxy>, transactional_state &state,
	char const *first, char const *last, transactional_proxy &proxy) noexcept
{
	++proxy.target->context_calls;
	for (; first != last; ++first)
	{
		if (*first == '|')
		{
			proxy.target->size = state.size;
			for (::std::size_t i{}; i != state.size; ++i)
			{
				proxy.target->value[i] = state.value[i];
			}
			return {first + 1u, ::fast_io::parse_code::ok};
		}
		if (state.size == state.value.size())
		{
			return {first, ::fast_io::parse_code::invalid};
		}
		state.value[state.size++] = *first;
	}
	return {last, ::fast_io::parse_code::partial};
}

inline constexpr ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, transactional_proxy>, transactional_state &state,
	transactional_proxy &proxy) noexcept
{
	if (state.size == 0u)
	{
		return ::fast_io::parse_code::end_of_file;
	}
	proxy.target->size = state.size;
	for (::std::size_t i{}; i != state.size; ++i)
	{
		proxy.target->value[i] = state.value[i];
	}
	return ::fast_io::parse_code::ok;
}

inline constexpr ::std::size_t
	scan_context_current_chunk_minimum_size(
		::fast_io::io_reserve_type_t<char, transactional_proxy>) noexcept
{
	return 2u;
}

inline constexpr ::fast_io::parse_result<char const *>
scan_context_current_chunk_define(
	::fast_io::io_reserve_type_t<char, transactional_proxy>, char const *first,
	char const *last, transactional_proxy &proxy) noexcept
{
	auto current{first};
	for (; current != last && *current != '|'; ++current)
	{
	}
	if (current == last)
	{
		// A miss is deliberately invisible: the target and reported iterator are unchanged.
		return {first, ::fast_io::parse_code::partial};
	}
	if (static_cast<::std::size_t>(current - first) > proxy.target->value.size())
	{
		return {current, ::fast_io::parse_code::invalid};
	}
	proxy.target->size = static_cast<::std::size_t>(current - first);
	for (::std::size_t i{}; i != proxy.target->size; ++i)
	{
		proxy.target->value[i] = first[i];
	}
	++proxy.target->current_chunk_commits;
	return {current + 1u, ::fast_io::parse_code::ok};
}

inline constexpr ::std::size_t wide_storage_size{64u};

struct wide_refill_source
{
	using input_char_type = char;

	::std::string_view source{};
	::std::size_t source_position{};
	::std::size_t chunk_size{wide_storage_size};
	::std::array<char, wide_storage_size> storage{};
	char *current{storage.data()};
	char *end{storage.data()};
	::std::size_t underflows{};

	inline constexpr void reset(::std::string_view text, ::std::size_t requested_chunk_size) noexcept
	{
		source = text;
		source_position = 0u;
		chunk_size = requested_chunk_size == 0u
						 ? 1u
						 : (requested_chunk_size < storage.size() ? requested_chunk_size : storage.size());
		current = end = storage.data();
		underflows = 0u;
	}
};

struct wide_refill_source_ref
{
	using input_char_type = char;
	wide_refill_source *source;
};

inline constexpr wide_refill_source_ref input_stream_ref_define(wide_refill_source &source) noexcept
{
	return {__builtin_addressof(source)};
}

inline constexpr char *ibuffer_begin(wide_refill_source_ref ref) noexcept
{
	return ref.source->storage.data();
}

inline constexpr char *ibuffer_curr(wide_refill_source_ref ref) noexcept
{
	return ref.source->current;
}

inline constexpr char *ibuffer_end(wide_refill_source_ref ref) noexcept
{
	return ref.source->end;
}

inline constexpr void ibuffer_set_curr(wide_refill_source_ref ref, char *current) noexcept
{
	ref.source->current = current;
}

inline bool ibuffer_underflow(wide_refill_source_ref ref) noexcept
{
	++ref.source->underflows;
	auto const remaining{ref.source->source.size() - ref.source->source_position};
	auto const count{remaining < ref.source->chunk_size ? remaining : ref.source->chunk_size};
	if (count == 0u)
	{
		return false;
	}
	for (::std::size_t i{}; i != count; ++i)
	{
		ref.source->storage[i] = ref.source->source[ref.source->source_position + i];
	}
	ref.source->source_position += count;
	ref.source->current = ref.source->storage.data();
	ref.source->end = ref.source->storage.data() + count;
	return true;
}

inline constexpr ::std::size_t ibuffer_minimum_size_define(
	::fast_io::io_reserve_type_t<char, wide_refill_source_ref>) noexcept
{
	return wide_storage_size;
}

inline void ibuffer_minimum_size_underflow_all_prepare_define(wide_refill_source_ref ref) noexcept
{
	auto const remaining{ref.source->source.size() - ref.source->source_position};
	assert(remaining >= wide_storage_size);
	for (::std::size_t i{}; i != wide_storage_size; ++i)
	{
		ref.source->storage[i] = ref.source->source[ref.source->source_position + i];
	}
	ref.source->source_position += wide_storage_size;
	ref.source->current = ref.source->storage.data();
	ref.source->end = ref.source->storage.data() + wide_storage_size;
}

inline bool equals(transactional_target const &target, ::std::string_view expected) noexcept
{
	return target.size == expected.size() &&
		   ::std::string_view(target.value.data(), target.size) == expected;
}

} // namespace current_chunk_context_test

namespace
{

using integer_proxy = decltype(::fast_io::scan_alias_define(
	::fast_io::io_alias, ::std::declval<::std::uint64_t &>()));

static_assert(::fast_io::current_chunk_context_scannable<
			  char, ::current_chunk_context_test::transactional_proxy>);
static_assert(::fast_io::current_chunk_context_scannable<char, integer_proxy>);
static_assert(!::fast_io::current_chunk_context_scannable<
			  char, ::scan_concept_harness::literal_proxy<true>>);
static_assert(::fast_io::details::scan_context_current_chunk_dispatch_available<
			  ::current_chunk_context_test::wide_refill_source_ref,
			  ::current_chunk_context_test::transactional_proxy>);
static_assert(!::fast_io::details::scan_context_current_chunk_dispatch_available<
			  ::scan_concept_harness::bounded_refill_source_ref, integer_proxy>);

inline void test_generic_transactional_dispatch()
{
	using namespace ::current_chunk_context_test;

	wide_refill_source direct_source;
	direct_source.reset(
		"direct|tail-keeps-the-advertised-buffer-capacity-available-for-the-fast-path-padding", 64u);
	auto direct_ref{input_stream_ref_define(direct_source)};
	assert(ibuffer_underflow(direct_ref));
	transactional_target direct;
	assert(::fast_io::io::scan<true>(direct_source, direct));
	assert(equals(direct, "direct"));
	assert(direct.current_chunk_commits == 1u && direct.context_calls == 0u);

	wide_refill_source split_source;
	split_source.reset(
		"boundary|tail-keeps-the-advertised-buffer-capacity-available-for-context-retry-padding", 3u);
	auto split_ref{input_stream_ref_define(split_source)};
	assert(ibuffer_underflow(split_ref));
	transactional_target split;
	assert(::fast_io::io::scan<true>(split_source, split));
	assert(equals(split, "boundary"));
	assert(split.current_chunk_commits == 0u && split.context_calls != 0u);
}

inline void test_integer_fragmented_context_path()
{
	using namespace ::scan_concept_harness;
	for (::std::size_t chunk_size{1u}; chunk_size <= refill_storage_size; ++chunk_size)
	{
		bounded_refill_source source;
		source.reset("-9223372036854775808 0 18446744073709551615", chunk_size);
		::std::int64_t minimum{};
		::std::uint64_t zero{1u};
		::std::uint64_t maximum{};
		assert(::fast_io::io::scan<true>(source, minimum, zero, maximum));
		assert(minimum == (::std::numeric_limits<::std::int64_t>::min)());
		assert(zero == 0u);
		assert(maximum == (::std::numeric_limits<::std::uint64_t>::max)());
	}
}

inline void test_integer_current_chunk_and_boundary_miss()
{
	using namespace ::current_chunk_context_test;

	wide_refill_source direct_source;
	direct_source.reset("123456789 17 trailing-input-keeps-the-current-chunk-wide", 64u);
	auto direct_ref{input_stream_ref_define(direct_source)};
	assert(ibuffer_underflow(direct_ref));
	::std::uint64_t direct{};
	assert(::fast_io::io::scan<true>(direct_source, direct));
	assert(direct == 123456789u);
	assert(direct_source.current != direct_source.storage.data());

	// The first chunk is wider than the integer context capacity, but ends in the middle of the value after skipws.
	// The transactional parse must leave both target and cursor unchanged before the state machine crosses the refill.
	wide_refill_source split_source;
	split_source.reset("                    1234567890 trailing-input", 26u);
	auto split_ref{input_stream_ref_define(split_source)};
	assert(ibuffer_underflow(split_ref));
	::std::uint64_t split{77u};
	assert(::fast_io::io::scan<true>(split_source, split));
	assert(split == 1234567890u);
	assert(split_source.underflows >= 2u);

	wide_refill_source hexadecimal_source;
	hexadecimal_source.reset("0xffffffffffffffff trailing-input-keeps-the-chunk-wide", 64u);
	auto hexadecimal_ref{input_stream_ref_define(hexadecimal_source)};
	assert(ibuffer_underflow(hexadecimal_ref));
	::std::uint64_t hexadecimal{};
	assert(::fast_io::io::scan<true>(
		hexadecimal_source, ::fast_io::mnp::hex0x_get(hexadecimal)));
	assert(hexadecimal == (::std::numeric_limits<::std::uint64_t>::max)());
}

inline void test_terminal_integer_semantics()
{
	::std::string_view text{
		"18446744073709551615 7 terminal-suffix-keeps-the-view-wide"};
	::fast_io::ibuffer_view input{text};
	::std::uint64_t maximum{};
	::std::uint64_t seven{};
	assert(::fast_io::io::scan<true>(input, maximum, seven));
	assert(maximum == (::std::numeric_limits<::std::uint64_t>::max)());
	assert(seven == 7u);
	assert(input.curr_ptr != input.end_ptr);
}

} // namespace

int main()
{
	test_generic_transactional_dispatch();
	test_integer_fragmented_context_path();
	test_integer_current_chunk_and_boundary_miss();
	test_terminal_integer_semantics();
}
