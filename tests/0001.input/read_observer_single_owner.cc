#include <array>
#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>

#include <fast_io_core.h>

namespace read_observer_single_owner
{

struct counters
{
	::std::size_t normalizations{};
	::std::size_t constructions{};
	::std::size_t moves{};
	::std::size_t destructions{};
	::std::size_t live{};
	::std::size_t maximum_live{};
	::std::size_t read_calls{};
	::std::size_t pread_calls{};
};

struct move_only_observer
{
	using input_char_type = char;
	counters *state{};

	inline explicit move_only_observer(counters *value) noexcept : state(value)
	{
		++state->constructions;
		++state->live;
		if (state->maximum_live < state->live)
		{
			state->maximum_live = state->live;
		}
	}

	move_only_observer(move_only_observer const &) = delete;
	move_only_observer &operator=(move_only_observer const &) = delete;

	inline move_only_observer(move_only_observer &&other) noexcept : state(other.state)
	{
		other.state = nullptr;
		++state->moves;
		++state->live;
		if (state->maximum_live < state->live)
		{
			state->maximum_live = state->live;
		}
	}

	move_only_observer &operator=(move_only_observer &&) = delete;

	inline ~move_only_observer()
	{
		if (state != nullptr)
		{
			++state->destructions;
			--state->live;
		}
	}
};

struct owned_source
{
	counters *state{};
};

inline move_only_observer input_stream_ref_define(owned_source &source) noexcept
{
	++source.state->normalizations;
	return move_only_observer{source.state};
}

inline char *read_some_underflow_define(move_only_observer &observer, char *first, char *last) noexcept
{
	++observer.state->read_calls;
	for (; first != last; ++first)
	{
		*first = 'r';
	}
	return last;
}

inline char *pread_some_underflow_define(move_only_observer &observer, char *first, char *last,
										 ::fast_io::intfpos_t offset) noexcept
{
	++observer.state->pread_calls;
	char const value{static_cast<char>('a' + offset)};
	for (; first != last; ++first)
	{
		*first = value;
	}
	return last;
}

// A large, noncopyable lvalue result models the conservative path for ABIs whose aggregate classification is not
// explicitly known. The stream-ref CPO must preserve this stable reference; neither a size threshold nor an x86/AArch64
// assumption may force the object through a value parameter.
struct large_borrowed_observer
{
	using input_char_type = char;
	counters *state{};
	::std::byte payload[128]{};

	inline explicit large_borrowed_observer(counters *value) noexcept : state(value)
	{}
	large_borrowed_observer(large_borrowed_observer const &) = delete;
	large_borrowed_observer &operator=(large_borrowed_observer const &) = delete;
};

struct borrowed_source
{
	large_borrowed_observer observer;

	inline explicit borrowed_source(counters *value) noexcept : observer(value)
	{}
};

inline large_borrowed_observer &input_stream_ref_define(borrowed_source &source) noexcept
{
	++source.observer.state->normalizations;
	return source.observer;
}

inline char *read_some_underflow_define(large_borrowed_observer &observer, char *first, char *last) noexcept
{
	++observer.state->read_calls;
	for (; first != last; ++first)
	{
		*first = 'b';
	}
	return last;
}

struct partial_observer
{
	using input_char_type = char;
	counters *state{};
};

struct partial_source
{
	partial_observer observer;
};

inline partial_observer &input_stream_ref_define(partial_source &source) noexcept
{
	++source.observer.state->normalizations;
	return source.observer;
}

inline char *read_some_underflow_define(partial_observer &observer, char *first, char *last) noexcept
{
	++observer.state->read_calls;
	if (first == last)
	{
		return first;
	}
	*first = 'p';
	return first + 1;
}

static_assert(::fast_io::operations::defines::has_input_stream_ref_define<owned_source &>);
static_assert(::fast_io::operations::defines::has_input_stream_ref_define<borrowed_source &>);
static_assert(::std::is_lvalue_reference_v<decltype(::fast_io::operations::input_stream_ref(::std::declval<borrowed_source &>()))>);

inline void verify_one_owner(counters const &state, ::std::size_t completed_calls)
{
	assert(state.normalizations == completed_calls);
	assert(state.constructions == completed_calls);
	assert(state.moves == 0u);
	assert(state.destructions == completed_calls);
	assert(state.live == 0u);
	assert(state.maximum_live == 1u);
}

} // namespace read_observer_single_owner

