#include <array>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <string_view>

#include <fast_io.h>
#include <fast_io_dsal/string.h>

namespace
{

namespace unproved_strlike_output
{

struct state
{
	::std::array<char, 64u> storage{};
	char *current{storage.data()};
};

inline constexpr char *strlike_begin(::fast_io::io_strlike_type_t<char, state>, state &value) noexcept
{
	return value.storage.data();
}

inline constexpr char *strlike_curr(::fast_io::io_strlike_type_t<char, state>, state &value) noexcept
{
	return value.current;
}

inline constexpr char *strlike_end(::fast_io::io_strlike_type_t<char, state>, state &value) noexcept
{
	return value.storage.data() + value.storage.size();
}

inline constexpr void strlike_set_curr(
	::fast_io::io_strlike_type_t<char, state>, state &value, char *position) noexcept
{
	value.current = position;
}

inline constexpr void strlike_reserve(
	::fast_io::io_strlike_type_t<char, state>, state &, ::std::size_t) noexcept
{}

using output = ::fast_io::io_strlike_reference_wrapper<char, state>;

inline void status_print_define(
	output, ::fast_io::basic_io_scatter_t<char>, ::fast_io::basic_io_scatter_t<char>) noexcept
{}

} // namespace unproved_strlike_output

namespace extensible_string_protocol
{

struct traits : ::std::char_traits<char>
{};

template <typename element_type>
struct allocator
{
	using value_type = element_type;

	constexpr allocator() noexcept = default;

	template <typename other_type>
	constexpr allocator(allocator<other_type> const &) noexcept
	{}

	[[nodiscard]] value_type *allocate(::std::size_t count)
	{
		return ::std::allocator<value_type>{}.allocate(count);
	}

	void deallocate(value_type *pointer, ::std::size_t count) noexcept
	{
		::std::allocator<value_type>{}.deallocate(pointer, count);
	}

