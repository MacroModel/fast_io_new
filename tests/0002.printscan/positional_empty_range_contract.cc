#include <array>
#include <cstddef>
#include <cstdlib>
#include <type_traits>

#include <fast_io_core.h>

namespace positional_empty_range_contract
{

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

struct call_record
{
	void const *first{};
	void const *last{};
	::std::size_t size{};
	::fast_io::intfpos_t offset{};
};

struct operation_state
{
	::std::array<call_record, 32u> calls{};
	::std::size_t call_count{};
	::fast_io::intfpos_t current{};
	::std::size_t seek_calls{};
	bool locked{};
	bool require_lock{};
	::std::size_t lock_calls{};
	::std::size_t unlock_calls{};
};

inline void reset(operation_state &state, ::fast_io::intfpos_t current = 0) noexcept
{
	state.call_count = 0u;
	state.current = current;
	state.seek_calls = 0u;
	state.locked = false;
	state.require_lock = false;
	state.lock_calls = 0u;
	state.unlock_calls = 0u;
}

template <typename element_type>
inline void record_range(operation_state &state, element_type const *first,
						 element_type const *last, ::fast_io::intfpos_t offset) noexcept
{
	require(!state.require_lock || state.locked);
	require(state.call_count != state.calls.size());
	::std::size_t size{};
	if (first != last)
	{
		// A nonempty primitive range retains the usual one-array proof. Equality is
		// handled first because `{nullptr,nullptr}` is a valid mathematical empty
		// range but does not authorize pointer subtraction.
		require(first != nullptr && last != nullptr);
		auto const difference{last - first};
		require(difference > 0);
		size = static_cast<::std::size_t>(difference);
	}
	state.calls[state.call_count++] = {first, last, size, offset};
}

struct byte_io_ref
{
	using input_char_type = char;
	using output_char_type = char;
	operation_state *state{};
};

inline constexpr byte_io_ref input_stream_ref_define(byte_io_ref stream) noexcept
{
	return stream;
}

inline constexpr byte_io_ref output_stream_ref_define(byte_io_ref stream) noexcept
{
	return stream;
}

inline constexpr ::std::true_type stream_ref_value_transport_safe_define(
	::fast_io::io_type_t<byte_io_ref>) noexcept
{
	// Copies contain only a pointer to the single external state block, so every
	// primitive observes the same call trace, lock state, and positional state.
	return {};
}

inline constexpr ::std::true_type abi_value_transport_force_direct(
	::fast_io::io_type_t<byte_io_ref>) noexcept
{
	// This one-pointer fixture deliberately selects the direct value path on all
	// test targets; its exact layout is scalar-equivalent under the test ABI.
	return {};
}

inline ::std::byte *pread_some_bytes_underflow_define(
	byte_io_ref stream, ::std::byte *first, ::std::byte *last,
	::fast_io::intfpos_t offset) noexcept
{
	record_range(*stream.state, first, last, offset);
	for (auto current{first}; current != last; ++current)
	{
		*current = static_cast<::std::byte>('i');
	}
	return last;
}

inline ::std::byte const *pwrite_some_bytes_overflow_define(
	byte_io_ref stream, ::std::byte const *first, ::std::byte const *last,
	::fast_io::intfpos_t offset) noexcept
{
	record_range(*stream.state, first, last, offset);
	return last;
}

static_assert(::fast_io::details::abi_value_direct_pread_some_bytes<byte_io_ref>);
static_assert(::fast_io::details::abi_value_direct_pwrite_some_bytes<byte_io_ref>);

struct mutex_proxy
{
	operation_state *state{};

	inline void lock() const noexcept
	{
		require(!state->locked);
		state->locked = true;
		++state->lock_calls;
	}

