#include <array>
#include <cstddef>
#include <cstdlib>

#include <fast_io.h>

namespace lazy_ibuffer_null_state_test
{

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

struct source_state
{
	::std::array<char, 16u> storage{};
	::std::size_t current{};
	::std::size_t read_calls{};
};

struct source_ref
{
	using input_char_type = char;
	source_state *state{};
};

struct source
{
	source_state *state{};

	inline explicit constexpr source(source_state *value) noexcept : state(value)
	{}
};

[[nodiscard]] inline constexpr source_ref input_stream_ref_define(
	source &value) noexcept
{
	return {value.state};
}

inline char *read_some_underflow_define(
	source_ref input, char *first, char *last) noexcept
{
	++input.state->read_calls;
	for (; first != last && input.state->current != input.state->storage.size();
		 ++first, ++input.state->current)
	{
		*first = input.state->storage[input.state->current];
	}
	return first;
}

using input_traits = ::fast_io::basic_io_buffer_traits<
	::fast_io::buffer_mode::in, ::fast_io::native_global_allocator, char,
	void, 8u, 0u>;
using input_buffer = ::fast_io::basic_io_buffer<source, input_traits>;

inline source_state make_source_state() noexcept
{
	source_state state;
	for (auto &element : state.storage)
	{
		element = 'x';
	}
	return state;
}

template <typename operation>
inline void check_fresh_buffer_nonempty(operation execute)
{
	auto physical{make_source_state()};
	input_buffer input{__builtin_addressof(physical)};
	require(input.input_buffer.buffer_begin == nullptr);
	require(input.input_buffer.buffer_curr == nullptr);
	require(input.input_buffer.buffer_end == nullptr);
	char destination{};
	execute(input, __builtin_addressof(destination));
	// The first nonempty operation must pass through the lazy null get area,
	// allocate/refill it, and initialize the requested destination exactly once.
	require(destination == 'x');
	require(input.input_buffer.buffer_begin != nullptr);
	require(input.input_buffer.buffer_curr != nullptr);
	require(input.input_buffer.buffer_end != nullptr);
	require(physical.read_calls != 0u);
}

template <typename operation>
inline void check_fresh_buffer_empty(operation execute)
{
	auto physical{make_source_state()};
	input_buffer input{__builtin_addressof(physical)};
	execute(input);
	// Empty scalar ranges and descriptors are structurally complete in the
	// existing get area. They must neither allocate nor invoke the source CPO.
	require(input.input_buffer.buffer_begin == nullptr);
	require(input.input_buffer.buffer_curr == nullptr);
	require(input.input_buffer.buffer_end == nullptr);
	require(physical.read_calls == 0u);
}

inline void test_basic_io_buffer_matrix()
{
	check_fresh_buffer_nonempty([](input_buffer &input, char *destination) {
		require(::fast_io::operations::read_some(
					input, destination, destination + 1u) == destination + 1u);
	});
	check_fresh_buffer_nonempty([](input_buffer &input, char *destination) {
		::fast_io::operations::read_all(input, destination, destination + 1u);
	});
	check_fresh_buffer_nonempty([](input_buffer &input, char *destination) {
		auto *first{reinterpret_cast<::std::byte *>(destination)};
		require(::fast_io::operations::read_some_bytes(
					input, first, first + 1u) == first + 1u);
	});
	check_fresh_buffer_nonempty([](input_buffer &input, char *destination) {
		auto *first{reinterpret_cast<::std::byte *>(destination)};
		::fast_io::operations::read_all_bytes(input, first, first + 1u);
	});
	check_fresh_buffer_nonempty([](input_buffer &input, char *destination) {
		::fast_io::basic_io_scatter_t<char> const scatter{destination, 1u};
		auto const status{::fast_io::operations::scatter_read_some(
			input, __builtin_addressof(scatter), 1u)};
		require(status.position == 1u && status.position_in_scatter == 0u);
	});
	check_fresh_buffer_nonempty([](input_buffer &input, char *destination) {
		::fast_io::basic_io_scatter_t<char> const scatter{destination, 1u};
		::fast_io::operations::scatter_read_all(
			input, __builtin_addressof(scatter), 1u);
	});
	check_fresh_buffer_nonempty([](input_buffer &input, char *destination) {
		::fast_io::io_scatter_t const scatter{destination, 1u};
		auto const status{::fast_io::operations::scatter_read_some_bytes(
			input, __builtin_addressof(scatter), 1u)};
		require(status.position == 1u && status.position_in_scatter == 0u);
	});
	check_fresh_buffer_nonempty([](input_buffer &input, char *destination) {
		::fast_io::io_scatter_t const scatter{destination, 1u};
		::fast_io::operations::scatter_read_all_bytes(
			input, __builtin_addressof(scatter), 1u);
	});
	check_fresh_buffer_nonempty([](input_buffer &input, char *destination) {
		::std::array<::fast_io::basic_io_scatter_t<char>, 3u> const scatters{{{nullptr, 0u}, {destination, 1u}, {nullptr, 0u}}};
		auto const status{::fast_io::operations::scatter_read_some(
			input, scatters.data(), scatters.size())};
		require(status.position == scatters.size() &&
				status.position_in_scatter == 0u);
	});
	check_fresh_buffer_nonempty([](input_buffer &input, char *destination) {
		::std::array<::fast_io::io_scatter_t, 3u> const scatters{{{nullptr, 0u}, {destination, 1u}, {nullptr, 0u}}};
		::fast_io::operations::scatter_read_all_bytes(
			input, scatters.data(), scatters.size());
	});

	check_fresh_buffer_empty([](input_buffer &input) {
		char *empty{};
		require(::fast_io::operations::read_some(input, empty, empty) == empty);
	});
	check_fresh_buffer_empty([](input_buffer &input) {
		char *empty{};
		::fast_io::operations::read_all(input, empty, empty);
	});
	check_fresh_buffer_empty([](input_buffer &input) {
		::std::byte *empty{};
		require(::fast_io::operations::read_some_bytes(input, empty, empty) == empty);
	});
	check_fresh_buffer_empty([](input_buffer &input) {
		::std::byte *empty{};
		::fast_io::operations::read_all_bytes(input, empty, empty);
	});
	check_fresh_buffer_empty([](input_buffer &input) {
		::fast_io::basic_io_scatter_t<char> const scatter{nullptr, 0u};
		auto const status{::fast_io::operations::scatter_read_some(
			input, __builtin_addressof(scatter), 1u)};
		require(status.position == 1u && status.position_in_scatter == 0u);
	});
	check_fresh_buffer_empty([](input_buffer &input) {
		::fast_io::basic_io_scatter_t<char> const scatter{nullptr, 0u};
		::fast_io::operations::scatter_read_all(
			input, __builtin_addressof(scatter), 1u);
	});
	check_fresh_buffer_empty([](input_buffer &input) {
		::fast_io::io_scatter_t const scatter{nullptr, 0u};
		auto const status{::fast_io::operations::scatter_read_some_bytes(
			input, __builtin_addressof(scatter), 1u)};
		require(status.position == 1u && status.position_in_scatter == 0u);
	});
	check_fresh_buffer_empty([](input_buffer &input) {
		::fast_io::io_scatter_t const scatter{nullptr, 0u};
		::fast_io::operations::scatter_read_all_bytes(
			input, __builtin_addressof(scatter), 1u);
	});
}

template <bool byte_mode>
struct retry_state
{
	::std::array<char, 2u> published{};
	char *begin{};
	char *current{};
	char *end{};
	::std::size_t calls{};
};

template <bool byte_mode>
struct retry_ref
{
	using input_char_type = char;
	retry_state<byte_mode> *state{};
};

template <bool byte_mode>
[[nodiscard]] inline constexpr retry_ref<byte_mode> input_stream_ref_define(
	retry_ref<byte_mode> input) noexcept
{
	return input;
}

template <bool byte_mode>
[[nodiscard]] inline constexpr char *ibuffer_begin(
	retry_ref<byte_mode> input) noexcept
{
	return input.state->begin;
}

template <bool byte_mode>
[[nodiscard]] inline constexpr char *ibuffer_curr(
	retry_ref<byte_mode> input) noexcept
{
	return input.state->current;
}

template <bool byte_mode>
[[nodiscard]] inline constexpr char *ibuffer_end(
	retry_ref<byte_mode> input) noexcept
{
	return input.state->end;
}

template <bool byte_mode>
inline constexpr void ibuffer_set_curr(
	retry_ref<byte_mode> input, char *current) noexcept
{
	input.state->current = current;
}

template <bool byte_mode>
[[nodiscard]] inline constexpr bool ibuffer_underflow(
	retry_ref<byte_mode>) noexcept
{
	return false;
}

inline char *read_some_underflow_define(
	retry_ref<false> input, char *first, char *last) noexcept
{
	require(first != last);
	++input.state->calls;
	*first = 'x';
	input.state->published[0] = 'y';
	input.state->begin = input.state->current = input.state->published.data();
	input.state->end = input.state->begin + 1u;
	return first + 1u;
}

inline ::std::byte *read_some_bytes_underflow_define(
	retry_ref<true> input, ::std::byte *first, ::std::byte *last) noexcept
{
	require(first != last);
	++input.state->calls;
	*first = static_cast<::std::byte>('x');
	input.state->published[0] = 'y';
	input.state->begin = input.state->current = input.state->published.data();
	input.state->end = input.state->begin + 1u;
	return first + 1u;
}

inline void test_cold_retry_after_lazy_publication()
{
	{
		retry_state<false> state;
		retry_ref<false> input{__builtin_addressof(state)};
		char destination[2]{};
		::fast_io::operations::read_all(
			input, destination, destination + 2u);
		require(state.calls == 1u);
		require(destination[0] == 'x' && destination[1] == 'y');
		require(state.current == state.end);
	}
	{
		retry_state<true> state;
		retry_ref<true> input{__builtin_addressof(state)};
		::std::byte destination[2]{};
		::fast_io::operations::read_all_bytes(
			input, destination, destination + 2u);
		require(state.calls == 1u);
		require(destination[0] == static_cast<::std::byte>('x') &&
				destination[1] == static_cast<::std::byte>('y'));
		require(state.current == state.end);
	}
}

template <bool byte_mode>
struct empty_state
{
	::std::size_t calls{};
	::std::size_t null_calls{};
	::std::size_t anchored_calls{};
};

template <bool byte_mode>
struct empty_ref
{
	using input_char_type = char;
	empty_state<byte_mode> *state{};
};

template <bool byte_mode>
[[nodiscard]] inline constexpr empty_ref<byte_mode> input_stream_ref_define(
	empty_ref<byte_mode> input) noexcept
{
	return input;
}

template <bool byte_mode, typename pointer>
inline pointer observe_empty_call(
	empty_ref<byte_mode> input, pointer first, pointer last) noexcept
{
	require(first == last);
	++input.state->calls;
	if (first == nullptr)
	{
		++input.state->null_calls;
	}
	else
	{
		++input.state->anchored_calls;
	}
	return first;
}

inline char *read_some_underflow_define(
	empty_ref<false> input, char *first, char *last) noexcept
{
	return observe_empty_call(input, first, last);
}

inline ::std::byte *read_some_bytes_underflow_define(
	empty_ref<true> input, ::std::byte *first, ::std::byte *last) noexcept
{
	return observe_empty_call(input, first, last);
}

inline void test_empty_scalarization_observability()
{
	{
		empty_state<false> state;
		empty_ref<false> input{__builtin_addressof(state)};
		char *empty{};
		require(::fast_io::operations::read_some(input, empty, empty) == empty);
		require(state.calls == 1u && state.null_calls == 1u);
	}
	{
		empty_state<false> state;
		empty_ref<false> input{__builtin_addressof(state)};
		::fast_io::basic_io_scatter_t<char> const scatter{nullptr, 0u};
		auto const status{::fast_io::operations::scatter_read_some(
			input, __builtin_addressof(scatter), 1u)};
		require(status.position == 1u && status.position_in_scatter == 0u);
		require(state.calls == 1u && state.anchored_calls == 1u);
	}
	{
		empty_state<false> state;
		empty_ref<false> input{__builtin_addressof(state)};
		::fast_io::basic_io_scatter_t<char> const scatter{nullptr, 0u};
		::fast_io::operations::scatter_read_all(
			input, __builtin_addressof(scatter), 1u);
		require(state.calls == 1u && state.anchored_calls == 1u);
	}
	{
		empty_state<true> state;
		empty_ref<true> input{__builtin_addressof(state)};
		::std::byte *empty{};
		require(::fast_io::operations::read_some_bytes(
					input, empty, empty) == empty);
		require(state.calls == 1u && state.null_calls == 1u);
	}
	{
		empty_state<true> state;
		empty_ref<true> input{__builtin_addressof(state)};
		::fast_io::io_scatter_t const scatter{nullptr, 0u};
		auto const status{::fast_io::operations::scatter_read_some_bytes(
			input, __builtin_addressof(scatter), 1u)};
		require(status.position == 1u && status.position_in_scatter == 0u);
		require(state.calls == 1u && state.anchored_calls == 1u);
	}
	{
		empty_state<true> state;
		empty_ref<true> input{__builtin_addressof(state)};
		::fast_io::io_scatter_t const scatter{nullptr, 0u};
		::fast_io::operations::scatter_read_all_bytes(
			input, __builtin_addressof(scatter), 1u);
		require(state.calls == 1u && state.anchored_calls == 1u);
	}
}

} // namespace lazy_ibuffer_null_state_test

int main()
{
	using namespace ::lazy_ibuffer_null_state_test;
	test_basic_io_buffer_matrix();
	test_cold_retry_after_lazy_publication();
	test_empty_scalarization_observability();
}