	template <typename other_type>
	friend constexpr bool operator==(allocator, allocator<other_type>) noexcept
	{
		return true;
	}
};

// Derivation retains the native allocation machinery while deliberately adding this namespace to the type's ADL set.
struct fast_io_allocator : ::fast_io::native_global_allocator
{};

using traits_string = ::std::basic_string<char, traits>;
using traits_string_view = ::std::basic_string_view<char, traits>;
using allocator_string = ::std::basic_string<char, ::std::char_traits<char>, allocator<char>>;
using fast_io_string = ::fast_io::containers::basic_string<char, fast_io_allocator>;

inline ::fast_io::basic_io_scatter_t<char> print_alias_define(
	::fast_io::io_alias_t, traits_string const &) noexcept
{
	static char scratch{'t'};
	return {__builtin_addressof(scratch), 1u};
}

inline ::fast_io::basic_io_scatter_t<char> print_alias_define(
	::fast_io::io_alias_t, traits_string_view) noexcept
{
	static char scratch{'v'};
	return {__builtin_addressof(scratch), 1u};
}

inline ::fast_io::basic_io_scatter_t<char> print_alias_define(
	::fast_io::io_alias_t, allocator_string const &) noexcept
{
	static char scratch{'a'};
	return {__builtin_addressof(scratch), 1u};
}

inline ::fast_io::basic_io_scatter_t<char> print_alias_define(
	::fast_io::io_alias_t, fast_io_string const &) noexcept
{
	static char scratch{'f'};
	return {__builtin_addressof(scratch), 1u};
}

} // namespace extensible_string_protocol

struct put_area_state
{
	::std::array<char, 512u> storage{};
	char *current{storage.data()};
	::std::size_t capacity{storage.size()};
	::std::string overflow_output;
};

struct put_area_sink
{
	using output_char_type = char;
	put_area_state *state;
};

inline constexpr put_area_sink output_stream_ref_define(put_area_sink sink) noexcept
{
	return sink;
}

inline constexpr ::std::true_type print_deferred_obuffer_commit_safe(
	::fast_io::io_reserve_type_t<char, put_area_sink>) noexcept
{
	// The array never relocates, cursor queries are observational, and the setter only publishes `current`.
	return {};
}

inline constexpr char *obuffer_begin(put_area_sink sink) noexcept
{
	return sink.state->storage.data();
}

inline constexpr char *obuffer_curr(put_area_sink sink) noexcept
{
	return sink.state->current;
}

inline constexpr char *obuffer_end(put_area_sink sink) noexcept
{
	return sink.state->storage.data() + sink.state->capacity;
}

inline constexpr void obuffer_set_curr(put_area_sink sink, char *position) noexcept
{
	sink.state->current = position;
}

inline void write_all_overflow_define(put_area_sink sink, char const *first, char const *last)
{
	auto &state{*sink.state};
	state.overflow_output.append(state.storage.data(), state.current);
	state.current = state.storage.data();
	state.overflow_output.append(first, last);
}

struct replay_element
{
	char character;
	::std::size_t *observations;
};

inline ::fast_io::basic_io_scatter_t<char> print_alias_define(
	::fast_io::io_alias_t, replay_element &element) noexcept
{
	++*element.observations;
	return {__builtin_addressof(element.character), 1u};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, replay_element>) noexcept
{
	// The descriptor points into the caller-owned array, and observing one unchanged element is repeatable.
	return {};
}

inline constexpr ::std::true_type print_scatter_output_state_independent(
	::fast_io::io_reserve_type_t<char, replay_element>) noexcept
{
	// Observation touches only the element and its test counter; it cannot inspect the destination put cursor.
	return {};
}

inline constexpr ::std::true_type print_scatter_direct_print_equivalent(
	::fast_io::io_reserve_type_t<char, replay_element>) noexcept
{
	// This test type has no status/direct-print hook; its one-byte scatter is its complete output semantics.
	return {};
}

struct repeatable_cursor_dependent_element
{
	char character;
};

inline ::fast_io::basic_io_scatter_t<char> print_alias_define(
	::fast_io::io_alias_t, repeatable_cursor_dependent_element &element) noexcept
{
	return {__builtin_addressof(element.character), 1u};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, repeatable_cursor_dependent_element>) noexcept
{
	// Stable bytes and repeatability intentionally do not opt this source into cursor-independent observation.
	return {};
}

struct direct_semantics_unproven_element
{
	char character;
};

inline ::fast_io::basic_io_scatter_t<char> print_alias_define(
	::fast_io::io_alias_t, direct_semantics_unproven_element &element) noexcept
{
	return {__builtin_addressof(element.character), 1u};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, direct_semantics_unproven_element>) noexcept
{
	return {};
}

inline constexpr ::std::true_type print_scatter_output_state_independent(
	::fast_io::io_reserve_type_t<char, direct_semantics_unproven_element>) noexcept
{
	// Cursor independence alone deliberately does not certify that bypassing a possible status hook is equivalent.
	return {};
}

struct potentially_throwing_element
{
	char character;
	::std::size_t *observations;
	bool fail;
};

struct expected_failure
{};

inline ::fast_io::basic_io_scatter_t<char> print_alias_define(
	::fast_io::io_alias_t, potentially_throwing_element &element)
{
	++*element.observations;
	if (element.fail)
	{
		throw expected_failure{};
	}
	return {__builtin_addressof(element.character), 1u};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, potentially_throwing_element>) noexcept
{
	return {};
}

template <::std::size_t count>
inline ::std::string_view written(put_area_state const &state) noexcept
{
	(void)count;
	return {state.storage.data(), static_cast<::std::size_t>(state.current - state.storage.data())};
}

inline ::std::string complete_written(put_area_state const &state)
{
	::std::string result{state.overflow_output};
	result.append(
		state.storage.data(),
		static_cast<::std::size_t>(state.current - state.storage.data()));
	return result;
}

void test_profitable_nothrow_direct_scatter()
{
	using unproved_output = unproved_strlike_output::output;
	static_assert(::fast_io::buffer_strlike<char, unproved_strlike_output::state>);
	static_assert(!::fast_io::buffered_print_preferred_strlike<
		char, unproved_strlike_output::state>);
	static_assert(!::fast_io::deferred_obuffer_commit_safe_strlike<
		char, unproved_strlike_output::state>);
	static_assert(!::fast_io::buffered_printable_preferred_stream<char, unproved_output>);
	static_assert(!::fast_io::deferred_obuffer_commit_safe<char, unproved_output>);
	static_assert(requires(unproved_output out, ::fast_io::basic_io_scatter_t<char> scatter) {
		status_print_define(out, scatter, scatter);
	});
	static_assert(::fast_io::buffered_print_preferred_strlike<char, ::fast_io::string>);
	static_assert(::fast_io::deferred_obuffer_commit_safe_strlike<char, ::fast_io::string>);
	static_assert(::fast_io::buffered_print_preferred_strlike<char, ::std::string>);
	static_assert(::fast_io::buffered_printable_preferred_stream<
		char, ::fast_io::ostring_ref_std>);
	static_assert(::fast_io::deferred_obuffer_commit_safe<char, put_area_sink>);
	static_assert(::fast_io::scatter_output_state_independent<char, replay_element>);
	static_assert(::fast_io::scatter_direct_print_equivalent<char, replay_element>);
	static_assert(::fast_io::scatter_output_state_independent<char, ::std::string_view>);
	static_assert(::fast_io::scatter_direct_print_equivalent<char, ::std::string_view>);
	static_assert(::fast_io::scatter_output_state_independent<char, ::std::string>);
	static_assert(::fast_io::scatter_direct_print_equivalent<char, ::std::string>);
	// Traits and allocator template arguments contribute associated namespaces. Each exact test overload above replaces
	// the ordinary data/size alias with shared scratch, proving why blanket specialization markers would be unsound.
	static_assert(!::fast_io::borrowed_scatter_source<
		char, extensible_string_protocol::traits_string>);
	static_assert(!::fast_io::scatter_output_state_independent<
		char, extensible_string_protocol::traits_string>);
	static_assert(!::fast_io::scatter_direct_print_equivalent<
		char, extensible_string_protocol::traits_string>);
	static_assert(!::fast_io::sized_range_view_nothrow_direct_scatter_v<
		char, extensible_string_protocol::traits_string *>);
	static_assert(!::fast_io::borrowed_scatter_source<
		char, extensible_string_protocol::traits_string_view>);
	static_assert(!::fast_io::sized_range_view_nothrow_direct_scatter_v<
		char, extensible_string_protocol::traits_string_view *>);
	static_assert(!::fast_io::borrowed_scatter_source<
		char, extensible_string_protocol::allocator_string>);
	static_assert(!::fast_io::sized_range_view_nothrow_direct_scatter_v<
		char, extensible_string_protocol::allocator_string *>);
	static_assert(!::fast_io::borrowed_scatter_source<
		char, extensible_string_protocol::fast_io_string>);
	static_assert(!::fast_io::sized_range_view_nothrow_direct_scatter_v<
		char, extensible_string_protocol::fast_io_string *>);
	static_assert(::fast_io::deferred_obuffer_commit_safe<char, ::fast_io::ostring_ref_fast_io>);
	static_assert(!::fast_io::scatter_output_state_independent<
		char, repeatable_cursor_dependent_element>);
	static_assert(!::fast_io::scatter_direct_print_equivalent<
		char, repeatable_cursor_dependent_element>);
	static_assert(!::fast_io::sized_range_view_nothrow_direct_scatter_v<
		char, repeatable_cursor_dependent_element *>);
	static_assert(::fast_io::scatter_output_state_independent<
		char, direct_semantics_unproven_element>);
	static_assert(!::fast_io::scatter_direct_print_equivalent<
		char, direct_semantics_unproven_element>);
	static_assert(!::fast_io::sized_range_view_nothrow_direct_scatter_v<
		char, direct_semantics_unproven_element *>);

	::std::size_t observations{};
	::std::array<replay_element, 64u> elements{};
	::std::string expected;
	for (::std::size_t i{}; i != elements.size(); ++i)
	{
		char const character{static_cast<char>('a' + i % 26u)};
		elements[i] = {character, __builtin_addressof(observations)};
		if (i != 0u)
		{
			expected.append("::");
		}
		expected.push_back(character);
	}
	// A two-code-unit separator keeps the profitable direct loop covered without pretending that a run-time descriptor
	// has a static extent. The existing short-put-area cases below retain one-code-unit and fallback coverage.
	auto range{::fast_io::mnp::rgvw(elements, "::")};
	static_assert(::fast_io::sized_range_view_nothrow_direct_scatter_v<
		char, typename decltype(range)::iterator>);

	put_area_state state;
	::fast_io::print(put_area_sink{__builtin_addressof(state)}, range);
	assert(written<64u>(state) == expected);
	// The explicit source/stream opt-ins permit one traversal and one final cursor publication.
	assert(observations == elements.size());

	observations = 0u;
	put_area_state adjacent_state;
	::fast_io::print(
		put_area_sink{__builtin_addressof(adjacent_state)}, ::std::string_view{}, range,
		::std::string_view{});
	assert(written<64u>(adjacent_state) == expected);
	// Empty adjacent leaves must not make the outer dispatcher add another source traversal.
	assert(observations == elements.size());
}

void test_small_range_keeps_single_pass()
{
	::std::size_t observations{};
	::std::array<replay_element, 4u> elements{{
		{'a', __builtin_addressof(observations)},
		{'b', __builtin_addressof(observations)},
		{'c', __builtin_addressof(observations)},
		{'d', __builtin_addressof(observations)},
	}};
	auto range{::fast_io::mnp::rgvw(elements, ":")};
	put_area_state state;
	::fast_io::print(put_area_sink{__builtin_addressof(state)}, range);
	assert(written<4u>(state) == "a:b:c:d");
	assert(observations == elements.size());
}

void test_short_put_area_handoff_does_not_reobserve_boundary()
{
	::std::size_t observations{};
	::std::array<replay_element, 16u> elements{};
	::std::string expected;
	for (::std::size_t i{}; i != elements.size(); ++i)
	{
		char const character{static_cast<char>('a' + i)};
		elements[i] = {character, __builtin_addressof(observations)};
		if (i != 0u)
		{
			expected.push_back(':');
		}
		expected.push_back(character);
	}
	auto range{::fast_io::mnp::rgvw(elements, ":")};
	put_area_state state;
	state.capacity = 5u;
	::fast_io::print(put_area_sink{__builtin_addressof(state)}, range);
	assert(complete_written(state) == expected);
	// The helper caches the first non-fitting descriptor before handing off; restarting at that element would make this
	// count seventeen even though borrowed/repeatable provenance would keep the bytes superficially correct.
	assert(observations == elements.size());
}

void test_throwing_replay_is_not_reordered()
{
	::std::size_t observations{};
	::std::array<potentially_throwing_element, 8u> elements{};
	for (::std::size_t i{}; i != elements.size(); ++i)
	{
		elements[i] = {
			static_cast<char>('a' + i), __builtin_addressof(observations), i == 3u};
	}
	auto range{::fast_io::mnp::rgvw(elements, ":")};
	static_assert(!::fast_io::sized_range_view_nothrow_direct_scatter_v<
		char, typename decltype(range)::iterator>);

	put_area_state state;
	try
	{
		::fast_io::print(put_area_sink{__builtin_addressof(state)}, range);
		assert(false);
	}
	catch (expected_failure const &)
	{
	}
	// Earlier elements remain committed exactly as in the historical one-pass ordering. A speculative measuring pass
	// would instead throw before exposing any prefix, which is why `noexcept` is part of strategy admission.
	assert(!written<8u>(state).empty());
	assert(written<8u>(state).size() < 15u);
	assert(observations == 4u);
}

} // namespace

int main()
{
	test_profitable_nothrow_direct_scatter();
	test_small_range_keeps_single_pass();
	test_short_put_area_handoff_does_not_reobserve_boundary();
	test_throwing_replay_is_not_reordered();
}
