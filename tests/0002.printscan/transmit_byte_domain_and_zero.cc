#include <array>
#include <cstddef>
#include <cstdlib>

namespace fast_io
{

/*
This translation unit selects an observing thread-local allocator before the
library headers define their native allocator aliases. The recorded byte extent
turns staging capacity into a correctness-testable postcondition without
changing the transmit implementation's allocator type or exposing test hooks in
production headers.
*/
class custom_thread_local_allocator
{
public:
	inline static ::std::size_t allocation_calls{};
	inline static ::std::size_t deallocation_calls{};
	inline static ::std::size_t last_allocation_bytes{};

	[[nodiscard]] static inline void *allocate(::std::size_t bytes) noexcept
	{
		++allocation_calls;
		last_allocation_bytes = bytes;
		void *const result{::std::malloc(bytes == 0u ? 1u : bytes)};
		if (result == nullptr)
		{
			::std::abort();
		}
		return result;
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
		last_allocation_bytes = 0u;
	}
};

} // namespace fast_io

#define FAST_IO_USE_CUSTOM_THREAD_LOCAL_ALLOCATOR
#include <fast_io_core.h>

namespace transmit_byte_domain_and_zero_test
{

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

inline constexpr ::std::array<::std::byte, 7u> byte_payload{
	::std::byte{0x11}, ::std::byte{0x22}, ::std::byte{0x33}, ::std::byte{0x44},
	::std::byte{0x55}, ::std::byte{0x66}, ::std::byte{0x77}};

struct byte_input_owner
{
	::std::size_t size{};
	::std::size_t chunk_size{1u};
	::std::size_t position{};
	::std::size_t normalizations{};
	::std::size_t byte_reads{};
};

struct byte_input_ref
{
	// A wide advertised character domain makes typed and byte progress observably
	// different: an odd terminal byte is valid byte input but not a complete char16_t.
	using input_char_type = char16_t;
	byte_input_owner *owner{};
};

inline byte_input_ref input_stream_ref_define(byte_input_owner &owner) noexcept
{
	++owner.normalizations;
	return {__builtin_addressof(owner)};
}

inline ::std::byte *read_some_bytes_underflow_define(
	byte_input_ref ref, ::std::byte *first, ::std::byte *last) noexcept
{
	++ref.owner->byte_reads;
	::std::size_t count{static_cast<::std::size_t>(last - first)};
	::std::size_t const remaining{ref.owner->size - ref.owner->position};
	if (remaining < count)
	{
		count = remaining;
	}
	if (ref.owner->chunk_size < count)
	{
		count = ref.owner->chunk_size;
	}
	for (::std::size_t index{}; index != count; ++index)
	{
		first[index] = byte_payload[ref.owner->position + index];
	}
	ref.owner->position += count;
	return first + count;
}

struct byte_output_owner
{
	::std::array<::std::byte, byte_payload.size()> storage{};
	::std::size_t position{};
	::std::size_t normalizations{};
	::std::size_t byte_writes{};
};

struct byte_output_ref
{
	// The destination deliberately has the same wide character domain and exposes
	// no typed output primitive. A byte transmit must not require character units.
	using output_char_type = char16_t;
	byte_output_owner *owner{};
};

inline byte_output_ref output_stream_ref_define(byte_output_owner &owner) noexcept
{
	++owner.normalizations;
	return {__builtin_addressof(owner)};
}

inline void write_all_bytes_overflow_define(
	byte_output_ref ref, ::std::byte const *first, ::std::byte const *last) noexcept
{
	++ref.owner->byte_writes;
	::std::size_t const count{static_cast<::std::size_t>(last - first)};
	require(count != 0u && count <= ref.owner->storage.size() - ref.owner->position);
	for (::std::size_t index{}; index != count; ++index)
	{
		ref.owner->storage[ref.owner->position + index] = first[index];
	}
	ref.owner->position += count;
}

using namespace ::fast_io::operations::decay::defines;

// These assertions are the structural half of the regression proof. The source
// and destination are byte-capable but intentionally not typed-capable; the odd
// payload cases below are the semantic half that rejects typed-unit synthesis.
static_assert(has_read_some_bytes_underflow_define<byte_input_ref>);
static_assert(!has_read_some_underflow_define<byte_input_ref>);
static_assert(bytes_readable<byte_input_ref>);
static_assert(!readable<byte_input_ref>);
static_assert(has_write_all_bytes_overflow_define<byte_output_ref>);
static_assert(!has_write_all_overflow_define<byte_output_ref>);
static_assert(bytes_writable<byte_output_ref>);
static_assert(!writable<byte_output_ref>);

inline void test_byte_until_eof_matrix() noexcept
{
	constexpr ::std::array<::std::size_t, 4u> lengths{0u, 1u, 3u, 7u};
	constexpr ::std::array<::std::size_t, 4u> chunks{1u, 2u, 3u, 5u};

	for (auto const length : lengths)
	{
		for (auto const chunk : chunks)
		{
			byte_input_owner input{length, chunk};
			byte_output_owner output{};
			auto const result{
				::fast_io::operations::transmit_bytes_until_eof(output, input)};

			require(result.transmitted == length);
			require(input.position == length && output.position == length);
			require(input.normalizations == 1u && output.normalizations == 1u);
			for (::std::size_t index{}; index != length; ++index)
			{
				require(output.storage[index] == byte_payload[index]);
			}

			// One byte-read CPO call produces each bounded chunk, followed by one
			// empty call that proves logical EOF. Every nonempty chunk is committed
			// by exactly one all-bytes write CPO.
			::std::size_t const nonempty_chunks{
				length == 0u ? 0u : (length + chunk - 1u) / chunk};
			require(input.byte_reads == nonempty_chunks + 1u);
			require(output.byte_writes == nonempty_chunks);
		}
	}
}

inline constexpr ::std::size_t finite_capacity{16u};

[[nodiscard]] inline constexpr char16_t typed_payload_at(
	::std::size_t position) noexcept
{
	return static_cast<char16_t>(0x120u + position);
}

[[nodiscard]] inline constexpr ::std::byte byte_payload_at(
	::std::size_t position) noexcept
{
	return static_cast<::std::byte>(0x40u + position);
}

struct finite_input_owner
{
	::std::size_t available{};
	::std::size_t chunk_size{1u};
	::std::size_t typed_position{};
	::std::size_t byte_position{};
	::std::size_t normalizations{};
	::std::size_t typed_primitives{};
	::std::size_t byte_primitives{};
};

struct finite_input_ref
{
	using input_char_type = char16_t;
	finite_input_owner *owner{};
};

inline finite_input_ref input_stream_ref_define(
	finite_input_owner &owner) noexcept
{
	++owner.normalizations;
	return {__builtin_addressof(owner)};
}

inline void read_all_underflow_define(
	finite_input_ref ref, char16_t *first, char16_t *last) noexcept
{
	++ref.owner->typed_primitives;
	auto const count{static_cast<::std::size_t>(last - first)};
	require(count <= ref.owner->available - ref.owner->typed_position);
	for (::std::size_t index{}; index != count; ++index)
	{
		first[index] = typed_payload_at(ref.owner->typed_position + index);
	}
	ref.owner->typed_position += count;
}

inline char16_t *read_some_underflow_define(
	finite_input_ref ref, char16_t *first, char16_t *last) noexcept
{
	++ref.owner->typed_primitives;
	auto count{static_cast<::std::size_t>(last - first)};
	auto const remaining{ref.owner->available - ref.owner->typed_position};
	if (ref.owner->chunk_size < count)
	{
		count = ref.owner->chunk_size;
	}
	if (remaining < count)
	{
		count = remaining;
	}
	for (::std::size_t index{}; index != count; ++index)
	{
		first[index] = typed_payload_at(ref.owner->typed_position + index);
	}
	ref.owner->typed_position += count;
	return first + count;
}

inline void read_all_bytes_underflow_define(
	finite_input_ref ref, ::std::byte *first, ::std::byte *last) noexcept
{
	++ref.owner->byte_primitives;
	auto const count{static_cast<::std::size_t>(last - first)};
	require(count <= ref.owner->available - ref.owner->byte_position);
	for (::std::size_t index{}; index != count; ++index)
	{
		first[index] = byte_payload_at(ref.owner->byte_position + index);
	}
	ref.owner->byte_position += count;
}

inline ::std::byte *read_some_bytes_underflow_define(
	finite_input_ref ref, ::std::byte *first, ::std::byte *last) noexcept
{
	++ref.owner->byte_primitives;
	auto count{static_cast<::std::size_t>(last - first)};
	auto const remaining{ref.owner->available - ref.owner->byte_position};
	if (ref.owner->chunk_size < count)
	{
		count = ref.owner->chunk_size;
	}
	if (remaining < count)
	{
		count = remaining;
	}
	for (::std::size_t index{}; index != count; ++index)
	{
		first[index] = byte_payload_at(ref.owner->byte_position + index);
	}
	ref.owner->byte_position += count;
	return first + count;
}

struct finite_output_owner
{
	::std::array<char16_t, finite_capacity> typed_storage{};
	::std::array<::std::byte, finite_capacity> byte_storage{};
	::std::size_t typed_position{};
	::std::size_t byte_position{};
	::std::size_t normalizations{};
	::std::size_t typed_primitives{};
	::std::size_t byte_primitives{};
};

struct finite_output_ref
{
	using output_char_type = char16_t;
	finite_output_owner *owner{};
};

inline finite_output_ref output_stream_ref_define(
	finite_output_owner &owner) noexcept
{
	++owner.normalizations;
	return {__builtin_addressof(owner)};
}

inline void write_all_overflow_define(
	finite_output_ref ref, char16_t const *first,
	char16_t const *last) noexcept
{
	++ref.owner->typed_primitives;
	auto const count{static_cast<::std::size_t>(last - first)};
	require(count <= ref.owner->typed_storage.size() - ref.owner->typed_position);
	for (::std::size_t index{}; index != count; ++index)
	{
		ref.owner->typed_storage[ref.owner->typed_position + index] = first[index];
	}
	ref.owner->typed_position += count;
}

inline void write_all_bytes_overflow_define(
	finite_output_ref ref, ::std::byte const *first,
	::std::byte const *last) noexcept
{
	++ref.owner->byte_primitives;
	auto const count{static_cast<::std::size_t>(last - first)};
	require(count <= ref.owner->byte_storage.size() - ref.owner->byte_position);
	for (::std::size_t index{}; index != count; ++index)
	{
		ref.owner->byte_storage[ref.owner->byte_position + index] = first[index];
	}
	ref.owner->byte_position += count;
}

inline void require_single_staging_allocation(
	::std::size_t expected_bytes) noexcept
{
	require(::fast_io::custom_thread_local_allocator::allocation_calls == 1u);
	require(::fast_io::custom_thread_local_allocator::deallocation_calls == 1u);
	require(
		::fast_io::custom_thread_local_allocator::last_allocation_bytes ==
		expected_bytes);
}

inline void require_typed_payload(
	finite_output_owner const &output, ::std::size_t count) noexcept
{
	require(output.typed_position == count);
	for (::std::size_t index{}; index != count; ++index)
	{
		require(output.typed_storage[index] == typed_payload_at(index));
	}
}

inline void require_byte_payload(
	finite_output_owner const &output, ::std::size_t count) noexcept
{
	require(output.byte_position == count);
	for (::std::size_t index{}; index != count; ++index)
	{
		require(output.byte_storage[index] == byte_payload_at(index));
	}
}

inline void test_finite_request_bounds_staging() noexcept
{
	constexpr ::std::array<::std::size_t, 3u> requests{1u, 3u, 7u};
	for (auto const request : requests)
	{
		{
			finite_input_owner input{request, 2u};
			finite_output_owner output{};
			::fast_io::custom_thread_local_allocator::reset_observation();
			::fast_io::operations::transmit_all(output, input, request);
			require_single_staging_allocation(request * sizeof(char16_t));
			require(input.typed_position == request &&
				input.normalizations == 1u && output.normalizations == 1u);
			require_typed_payload(output, request);
		}

		{
			auto const available{request == 7u ? 3u : request};
			finite_input_owner input{available, 2u};
			finite_output_owner output{};
			::fast_io::custom_thread_local_allocator::reset_observation();
			auto const transferred{
				::fast_io::operations::transmit_some(output, input, request)};
			require(transferred == available);
			/*
			Partial availability changes reported progress, not the allocation
			bound: the implementation knows the remaining request before it can
			observe EOF. The staging extent must therefore remain `request`
			elements while each primitive result controls committed progress.
			*/
			require_single_staging_allocation(request * sizeof(char16_t));
			require(input.typed_position == available);
			require_typed_payload(output, available);
		}

		{
			finite_input_owner input{request, 2u};
			finite_output_owner output{};
			::fast_io::custom_thread_local_allocator::reset_observation();
			::fast_io::operations::transmit_bytes_all(output, input, request);
			/*
			The byte operation allocates exactly `request` bytes even though both
			observers advertise char16_t. This rejects accidental reuse of typed
			element counts or multiplication by sizeof(input_char_type).
			*/
			require_single_staging_allocation(request);
			require(input.byte_position == request);
			require_byte_payload(output, request);
		}

		{
			auto const available{request == 7u ? 3u : request};
			finite_input_owner input{available, 2u};
			finite_output_owner output{};
			::fast_io::custom_thread_local_allocator::reset_observation();
			auto const transferred{
				::fast_io::operations::transmit_bytes_some(output, input, request)};
			require(transferred == available);
			require_single_staging_allocation(request);
			require(input.byte_position == available);
			require_byte_payload(output, available);
		}
	}
}

struct lock_state
{
	bool locked{};
	::std::size_t locks{};
	::std::size_t unlocks{};
};

struct mutex_proxy
{
	lock_state *state{};

