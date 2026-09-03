#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>

#if defined(__unix__) || defined(__APPLE__)
#include <csignal>
#include <sys/time.h>
#endif

namespace fast_io
{

/*
The benchmark observes the exact byte extent requested by fast_io's native
thread-local staging allocator.  This is a protocol observation rather than a
surrogate RSS measurement: typed_generic_allocator_adapter performs the sole
element-to-byte conversion, so the recorded value distinguishes element-domain
and byte-domain capacity without changing either transmit CPO.
*/
class custom_thread_local_allocator
{
public:
	inline static ::std::size_t allocation_calls{};
	inline static ::std::size_t deallocation_calls{};
	inline static ::std::size_t allocation_bytes{};
	inline static ::std::size_t last_allocation_bytes{};

	[[nodiscard]] static inline void *allocate(::std::size_t bytes) noexcept
	{
		++allocation_calls;
		allocation_bytes += bytes;
		last_allocation_bytes = bytes;
		void *const pointer{::std::malloc(bytes == 0u ? 1u : bytes)};
		if (pointer == nullptr)
		{
			::std::abort();
		}
		return pointer;
	}

	static inline void deallocate(void *pointer) noexcept
	{
		++deallocation_calls;
		::std::free(pointer);
	}

	static inline void reset_observation() noexcept
	{
		allocation_calls = 0u;
		deallocation_calls = 0u;
		allocation_bytes = 0u;
		last_allocation_bytes = 0u;
	}
};

} // namespace fast_io

#define FAST_IO_USE_CUSTOM_THREAD_LOCAL_ALLOCATOR
#include <fast_io_core.h>

#ifndef FAST_IO_FINITE_STAGING_CHAR_WIDTH
#define FAST_IO_FINITE_STAGING_CHAR_WIDTH 1
#endif

#ifndef FAST_IO_FINITE_STAGING_KIND
#define FAST_IO_FINITE_STAGING_KIND 0
#endif

#ifndef FAST_IO_FINITE_STAGING_MIN_POLICY
#define FAST_IO_FINITE_STAGING_MIN_POLICY 1
#endif

