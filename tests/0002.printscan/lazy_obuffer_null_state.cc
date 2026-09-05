#include <array>
#include <cassert>
#include <cstddef>
#include <type_traits>

#include <fast_io.h>

namespace lazy_obuffer_null_state_test
{

struct sink_state
{
	::std::array<char, 32u> storage{};
	::std::size_t size{};
	::std::size_t write_calls{};
};

struct sink_ref
{
	using output_char_type = char;
	sink_state *state{};
};

struct sink
{
	sink_state *state{};

	inline explicit constexpr sink(sink_state *value) noexcept : state(value) {}
};

[[nodiscard]] inline constexpr sink_ref output_stream_ref_define(sink &value) noexcept
{
	return {value.state};
}

inline void write_all_overflow_define(
	sink_ref output, char const *first, char const *last) noexcept
{
	++output.state->write_calls;
	for (; first != last; ++first)
	{
		assert(output.state->size != output.state->storage.size());
		output.state->storage[output.state->size++] = *first;
	}
}

using output_traits = ::fast_io::basic_io_buffer_traits<
	::fast_io::buffer_mode::out, ::fast_io::native_global_allocator, void,
	char, 0u, 8u>;
using output_buffer = ::fast_io::basic_io_buffer<sink, output_traits>;
using output_buffer_ref = ::fast_io::basic_io_buffer_ref<output_buffer>;

static_assert(::fast_io::obuffer_address_distance_safe<char, output_buffer_ref>);

consteval bool null_zero_copy_contract()
{
	char const *source{};
	char *destination{};
	return ::fast_io::details::non_overlapped_copy_n(
			   source, 0u, destination) == nullptr;
}

static_assert(null_zero_copy_contract());

template <bool byte_mode>
struct retry_state
{
	::std::array<char, 8u> buffer{};
	char direct_character{};
	char *begin{};
	char *current{};
	char *end{};
	::std::size_t calls{};
};

template <bool byte_mode>
struct retry_ref
{
	using output_char_type = char;
	retry_state<byte_mode> *state{};
};

template <bool byte_mode>
[[nodiscard]] inline constexpr retry_ref<byte_mode> output_stream_ref_define(
	retry_ref<byte_mode> output) noexcept
{
	// The fixture observer is already normalized; returning its pointer-sized
	// value preserves identity while exercising the public primitive entry point.
	return output;
}

template <bool byte_mode>
[[nodiscard]] inline constexpr char *obuffer_begin(retry_ref<byte_mode> output) noexcept
{
	return output.state->begin;
}

template <bool byte_mode>
[[nodiscard]] inline constexpr char *obuffer_curr(retry_ref<byte_mode> output) noexcept
{
	return output.state->current;
}

template <bool byte_mode>
[[nodiscard]] inline constexpr char *obuffer_end(retry_ref<byte_mode> output) noexcept
{
	return output.state->end;
}

template <bool byte_mode>
inline constexpr void obuffer_set_curr(
	retry_ref<byte_mode> output, char *current) noexcept
{
	output.state->current = current;
}

inline char const *write_some_overflow_define(
	retry_ref<false> output, char const *first, char const *last) noexcept
{
	assert(first != last);
	assert(output.state->begin == nullptr);
	++output.state->calls;
	output.state->direct_character = *first;
	output.state->begin = output.state->current = output.state->buffer.data();
	output.state->end = output.state->begin + output.state->buffer.size();
	return first + 1u;
}

inline ::std::byte const *write_some_bytes_overflow_define(
	retry_ref<true> output, ::std::byte const *first,
	::std::byte const *last) noexcept
{
	assert(first != last);
	assert(output.state->begin == nullptr);
	++output.state->calls;
	output.state->direct_character = static_cast<char>(*first);
	output.state->begin = output.state->current = output.state->buffer.data();
	output.state->end = output.state->begin + output.state->buffer.size();
	return first + 1u;
}

inline void test_cold_retry_after_lazy_publication()
{
	char const source[]{'x', 'y'};
	{
		retry_state<false> state;
		retry_ref<false> output{__builtin_addressof(state)};
		::fast_io::operations::write_all(output, source, source + 2u);
		// The some-CPO consumes one character and publishes a fresh put area. The
		// all-operation must reacquire that area before copying the remaining suffix.
		assert(state.calls == 1u && state.direct_character == 'x');
		assert(state.current == state.begin + 1u && state.buffer[0] == 'y');
	}
	{
		retry_state<true> state;
		retry_ref<true> output{__builtin_addressof(state)};
		auto const *const first{
			reinterpret_cast<::std::byte const *>(source)};
		::fast_io::operations::write_all_bytes(output, first, first + 2u);
		// Byte retry follows the identical state transition and must not reuse the
		// null cursors observed before the first overflow call.
		assert(state.calls == 1u && state.direct_character == 'x');
		assert(state.current == state.begin + 1u && state.buffer[0] == 'y');
	}
}

template <typename operation>
inline void check_fresh_buffer_nonempty(operation execute)
{
	sink_state physical;
	{
		output_buffer output{__builtin_addressof(physical)};
		assert(output.output_buffer.buffer_begin == nullptr);
		assert(output.output_buffer.buffer_curr == nullptr);
		assert(output.output_buffer.buffer_end == nullptr);
		execute(output);
		// The first nonempty operation reaches lazy allocation without subtracting
		// the all-null cursor pair. Its one unit remains buffered until teardown.
		assert(output.output_buffer.buffer_begin != nullptr);
		assert(output.output_buffer.buffer_curr ==
			   output.output_buffer.buffer_begin + 1u);
		assert(physical.write_calls == 0u);
	}
	assert(physical.write_calls == 1u);
	assert(physical.size == 1u && physical.storage[0] == 'x');
}

template <typename operation>
inline void check_fresh_buffer_empty_scatter(operation execute)
{
	sink_state physical;
	{
		output_buffer output{__builtin_addressof(physical)};
		execute(output);
		// A zero-extent descriptor is complete but carries no readable object. It
		// must not allocate, call the sink, or perform arithmetic on either null base.
		assert(output.output_buffer.buffer_begin == nullptr);
		assert(output.output_buffer.buffer_curr == nullptr);
		assert(output.output_buffer.buffer_end == nullptr);
		assert(physical.write_calls == 0u);
	}
	assert(physical.write_calls == 0u && physical.size == 0u);
}

inline void test_basic_io_buffer_matrix()
{
	char const source{'x'};
	auto const *const first{__builtin_addressof(source)};
	auto const *const last{first + 1u};
	auto const *const byte_first{reinterpret_cast<::std::byte const *>(first)};
	auto const *const byte_last{byte_first + 1u};
	::fast_io::basic_io_scatter_t<char> const typed_scatter{first, 1u};
	::fast_io::io_scatter_t const byte_scatter{first, 1u};
	::std::array<::fast_io::basic_io_scatter_t<char>, 3u> const mixed_typed{{
		{nullptr, 0u}, {first, 1u}, {nullptr, 0u}}};
	::std::array<::fast_io::io_scatter_t, 3u> const mixed_bytes{{
		{nullptr, 0u}, {first, 1u}, {nullptr, 0u}}};

	check_fresh_buffer_nonempty([&](output_buffer &output) {
		assert(::fast_io::operations::write_some(output, first, last) == last);
	});
	check_fresh_buffer_nonempty([&](output_buffer &output) {
		::fast_io::operations::write_all(output, first, last);
	});
	check_fresh_buffer_nonempty([&](output_buffer &output) {
		assert(::fast_io::operations::write_some_bytes(
				   output, byte_first, byte_last) == byte_last);
	});
	check_fresh_buffer_nonempty([&](output_buffer &output) {
		::fast_io::operations::write_all_bytes(output, byte_first, byte_last);
	});
	check_fresh_buffer_nonempty([&](output_buffer &output) {
		auto const status{::fast_io::operations::scatter_write_some(
			output, __builtin_addressof(typed_scatter), 1u)};
		assert(status.position == 1u && status.position_in_scatter == 0u);
	});
	check_fresh_buffer_nonempty([&](output_buffer &output) {
		::fast_io::operations::scatter_write_all(
			output, __builtin_addressof(typed_scatter), 1u);
	});
	check_fresh_buffer_nonempty([&](output_buffer &output) {
		auto const status{::fast_io::operations::scatter_write_some_bytes(
			output, __builtin_addressof(byte_scatter), 1u)};
		assert(status.position == 1u && status.position_in_scatter == 0u);
	});
	check_fresh_buffer_nonempty([&](output_buffer &output) {
		::fast_io::operations::scatter_write_all_bytes(
			output, __builtin_addressof(byte_scatter), 1u);
	});
	check_fresh_buffer_nonempty([&](output_buffer &output) {
		auto const status{::fast_io::operations::scatter_write_some(
			output, mixed_typed.data(), mixed_typed.size())};
		// Empty descriptors on either side preserve the logical position while the
		// middle descriptor is the first operation that may allocate the put area.
		assert(status.position == mixed_typed.size() &&
			   status.position_in_scatter == 0u);
	});
	check_fresh_buffer_nonempty([&](output_buffer &output) {
		::fast_io::operations::scatter_write_all_bytes(
			output, mixed_bytes.data(), mixed_bytes.size());
	});

	::fast_io::basic_io_scatter_t<char> const empty_typed{nullptr, 0u};
	::fast_io::io_scatter_t const empty_bytes{nullptr, 0u};
	check_fresh_buffer_empty_scatter([&](output_buffer &output) {
		auto const status{::fast_io::operations::scatter_write_some(
			output, __builtin_addressof(empty_typed), 1u)};
		assert(status.position == 1u && status.position_in_scatter == 0u);
	});
	check_fresh_buffer_empty_scatter([&](output_buffer &output) {
		::fast_io::operations::scatter_write_all(
			output, __builtin_addressof(empty_typed), 1u);
	});
	check_fresh_buffer_empty_scatter([&](output_buffer &output) {
		auto const status{::fast_io::operations::scatter_write_some_bytes(
			output, __builtin_addressof(empty_bytes), 1u)};
		assert(status.position == 1u && status.position_in_scatter == 0u);
	});
	check_fresh_buffer_empty_scatter([&](output_buffer &output) {
		::fast_io::operations::scatter_write_all_bytes(
			output, __builtin_addressof(empty_bytes), 1u);
	});
}

template <typename operation>
inline void check_fresh_transcoder_nonempty(operation execute)
{
	::std::array<char, 4u> storage{};
	::fast_io::basic_obuffer_view<char> physical{storage};
	auto output{::fast_io::make_otranscoder(
		physical, ::fast_io::transcoders::lf_to_crlf{})};
	using output_ref = ::std::remove_cvref_t<decltype(
		::fast_io::operations::output_stream_ref(output))>;
	static_assert(::fast_io::obuffer_address_distance_safe<char, output_ref>);
	assert(output.public_buffer.buffer_begin == nullptr);
	assert(output.public_buffer.buffer_curr == nullptr);
	assert(output.public_buffer.buffer_end == nullptr);
	execute(output);
	// The transcoder owns an independent lazy public put area. Its first primitive
	// write allocates that area but cannot run the engine before explicit finish.
	assert(output.public_buffer.buffer_begin != nullptr);
	assert(output.public_buffer.buffer_curr ==
		   output.public_buffer.buffer_begin + 1u);
	assert(physical.empty());
	::fast_io::operations::output_stream_finish(output);
	assert(physical.size() == 1u && storage[0] == 'x');
}

template <typename operation>
inline void check_fresh_transcoder_empty_scatter(operation execute)
{
	::std::array<char, 4u> storage{};
	::fast_io::basic_obuffer_view<char> physical{storage};
	auto output{::fast_io::make_otranscoder(
		physical, ::fast_io::transcoders::lf_to_crlf{})};
	execute(output);
	// Structural completion of an empty descriptor cannot allocate the public
	// adapter buffer or emit any transformed unit.
	assert(output.public_buffer.buffer_begin == nullptr);
	assert(output.public_buffer.buffer_curr == nullptr);
	assert(output.public_buffer.buffer_end == nullptr);
	assert(physical.empty());
}

inline void test_otranscoder_matrix()
{
	char const source{'x'};
	auto const *const first{__builtin_addressof(source)};
	auto const *const last{first + 1u};
	auto const *const byte_first{reinterpret_cast<::std::byte const *>(first)};
	auto const *const byte_last{byte_first + 1u};
	::fast_io::basic_io_scatter_t<char> const typed_scatter{first, 1u};
	::fast_io::io_scatter_t const byte_scatter{first, 1u};
	::std::array<::fast_io::basic_io_scatter_t<char>, 3u> const mixed_typed{{
		{nullptr, 0u}, {first, 1u}, {nullptr, 0u}}};
	::std::array<::fast_io::io_scatter_t, 3u> const mixed_bytes{{
		{nullptr, 0u}, {first, 1u}, {nullptr, 0u}}};

	check_fresh_transcoder_nonempty([&](auto &output) {
		assert(::fast_io::operations::write_some(output, first, last) == last);
	});
	check_fresh_transcoder_nonempty([&](auto &output) {
		::fast_io::operations::write_all(output, first, last);
	});
	check_fresh_transcoder_nonempty([&](auto &output) {
		assert(::fast_io::operations::write_some_bytes(
				   output, byte_first, byte_last) == byte_last);
	});
	check_fresh_transcoder_nonempty([&](auto &output) {
		::fast_io::operations::write_all_bytes(output, byte_first, byte_last);
	});
	check_fresh_transcoder_nonempty([&](auto &output) {
		auto const status{::fast_io::operations::scatter_write_some(
			output, __builtin_addressof(typed_scatter), 1u)};
		assert(status.position == 1u && status.position_in_scatter == 0u);
	});
	check_fresh_transcoder_nonempty([&](auto &output) {
		::fast_io::operations::scatter_write_all(
			output, __builtin_addressof(typed_scatter), 1u);
	});
	check_fresh_transcoder_nonempty([&](auto &output) {
		auto const status{::fast_io::operations::scatter_write_some_bytes(
			output, __builtin_addressof(byte_scatter), 1u)};
		assert(status.position == 1u && status.position_in_scatter == 0u);
	});
	check_fresh_transcoder_nonempty([&](auto &output) {
		::fast_io::operations::scatter_write_all_bytes(
			output, __builtin_addressof(byte_scatter), 1u);
	});
	check_fresh_transcoder_nonempty([&](auto &output) {
		auto const status{::fast_io::operations::scatter_write_some(
			output, mixed_typed.data(), mixed_typed.size())};
		assert(status.position == mixed_typed.size() &&
			   status.position_in_scatter == 0u);
	});
	check_fresh_transcoder_nonempty([&](auto &output) {
		::fast_io::operations::scatter_write_all_bytes(
			output, mixed_bytes.data(), mixed_bytes.size());
	});

	::fast_io::basic_io_scatter_t<char> const empty_typed{nullptr, 0u};
	::fast_io::io_scatter_t const empty_bytes{nullptr, 0u};
	check_fresh_transcoder_empty_scatter([&](auto &output) {
		auto const status{::fast_io::operations::scatter_write_some(
			output, __builtin_addressof(empty_typed), 1u)};
		assert(status.position == 1u && status.position_in_scatter == 0u);
	});
	check_fresh_transcoder_empty_scatter([&](auto &output) {
		::fast_io::operations::scatter_write_all(
			output, __builtin_addressof(empty_typed), 1u);
	});
	check_fresh_transcoder_empty_scatter([&](auto &output) {
		auto const status{::fast_io::operations::scatter_write_some_bytes(
			output, __builtin_addressof(empty_bytes), 1u)};
		assert(status.position == 1u && status.position_in_scatter == 0u);
	});
	check_fresh_transcoder_empty_scatter([&](auto &output) {
		::fast_io::operations::scatter_write_all_bytes(
			output, __builtin_addressof(empty_bytes), 1u);
	});
}

} // namespace lazy_obuffer_null_state_test

int main()
{
	::lazy_obuffer_null_state_test::test_cold_retry_after_lazy_publication();
	::lazy_obuffer_null_state_test::test_basic_io_buffer_matrix();
	::lazy_obuffer_null_state_test::test_otranscoder_matrix();
}