	inline void lock() const noexcept
	{
		require(state != nullptr && !state->locked);
		state->locked = true;
		++state->locks;
	}

	inline void unlock() const noexcept
	{
		require(state != nullptr && state->locked);
		state->locked = false;
		++state->unlocks;
	}
};

struct zero_input_owner;

struct zero_locked_input_ref
{
	using input_char_type = char16_t;
	zero_input_owner *owner{};
};

struct zero_unlocked_input_ref
{
	using input_char_type = char16_t;
	zero_input_owner *owner{};
};

struct zero_input_owner
{
	lock_state lock{};
	::std::size_t normalizations{};
	::std::size_t typed_primitives{};
	::std::size_t byte_primitives{};
};

inline zero_locked_input_ref input_stream_ref_define(zero_input_owner &owner) noexcept
{
	++owner.normalizations;
	return {__builtin_addressof(owner)};
}

inline constexpr mutex_proxy input_stream_mutex_ref_define(zero_locked_input_ref ref) noexcept
{
	return {__builtin_addressof(ref.owner->lock)};
}

inline constexpr zero_unlocked_input_ref input_stream_unlocked_ref_define(zero_locked_input_ref ref) noexcept
{
	return {ref.owner};
}

inline char16_t *read_some_underflow_define(
	zero_unlocked_input_ref ref, char16_t *first, char16_t *) noexcept
{
	require(ref.owner->lock.locked);
	++ref.owner->typed_primitives;
	return first;
}

inline void read_all_underflow_define(
	zero_unlocked_input_ref ref, char16_t *, char16_t *) noexcept
{
	require(ref.owner->lock.locked);
	++ref.owner->typed_primitives;
}

inline ::std::byte *read_some_bytes_underflow_define(
	zero_unlocked_input_ref ref, ::std::byte *first, ::std::byte *) noexcept
{
	require(ref.owner->lock.locked);
	++ref.owner->byte_primitives;
	return first;
}

inline void read_all_bytes_underflow_define(
	zero_unlocked_input_ref ref, ::std::byte *, ::std::byte *) noexcept
{
	require(ref.owner->lock.locked);
	++ref.owner->byte_primitives;
}

struct zero_output_owner;

struct zero_locked_output_ref
{
	using output_char_type = char16_t;
	zero_output_owner *owner{};
};

struct zero_unlocked_output_ref
{
	using output_char_type = char16_t;
	zero_output_owner *owner{};
};

struct zero_output_owner
{
	lock_state lock{};
	::std::size_t normalizations{};
	::std::size_t typed_primitives{};
	::std::size_t byte_primitives{};
};

inline zero_locked_output_ref output_stream_ref_define(zero_output_owner &owner) noexcept
{
	++owner.normalizations;
	return {__builtin_addressof(owner)};
}

inline constexpr mutex_proxy output_stream_mutex_ref_define(zero_locked_output_ref ref) noexcept
{
	return {__builtin_addressof(ref.owner->lock)};
}

inline constexpr zero_unlocked_output_ref output_stream_unlocked_ref_define(zero_locked_output_ref ref) noexcept
{
	return {ref.owner};
}

inline void write_all_overflow_define(
	zero_unlocked_output_ref ref, char16_t const *, char16_t const *) noexcept
{
	require(ref.owner->lock.locked);
	++ref.owner->typed_primitives;
}

inline void write_all_bytes_overflow_define(
	zero_unlocked_output_ref ref, ::std::byte const *, ::std::byte const *) noexcept
{
	require(ref.owner->lock.locked);
	++ref.owner->byte_primitives;
}

static_assert(has_complete_input_stream_mutex_protocol<zero_locked_input_ref>);
static_assert(has_complete_output_stream_mutex_protocol<zero_locked_output_ref>);
static_assert(has_complete_transmit_mutex_protocols<zero_locked_output_ref, zero_locked_input_ref>);

inline void test_zero_count_operation_boundary() noexcept
{
	zero_input_owner input{};
	zero_output_owner output{};

	::fast_io::custom_thread_local_allocator::reset_observation();
	::fast_io::operations::transmit_all(output, input, 0u);
	require(::fast_io::operations::transmit_some(output, input, 0u) == 0u);
	::fast_io::operations::transmit_bytes_all(output, input, 0u);
	require(::fast_io::operations::transmit_bytes_some(output, input, 0u) == 0u);

	// Zero payload is the data-plane identity, not permission to erase the public
	// operation boundary. Each facade must still normalize both streams once and
	// hold each directional mutex for the complete logical operation, while no
	// typed or byte primitive is invoked.
	require(input.normalizations == 4u && output.normalizations == 4u);
	require(input.lock.locks == 4u && input.lock.unlocks == 4u && !input.lock.locked);
	require(output.lock.locks == 4u && output.lock.unlocks == 4u && !output.lock.locked);
	require(input.typed_primitives == 0u && input.byte_primitives == 0u);
	require(output.typed_primitives == 0u && output.byte_primitives == 0u);
	require(::fast_io::custom_thread_local_allocator::allocation_calls == 0u);
	require(::fast_io::custom_thread_local_allocator::deallocation_calls == 0u);
}

} // namespace transmit_byte_domain_and_zero_test

int main()
{
	::transmit_byte_domain_and_zero_test::test_byte_until_eof_matrix();
	::transmit_byte_domain_and_zero_test::test_finite_request_bounds_staging();
	::transmit_byte_domain_and_zero_test::test_zero_count_operation_boundary();
}