namespace fast_io_finite_staging_ab
{

inline constexpr unsigned selected_char_width{
	FAST_IO_FINITE_STAGING_CHAR_WIDTH};
inline constexpr unsigned selected_kind{FAST_IO_FINITE_STAGING_KIND};
inline constexpr bool selected_min_policy{
	FAST_IO_FINITE_STAGING_MIN_POLICY != 0};
static_assert(selected_char_width == 1u || selected_char_width == 2u);
static_assert(selected_kind <= 3u);

using char_type = ::std::conditional_t<selected_char_width == 1u, char, char16_t>;
inline constexpr bool selected_bytes{selected_kind >= 2u};
inline constexpr ::std::size_t maximum_request{4097u};
inline constexpr ::std::size_t fixed_staging_bytes{
	selected_bytes
		? ::fast_io::details::transmit_buffer_size_cache<1>
		: ::fast_io::details::transmit_buffer_size_cache<sizeof(char_type)> *
			sizeof(char_type)};
static_assert(fixed_staging_bytes == 131072u);

#if __has_cpp_attribute(__gnu__::__noinline__)
#define FAST_IO_FINITE_NOINLINE [[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
#define FAST_IO_FINITE_NOINLINE [[msvc::noinline]]
#else
#define FAST_IO_FINITE_NOINLINE
#endif

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

[[nodiscard]] inline constexpr char_type typed_value_at(
	::std::size_t index) noexcept
{
	auto const value{static_cast<::std::uint_least32_t>(
		(index * UINT32_C(40503) + UINT32_C(127)) & UINT32_C(0xffff))};
	return static_cast<char_type>(value);
}

[[nodiscard]] inline constexpr ::std::byte byte_value_at(
	::std::size_t index) noexcept
{
	return static_cast<::std::byte>(
		(index * ::std::size_t{151u} + ::std::size_t{73u}) &
		::std::size_t{0xffu});
}

struct input_state
{
	::std::array<char_type, maximum_request> typed_storage{};
	::std::array<::std::byte, maximum_request> byte_storage{};
	::std::size_t position{};
	::std::size_t limit{};
	::std::size_t primitive_calls{};
	::std::size_t normalizations{};
};

struct output_state
{
	::std::array<char_type, maximum_request> typed_storage{};
	::std::array<::std::byte, maximum_request> byte_storage{};
	::std::size_t position{};
	::std::size_t primitive_calls{};
	::std::size_t normalizations{};
};

struct input_ref
{
	using input_char_type = char_type;
	input_state *state{};
};

struct output_ref
{
	using output_char_type = char_type;
	output_state *state{};
};

[[nodiscard]] inline input_ref input_stream_ref_define(
	input_state &state) noexcept
{
	++state.normalizations;
	return {__builtin_addressof(state)};
}

[[nodiscard]] inline output_ref output_stream_ref_define(
	output_state &state) noexcept
{
	++state.normalizations;
	return {__builtin_addressof(state)};
}

inline void read_all_underflow_define(
	input_ref input, char_type *first, char_type *last) noexcept
{
	++input.state->primitive_calls;
	auto const count{static_cast<::std::size_t>(last - first)};
	require(count <= input.state->limit - input.state->position);
	::std::memcpy(first, input.state->typed_storage.data() + input.state->position,
		count * sizeof(char_type));
	input.state->position += count;
}

[[nodiscard]] inline char_type *read_some_underflow_define(
	input_ref input, char_type *first, char_type *last) noexcept
{
	++input.state->primitive_calls;
	auto count{static_cast<::std::size_t>(last - first)};
	auto const remaining{input.state->limit - input.state->position};
	if (remaining < count)
	{
		count = remaining;
	}
	::std::memcpy(first, input.state->typed_storage.data() + input.state->position,
		count * sizeof(char_type));
	input.state->position += count;
	return first + count;
}

inline void read_all_bytes_underflow_define(
	input_ref input, ::std::byte *first, ::std::byte *last) noexcept
{
	++input.state->primitive_calls;
	auto const count{static_cast<::std::size_t>(last - first)};
	require(count <= input.state->limit - input.state->position);
	::std::memcpy(first, input.state->byte_storage.data() + input.state->position,
		count);
	input.state->position += count;
}

[[nodiscard]] inline ::std::byte *read_some_bytes_underflow_define(
	input_ref input, ::std::byte *first, ::std::byte *last) noexcept
{
	++input.state->primitive_calls;
	auto count{static_cast<::std::size_t>(last - first)};
	auto const remaining{input.state->limit - input.state->position};
	if (remaining < count)
	{
		count = remaining;
	}
	::std::memcpy(first, input.state->byte_storage.data() + input.state->position,
		count);
	input.state->position += count;
	return first + count;
}

inline void write_all_overflow_define(
	output_ref output, char_type const *first, char_type const *last) noexcept
{
	++output.state->primitive_calls;
	auto const count{static_cast<::std::size_t>(last - first)};
	require(count <= output.state->typed_storage.size() - output.state->position);
	::std::memcpy(output.state->typed_storage.data() + output.state->position,
		first, count * sizeof(char_type));
	output.state->position += count;
}

inline void write_all_bytes_overflow_define(
	output_ref output, ::std::byte const *first,
	::std::byte const *last) noexcept
{
	++output.state->primitive_calls;
	auto const count{static_cast<::std::size_t>(last - first)};
	require(count <= output.state->byte_storage.size() - output.state->position);
	::std::memcpy(output.state->byte_storage.data() + output.state->position,
		first, count);
	output.state->position += count;
}

template <typename value_type>
inline void opaque_escape(value_type const *pointer) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
	/*
	The memory clobber makes the complete destination externally observable at
	each operation boundary.  It preserves the actual staging/read/write path
	without adding a data-dependent oracle to the timed interval.
	*/
	__asm__ __volatile__("" : : "r"(pointer) : "memory");
#else
	(void)pointer;
#endif
}

