#include <cassert>
#include <cstddef>

#if !defined(FAST_IO_DISABLE_FLOATING_POINT)
#define FAST_IO_DISABLE_FLOATING_POINT
#define FAST_IO_IO_BUFFER_OBSERVER_TEST_RESTORE_FLOATING_POINT
#endif
#include <fast_io.h>
#if defined(FAST_IO_IO_BUFFER_OBSERVER_TEST_RESTORE_FLOATING_POINT)
#undef FAST_IO_IO_BUFFER_OBSERVER_TEST_RESTORE_FLOATING_POINT
#undef FAST_IO_DISABLE_FLOATING_POINT
#endif

namespace io_buffer_observer_single_owner
{

struct backend_state
{
	char input[2]{'i', 'n'};
	::std::size_t input_position{};
	char output[8]{};
	::std::size_t output_size{};
	::std::size_t read_calls{};
	::std::size_t write_calls{};
};

// The deliberately large, noncopyable object represents an identity-bearing cursor rather than a compact handle.
// Returning it by lvalue reference forces every buffer bridge to prove stable borrowing independently of the native
// ABI's aggregate register rules.
struct backend_observer
{
	using input_char_type = char;
	using output_char_type = char;

	backend_state *state{};
	unsigned char padding[128]{};

	inline explicit constexpr backend_observer(backend_state *value) noexcept : state(value) {}
	backend_observer(backend_observer const &) = delete;
	backend_observer &operator=(backend_observer const &) = delete;
	backend_observer(backend_observer &&) = delete;
	backend_observer &operator=(backend_observer &&) = delete;
};

struct backend_handle
{
	backend_observer observer;

	inline explicit constexpr backend_handle(backend_state *state) noexcept : observer(state) {}
};

inline constexpr backend_observer &input_stream_ref_define(backend_handle &handle) noexcept
{
	return handle.observer;
}

inline constexpr backend_observer &output_stream_ref_define(backend_handle &handle) noexcept
{
	return handle.observer;
}

inline constexpr char *read_some_underflow_define(
	backend_observer &observer, char *first, char *last) noexcept
{
	++observer.state->read_calls;
	while (first != last && observer.state->input_position != sizeof(observer.state->input))
	{
		*first++ = observer.state->input[observer.state->input_position++];
	}
	return first;
}

inline constexpr void write_all_overflow_define(
	backend_observer &observer, char const *first, char const *last) noexcept
{
	++observer.state->write_calls;
	while (first != last)
	{
		assert(observer.state->output_size != sizeof(observer.state->output));
		observer.state->output[observer.state->output_size++] = *first++;
	}
}

static_assert(!::std::copy_constructible<backend_observer>);
static_assert(::fast_io::operations::defines::has_input_stream_ref_define<backend_handle &>);
static_assert(::fast_io::operations::defines::has_output_stream_ref_define<backend_handle &>);

} // namespace io_buffer_observer_single_owner

int main()
{
	using namespace ::io_buffer_observer_single_owner;
	backend_state state;
	::fast_io::basic_iobuf<backend_handle> buffer{__builtin_addressof(state)};
	auto buffer_ref{::fast_io::operations::io_stream_ref(buffer)};

	// The short output first remains in the outer buffer. Input underflow on a tied iobuf must flush it through the
	// noncopyable output observer and then refill through the same noncopyable input observer, each normalized once.
	char const output[]{'o', 'u', 't'};
	write_all_overflow_define(buffer_ref, output, output + sizeof(output));
	assert(state.write_calls == 0u);
	assert(ibuffer_underflow(buffer_ref));
	assert(state.write_calls == 1u);
	assert(state.output_size == sizeof(output));
	assert(state.output[0] == 'o' && state.output[1] == 'u' && state.output[2] == 't');
	assert(state.read_calls == 1u);
	assert(ibuffer_curr(buffer_ref) != ibuffer_end(buffer_ref));
	assert(*ibuffer_curr(buffer_ref) == 'i');
}