	inline void unlock() const noexcept
	{
		require(state->locked);
		state->locked = false;
		++state->unlock_calls;
	}
};

struct locked_io_ref
{
	using input_char_type = char;
	using output_char_type = char;
	operation_state *state{};
};

inline constexpr locked_io_ref input_stream_ref_define(locked_io_ref stream) noexcept
{
	return stream;
}

inline constexpr locked_io_ref output_stream_ref_define(locked_io_ref stream) noexcept
{
	return stream;
}

inline constexpr mutex_proxy io_stream_mutex_ref_define(locked_io_ref stream) noexcept
{
	return {stream.state};
}

inline constexpr byte_io_ref io_stream_unlocked_ref_define(locked_io_ref stream) noexcept
{
	return {stream.state};
}

template <bool byte_mode>
struct seek_io_ref
{
	using input_char_type = char;
	using output_char_type = char;
	operation_state *state{};
};

template <bool byte_mode>
inline constexpr seek_io_ref<byte_mode> input_stream_ref_define(
	seek_io_ref<byte_mode> stream) noexcept
{
	return stream;
}

template <bool byte_mode>
inline constexpr seek_io_ref<byte_mode> output_stream_ref_define(
	seek_io_ref<byte_mode> stream) noexcept
{
	return stream;
}

inline ::fast_io::intfpos_t update_seek(
	operation_state &state, ::fast_io::intfpos_t offset,
	::fast_io::seekdir direction) noexcept
{
	++state.seek_calls;
	if (direction == ::fast_io::seekdir::beg)
	{
		state.current = offset;
	}
	else
	{
		require(direction == ::fast_io::seekdir::cur);
		state.current += offset;
	}
	return state.current;
}

inline ::fast_io::intfpos_t input_stream_seek_define(
	seek_io_ref<false> stream, ::fast_io::intfpos_t offset,
	::fast_io::seekdir direction) noexcept
{
	return update_seek(*stream.state, offset, direction);
}

inline ::fast_io::intfpos_t output_stream_seek_define(
	seek_io_ref<false> stream, ::fast_io::intfpos_t offset,
	::fast_io::seekdir direction) noexcept
{
	return update_seek(*stream.state, offset, direction);
}

inline ::fast_io::intfpos_t input_stream_seek_bytes_define(
	seek_io_ref<true> stream, ::fast_io::intfpos_t offset,
	::fast_io::seekdir direction) noexcept
{
	return update_seek(*stream.state, offset, direction);
}

inline ::fast_io::intfpos_t output_stream_seek_bytes_define(
	seek_io_ref<true> stream, ::fast_io::intfpos_t offset,
	::fast_io::seekdir direction) noexcept
{
	return update_seek(*stream.state, offset, direction);
}

inline char *read_some_underflow_define(
	seek_io_ref<false> stream, char *first, char *last) noexcept
{
	record_range(*stream.state, first, last, stream.state->current);
	return last;
}

inline char const *write_some_overflow_define(
	seek_io_ref<false> stream, char const *first, char const *last) noexcept
{
	record_range(*stream.state, first, last, stream.state->current);
	return last;
}

inline ::std::byte *read_some_bytes_underflow_define(
	seek_io_ref<true> stream, ::std::byte *first, ::std::byte *last) noexcept
{
	record_range(*stream.state, first, last, stream.state->current);
	return last;
}

inline ::std::byte const *write_some_bytes_overflow_define(
	seek_io_ref<true> stream, ::std::byte const *first,
	::std::byte const *last) noexcept
{
	record_range(*stream.state, first, last, stream.state->current);
	return last;
}

inline void require_one_null_call(
	operation_state const &state, ::fast_io::intfpos_t offset) noexcept
{
	require(state.call_count == 1u);
	auto const &call{state.calls[0]};
	require(call.first == nullptr && call.last == nullptr);
	require(call.size == 0u && call.offset == offset);
}

inline void test_scalar_matrix()
{
	operation_state state{};
	byte_io_ref stream{__builtin_addressof(state)};
	char *typed_input{};
	char const *typed_output{};
	::std::byte *byte_input{};
	::std::byte const *byte_output{};
	constexpr ::fast_io::intfpos_t offset{41};

	reset(state);
	require(::fast_io::operations::pread_some(
				stream, typed_input, typed_input, offset) == typed_input);
	require_one_null_call(state, offset);
	reset(state);
	::fast_io::operations::pread_all(stream, typed_input, typed_input, offset);
	require_one_null_call(state, offset);
	reset(state);
	require(::fast_io::operations::pread_some_bytes(
				stream, byte_input, byte_input, offset) == byte_input);
	require_one_null_call(state, offset);
	reset(state);
	::fast_io::operations::pread_all_bytes(stream, byte_input, byte_input, offset);
	require_one_null_call(state, offset);

	reset(state);
	require(::fast_io::operations::pwrite_some(
				stream, typed_output, typed_output, offset) == typed_output);
	require_one_null_call(state, offset);
	reset(state);
	::fast_io::operations::pwrite_all(stream, typed_output, typed_output, offset);
	require_one_null_call(state, offset);
	reset(state);
	require(::fast_io::operations::pwrite_some_bytes(
				stream, byte_output, byte_output, offset) == byte_output);
	require_one_null_call(state, offset);
	reset(state);
	::fast_io::operations::pwrite_all_bytes(stream, byte_output, byte_output, offset);
	require_one_null_call(state, offset);
}

inline void require_scatter_trace(
	operation_state const &state, void const *named_empty_base,
	::fast_io::intfpos_t offset) noexcept
{
	constexpr ::std::array<::std::size_t, 5u> sizes{0u, 1u, 0u, 1u, 0u};
	constexpr ::std::array<::fast_io::intfpos_t, 5u> advances{0, 0, 1, 1, 2};
	require(state.call_count == sizes.size());
	for (::std::size_t index{}; index != sizes.size(); ++index)
	{
		auto const &call{state.calls[index]};
		require(call.size == sizes[index]);
		require(call.offset == offset + advances[index]);
	}
	// Null-empty descriptors acquire stable, typed scalar anchors. A named empty
	// descriptor preserves its original provenance, and neither form advances off.
	require(state.calls[0].first != nullptr);
	require(state.calls[0].first == state.calls[0].last);
	require(state.calls[2].first == named_empty_base);
	require(state.calls[2].last == named_empty_base);
	require(state.calls[4].first != nullptr);
	require(state.calls[4].first == state.calls[4].last);
}

inline void test_scatter_matrix()
{
	operation_state state{};
	byte_io_ref stream{__builtin_addressof(state)};
	char payload[3]{};
	constexpr ::fast_io::intfpos_t offset{73};
	::std::array<::fast_io::basic_io_scatter_t<char>, 5u> const typed{{{nullptr, 0u}, {payload, 1u}, {payload + 1u, 0u}, {payload + 1u, 1u}, {nullptr, 0u}}};
	::std::array<::fast_io::io_scatter_t, 5u> const bytes{{{nullptr, 0u}, {payload, 1u}, {payload + 1u, 0u}, {payload + 1u, 1u}, {nullptr, 0u}}};

	reset(state);
	auto status{::fast_io::operations::scatter_pread_some(
		stream, typed.data(), typed.size(), offset)};
	require(status.position == typed.size() && status.position_in_scatter == 0u);
	require_scatter_trace(state, payload + 1u, offset);
	reset(state);
	::fast_io::operations::scatter_pread_all(
		stream, typed.data(), typed.size(), offset);
	require_scatter_trace(state, payload + 1u, offset);

	reset(state);
	status = ::fast_io::operations::scatter_pread_some_bytes(
		stream, bytes.data(), bytes.size(), offset);
	require(status.position == bytes.size() && status.position_in_scatter == 0u);
	require_scatter_trace(state, payload + 1u, offset);
	reset(state);
	::fast_io::operations::scatter_pread_all_bytes(
		stream, bytes.data(), bytes.size(), offset);
	require_scatter_trace(state, payload + 1u, offset);

	reset(state);
	status = ::fast_io::operations::scatter_pwrite_some(
		stream, typed.data(), typed.size(), offset);
	require(status.position == typed.size() && status.position_in_scatter == 0u);
	require_scatter_trace(state, payload + 1u, offset);
	reset(state);
	::fast_io::operations::scatter_pwrite_all(
		stream, typed.data(), typed.size(), offset);
	require_scatter_trace(state, payload + 1u, offset);

	reset(state);
	status = ::fast_io::operations::scatter_pwrite_some_bytes(
		stream, bytes.data(), bytes.size(), offset);
	require(status.position == bytes.size() && status.position_in_scatter == 0u);
	require_scatter_trace(state, payload + 1u, offset);
	reset(state);
	::fast_io::operations::scatter_pwrite_all_bytes(
		stream, bytes.data(), bytes.size(), offset);
	require_scatter_trace(state, payload + 1u, offset);
}

inline void test_mutex_observability()
{
	operation_state state{};
	locked_io_ref stream{__builtin_addressof(state)};
	char *input{};
	char const *output{};
	constexpr ::fast_io::intfpos_t offset{89};

	reset(state);
	state.require_lock = true;
	::fast_io::operations::pread_all(stream, input, input, offset);
	require_one_null_call(state, offset);
	require(!state.locked && state.lock_calls == 1u && state.unlock_calls == 1u);

	reset(state);
	state.require_lock = true;
	::fast_io::operations::pwrite_all(stream, output, output, offset);
	require_one_null_call(state, offset);
	require(!state.locked && state.lock_calls == 1u && state.unlock_calls == 1u);
}

template <bool byte_mode>
inline void test_seek_fallback_pair()
{
	operation_state state{};
	seek_io_ref<byte_mode> stream{__builtin_addressof(state)};
	constexpr ::fast_io::intfpos_t original{19};
	constexpr ::fast_io::intfpos_t requested{113};
	reset(state, original);
	if constexpr (byte_mode)
	{
		::std::byte *input{};
		require(::fast_io::operations::pread_some_bytes(
					stream, input, input, requested) == input);
	}
	else
	{
		char *input{};
		require(::fast_io::operations::pread_some(
					stream, input, input, requested) == input);
	}
	require_one_null_call(state, requested);
	require(state.seek_calls == 3u && state.current == original);

	reset(state, original);
	if constexpr (byte_mode)
	{
		::std::byte const *output{};
		::fast_io::operations::pwrite_all_bytes(
			stream, output, output, requested);
	}
	else
	{
		char const *output{};
		::fast_io::operations::pwrite_all(
			stream, output, output, requested);
	}
	require_one_null_call(state, requested);
	require(state.seek_calls == 3u && state.current == original);
}

} // namespace positional_empty_range_contract

int main()
{
	using namespace positional_empty_range_contract;
	test_scalar_matrix();
	test_scatter_matrix();
	test_mutex_observability();
	test_seek_fallback_pair<false>();
	test_seek_fallback_pair<true>();
}