[[nodiscard]] inline ::fast_io::uintfpos_t invoke_selected(
	output_state &output, input_state &input,
	::fast_io::uintfpos_t request) noexcept
{
	::fast_io::uintfpos_t reported{request};
	if constexpr (selected_kind == 0u)
	{
		::fast_io::operations::transmit_all(output, input, request);
	}
	else if constexpr (selected_kind == 1u)
	{
		reported = ::fast_io::operations::transmit_some(output, input, request);
	}
	else if constexpr (selected_kind == 2u)
	{
		::fast_io::operations::transmit_bytes_all(output, input, request);
	}
	else
	{
		reported = ::fast_io::operations::transmit_bytes_some(
			output, input, request);
	}
	return reported;
}

FAST_IO_FINITE_NOINLINE [[nodiscard]] ::std::uint_least64_t run_once(
	input_state &input, output_state &output,
	::std::size_t request) noexcept
{
	input.position = 0u;
	input.limit = request;
	output.position = 0u;
	auto const reported{invoke_selected(
		output, input, static_cast<::fast_io::uintfpos_t>(request))};
	if constexpr (selected_bytes)
	{
		opaque_escape(output.byte_storage.data());
		auto const first{::std::to_integer<unsigned>(output.byte_storage[0])};
		auto const last{
			::std::to_integer<unsigned>(output.byte_storage[request - 1u])};
		return (static_cast<::std::uint_least64_t>(reported) << 32u) ^
			(static_cast<::std::uint_least64_t>(output.position) << 16u) ^
			(static_cast<::std::uint_least64_t>(first) << 8u) ^ last;
	}
	else
	{
		opaque_escape(output.typed_storage.data());
		auto const first{static_cast<::std::uint_least32_t>(
			output.typed_storage[0])};
		auto const last{static_cast<::std::uint_least32_t>(
			output.typed_storage[request - 1u])};
		return (static_cast<::std::uint_least64_t>(reported) << 32u) ^
			(static_cast<::std::uint_least64_t>(output.position) << 16u) ^
			(static_cast<::std::uint_least64_t>(first) << 8u) ^ last;
	}
}

[[nodiscard]] inline constexpr ::std::size_t expected_allocation_bytes(
	::std::size_t request) noexcept
{
	if constexpr (selected_min_policy)
	{
		return selected_bytes ? request : request * sizeof(char_type);
	}
	else
	{
		return fixed_staging_bytes;
	}
}

inline void initialize_fixture(input_state &input) noexcept
{
	for (::std::size_t index{}; index != maximum_request; ++index)
	{
		input.typed_storage[index] = typed_value_at(index);
		input.byte_storage[index] = byte_value_at(index);
	}
}

inline void validate_once(
	input_state &input, output_state &output,
	::std::size_t request) noexcept
{
	input.position = input.limit = input.primitive_calls = input.normalizations = 0u;
	output.position = output.primitive_calls = output.normalizations = 0u;
	::fast_io::custom_thread_local_allocator::reset_observation();
	auto const signature{run_once(input, output, request)};
	require(signature != 0u);
	require(input.position == request && output.position == request);
	require(input.primitive_calls == 1u && output.primitive_calls == 1u);
	require(input.normalizations == 1u && output.normalizations == 1u);
	require(::fast_io::custom_thread_local_allocator::allocation_calls == 1u);
	require(::fast_io::custom_thread_local_allocator::deallocation_calls == 1u);
	require(
		::fast_io::custom_thread_local_allocator::last_allocation_bytes ==
		expected_allocation_bytes(request));
	if constexpr (selected_bytes)
	{
		for (::std::size_t index{}; index != request; ++index)
		{
			require(output.byte_storage[index] == byte_value_at(index));
		}
	}
	else
	{
		for (::std::size_t index{}; index != request; ++index)
		{
			require(output.typed_storage[index] == typed_value_at(index));
		}
	}
}