int main()
{
	using namespace ::read_observer_single_owner;
	counters state;
	owned_source source{__builtin_addressof(state)};
	::std::size_t calls{};

	{
		char buffer[3]{};
		assert(::fast_io::operations::read_some(source, buffer, buffer + 3) == buffer + 3);
		++calls;
	}
	verify_one_owner(state, calls);

	{
		char buffer[3]{};
		::fast_io::operations::read_all(source, buffer, buffer + 3);
		++calls;
	}
	verify_one_owner(state, calls);

	{
		::std::byte buffer[3]{};
		assert(::fast_io::operations::read_some_bytes(source, buffer, buffer + 3) == buffer + 3);
		++calls;
	}
	verify_one_owner(state, calls);

	{
		::std::byte buffer[3]{};
		::fast_io::operations::read_all_bytes(source, buffer, buffer + 3);
		++calls;
	}
	verify_one_owner(state, calls);

	{
		char first[2]{};
		char second[3]{};
		::fast_io::basic_io_scatter_t<char> scatter[]{{first, 2u}, {second, 3u}};
		auto const status{::fast_io::operations::scatter_read_some(source, scatter, 2u)};
		assert(status.position == 2u && status.position_in_scatter == 0u);
		++calls;
	}
	verify_one_owner(state, calls);

	{
		char first[2]{};
		char second[3]{};
		::fast_io::basic_io_scatter_t<char> scatter[]{{first, 2u}, {second, 3u}};
		::fast_io::operations::scatter_read_all(source, scatter, 2u);
		++calls;
	}
	verify_one_owner(state, calls);

	{
		::std::byte first[2]{};
		::std::byte second[3]{};
		::fast_io::io_scatter_t scatter[]{{first, 2u}, {second, 3u}};
		auto const status{::fast_io::operations::scatter_read_some_bytes(source, scatter, 2u)};
		assert(status.position == 2u && status.position_in_scatter == 0u);
		++calls;
	}
	verify_one_owner(state, calls);

	{
		::std::byte first[2]{};
		::std::byte second[3]{};
		::fast_io::io_scatter_t scatter[]{{first, 2u}, {second, 3u}};
		::fast_io::operations::scatter_read_all_bytes(source, scatter, 2u);
		++calls;
	}
	verify_one_owner(state, calls);

	{
		char buffer[3]{};
		assert(::fast_io::operations::pread_some(source, buffer, buffer + 3, 2) == buffer + 3);
		++calls;
	}
	verify_one_owner(state, calls);

	{
		char buffer[3]{};
		::fast_io::operations::pread_all(source, buffer, buffer + 3, 3);
		++calls;
	}
	verify_one_owner(state, calls);

	{
		::std::byte buffer[3]{};
		assert(::fast_io::operations::pread_some_bytes(source, buffer, buffer + 3, 4) == buffer + 3);
		++calls;
	}
	verify_one_owner(state, calls);

	{
		::std::byte buffer[3]{};
		::fast_io::operations::pread_all_bytes(source, buffer, buffer + 3, 5);
		++calls;
	}
	verify_one_owner(state, calls);

	{
		char first[2]{};
		char second[3]{};
		::fast_io::basic_io_scatter_t<char> scatter[]{{first, 2u}, {second, 3u}};
		auto const status{::fast_io::operations::scatter_pread_some(source, scatter, 2u, 6)};
		assert(status.position == 2u && status.position_in_scatter == 0u);
		++calls;
	}
	verify_one_owner(state, calls);

	{
		char first[2]{};
		char second[3]{};
		::fast_io::basic_io_scatter_t<char> scatter[]{{first, 2u}, {second, 3u}};
		::fast_io::operations::scatter_pread_all(source, scatter, 2u, 7);
		++calls;
	}
	verify_one_owner(state, calls);

	{
		::std::byte first[2]{};
		::std::byte second[3]{};
		::fast_io::io_scatter_t scatter[]{{first, 2u}, {second, 3u}};
		auto const status{::fast_io::operations::scatter_pread_some_bytes(source, scatter, 2u, 8)};
		assert(status.position == 2u && status.position_in_scatter == 0u);
		++calls;
	}
	verify_one_owner(state, calls);

	{
		::std::byte first[2]{};
		::std::byte second[3]{};
		::fast_io::io_scatter_t scatter[]{{first, 2u}, {second, 3u}};
		::fast_io::operations::scatter_pread_all_bytes(source, scatter, 2u, 9);
		++calls;
	}
	verify_one_owner(state, calls);

	{
		char buffer[3]{};
		::fast_io::containers::span<char> destination{buffer, 3u};
		auto const result{::fast_io::operations::read_some_span(source, destination)};
		assert(result.data() == buffer && result.size() == 3u);
		++calls;
	}
	verify_one_owner(state, calls);

	{
		char first[2]{};
		char second[3]{};
		::fast_io::basic_io_scatter_t<char> scatter[]{{first, 2u}, {second, 3u}};
		::fast_io::containers::span<::fast_io::basic_io_scatter_t<char> const> destination{scatter, 2u};
		auto const status{::fast_io::operations::scatter_pread_some_span(source, destination, 10)};
		assert(status.position == 2u && status.position_in_scatter == 0u);
		++calls;
	}
	verify_one_owner(state, calls);

	// The parameter type of the scatter entry point is formed from remove_cvref_t<ref-result>. This call therefore
	// checks both signature admission and the run-time guarantee that a large noncopyable observer remains borrowed.
	counters borrowed_state;
	borrowed_source borrowed{__builtin_addressof(borrowed_state)};
	char borrowed_buffer[2]{};
	::fast_io::basic_io_scatter_t<char> borrowed_scatter[]{
		{borrowed_buffer, sizeof(borrowed_buffer)}};
	::fast_io::containers::span<::fast_io::basic_io_scatter_t<char> const> borrowed_destination{
		borrowed_scatter, 1u};
	auto const borrowed_status{
		::fast_io::operations::scatter_read_some_span(borrowed, borrowed_destination)};
	assert(borrowed_status.position == 1u);
	assert(borrowed_state.normalizations == 1u);
	assert(borrowed_state.read_calls == 1u);
	assert(borrowed_buffer[0] == 'b' && borrowed_buffer[1] == 'b');

	// `read_all_span` must select the all-controller after normalization. A one-element backend proves that the
	// controller retries with the same borrowed observer instead of accidentally exposing read-some semantics.
	counters partial_state;
	partial_source partial{{__builtin_addressof(partial_state)}};
	char partial_buffer[3]{};
	::fast_io::containers::span<char> partial_destination{partial_buffer, 3u};
	::fast_io::operations::read_all_span(partial, partial_destination);
	assert(partial_state.normalizations == 1u);
	assert(partial_state.read_calls == 3u);
	assert(partial_buffer[0] == 'p' && partial_buffer[1] == 'p' && partial_buffer[2] == 'p');
}