[[nodiscard]] inline ::std::size_t parse_positive(
	char const *text) noexcept
{
	if (text == nullptr || *text == '\0')
	{
		return 0u;
	}
	::std::size_t value{};
	for (; *text != '\0'; ++text)
	{
		auto const digit{
			static_cast<unsigned char>(*text) - static_cast<unsigned>('0')};
		if (digit > 9u ||
			value > (::std::numeric_limits<::std::size_t>::max() - digit) / 10u)
		{
			return 0u;
		}
		value = value * 10u + digit;
	}
	return value;
}

#if defined(__unix__) || defined(__APPLE__)
extern "C" void deadline_handler(int) noexcept
{
	::_Exit(124);
}

inline void arm_deadline() noexcept
{
	::std::signal(SIGALRM, deadline_handler);
	::itimerval timer{};
	timer.it_value.tv_usec = 800000;
	require(::setitimer(ITIMER_REAL, __builtin_addressof(timer), nullptr) == 0);
}
#else
inline void arm_deadline() noexcept
{}
#endif

} // namespace fast_io_finite_staging_ab

int main(int argc, char **argv)
{
	using namespace ::fast_io_finite_staging_ab;
	if (argc != 3)
	{
		::std::fputs("usage: finite_staging_ab REQUEST ITERATIONS\n", stderr);
		return 64;
	}
	auto const request{parse_positive(argv[1])};
	auto const iterations{parse_positive(argv[2])};
	if (request == 0u || request > maximum_request || iterations == 0u)
	{
		::std::fputs("REQUEST must be 1..4097 and ITERATIONS must be positive\n",
			stderr);
		return 64;
	}
	arm_deadline();
	input_state input{};
	output_state output{};
	initialize_fixture(input);
	validate_once(input, output, request);

	input.position = input.limit = input.primitive_calls = input.normalizations = 0u;
	output.position = output.primitive_calls = output.normalizations = 0u;
	::fast_io::custom_thread_local_allocator::reset_observation();
	::std::uint_least64_t checksum{UINT64_C(14695981039346656037)};
	auto const start{::std::chrono::steady_clock::now()};
	for (::std::size_t iteration{}; iteration != iterations; ++iteration)
	{
		checksum = (checksum ^ run_once(input, output, request)) *
			UINT64_C(1099511628211);
	}
	auto const finish{::std::chrono::steady_clock::now()};
	auto const elapsed_ns{::std::chrono::duration_cast<::std::chrono::nanoseconds>(
		finish - start).count()};

	/*
	For a nonzero finite request each generic CPO performs one allocation, one
	primitive read, and one committing write in this full-progress fixture.  The
	post-timing proof detects accidental hoisting, elision, extra EOF probing, or
	partial publication without charging those checks to the measured interval.
	*/
	require(input.position == request && output.position == request);
	require(input.primitive_calls == iterations);
	require(output.primitive_calls == iterations);
	require(input.normalizations == iterations);
	require(output.normalizations == iterations);
	require(::fast_io::custom_thread_local_allocator::allocation_calls == iterations);
	require(::fast_io::custom_thread_local_allocator::deallocation_calls == iterations);
	auto const expected_bytes{expected_allocation_bytes(request)};
	require(::fast_io::custom_thread_local_allocator::last_allocation_bytes ==
		expected_bytes);
	require(::fast_io::custom_thread_local_allocator::allocation_bytes ==
		expected_bytes * iterations);

	::std::printf(
		"request=%zu iterations=%zu elapsed_ns=%lld ns_per_op=%.6f "
		"checksum=%llu alloc_calls=%zu alloc_bytes=%zu last_alloc=%zu\n",
		request, iterations, static_cast<long long>(elapsed_ns),
		static_cast<double>(elapsed_ns) / static_cast<double>(iterations),
		static_cast<unsigned long long>(checksum),
		::fast_io::custom_thread_local_allocator::allocation_calls,
		::fast_io::custom_thread_local_allocator::allocation_bytes,
		::fast_io::custom_thread_local_allocator::last_allocation_bytes);
}
