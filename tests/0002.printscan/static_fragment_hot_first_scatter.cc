#include <fast_io.h>
#include <fast_io_format.h>

#include <array>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <string_view>

namespace
{

template <typename char_type>
struct scripted_state
{
	::std::array<::fast_io::io_scatter_status_t, 4u> script{};
	::std::size_t script_size{};
	::std::size_t script_position{};
	::std::array<char_type, 96u> output{};
	::std::size_t output_size{};
	::std::size_t scatter_calls{};
	::std::size_t scalar_calls{};
};

struct byte_sink
{
	using output_char_type = char;
	scripted_state<char> *state{};
};

inline constexpr byte_sink output_stream_ref_define(byte_sink sink) noexcept
{
	return sink;
}

inline constexpr ::std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char, byte_sink>) noexcept
{
	return {};
}

inline ::std::byte const *write_some_bytes_overflow_define(
	byte_sink sink, ::std::byte const *first, ::std::byte const *last) noexcept
{
	auto &state{*sink.state};
	++state.scalar_calls;
	for (; first != last; ++first)
	{
		state.output[state.output_size++] = static_cast<char>(*first);
	}
	return last;
}

inline ::fast_io::io_scatter_status_t scatter_write_some_bytes_overflow_define(
	byte_sink sink, ::fast_io::io_scatter_t const *scatters,
	::std::size_t count) noexcept
{
	auto &state{*sink.state};
	assert(state.script_position < state.script_size);
	auto const status{state.script[state.script_position++]};
	assert(status.position <= count);
	++state.scatter_calls;
	for (::std::size_t index{}; index != status.position; ++index)
	{
		auto const *first{
			reinterpret_cast<char const *>(scatters[index].base)};
		for (::std::size_t offset{}; offset != scatters[index].len; ++offset)
		{
			state.output[state.output_size++] = first[offset];
		}
	}
	if (status.position != count)
	{
		assert(status.position_in_scatter <= scatters[status.position].len);
		auto const *first{reinterpret_cast<char const *>(
			scatters[status.position].base)};
		for (::std::size_t offset{};
			offset != status.position_in_scatter; ++offset)
		{
			state.output[state.output_size++] = first[offset];
		}
	}
	else
	{
		assert(status.position_in_scatter == 0u);
	}
	return status;
}

struct typed_sink
{
	using output_char_type = char16_t;
	scripted_state<char16_t> *state{};
};

inline constexpr typed_sink output_stream_ref_define(typed_sink sink) noexcept
{
	return sink;
}

inline constexpr ::std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char16_t, typed_sink>) noexcept
{
	return {};
}

inline char16_t const *write_some_overflow_define(
	typed_sink sink, char16_t const *first, char16_t const *last) noexcept
{
	auto &state{*sink.state};
	++state.scalar_calls;
	for (; first != last; ++first)
	{
		state.output[state.output_size++] = *first;
	}
	return last;
}

inline ::fast_io::io_scatter_status_t scatter_write_some_overflow_define(
	typed_sink sink,
	::fast_io::basic_io_scatter_t<char16_t> const *scatters,
	::std::size_t count) noexcept
{
	auto &state{*sink.state};
	assert(state.script_position < state.script_size);
	auto const status{state.script[state.script_position++]};
	assert(status.position <= count);
	++state.scatter_calls;
	for (::std::size_t index{}; index != status.position; ++index)
	{
		for (::std::size_t offset{}; offset != scatters[index].len; ++offset)
		{
			state.output[state.output_size++] = scatters[index].base[offset];
		}
	}
	if (status.position != count)
	{
		assert(status.position_in_scatter <= scatters[status.position].len);
		for (::std::size_t offset{};
			offset != status.position_in_scatter; ++offset)
		{
			state.output[state.output_size++] =
				scatters[status.position].base[offset];
		}
	}
	else
	{
		assert(status.position_in_scatter == 0u);
	}
	return status;
}

template <typename char_type, ::std::size_t extent>
void check(scripted_state<char_type> const &state,
	char_type const (&expected)[extent])
{
	assert(state.output_size == extent - 1u);
	assert((::std::basic_string_view<char_type>{
		state.output.data(), state.output_size} ==
		::std::basic_string_view<char_type>{expected, extent - 1u}));
	assert(state.script_position == state.script_size);
}

template <::std::size_t count>
void run_byte_large(::std::array<::fast_io::io_scatter_status_t, count> const &script,
	::std::size_t expected_scatter_calls,
	::std::size_t expected_scalar_calls)
{
	scripted_state<char> state{};
	state.script_size = count;
	for (::std::size_t index{}; index != count; ++index)
	{
		state.script[index] = script[index];
	}
	::fast_io::fmt::print<
		"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef{}">(
			byte_sink{&state}, 32);
	check(state,
		"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef32");
	assert(state.scatter_calls == expected_scatter_calls);
	assert(state.scalar_calls == expected_scalar_calls);
}

template <::std::size_t count>
void run_typed_large(::std::array<::fast_io::io_scatter_status_t, count> const &script,
	::std::size_t expected_scatter_calls,
	::std::size_t expected_scalar_calls)
{
	scripted_state<char16_t> state{};
	state.script_size = count;
	for (::std::size_t index{}; index != count; ++index)
	{
		state.script[index] = script[index];
	}
	::fast_io::fmt::print<
		u"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef{}">(
			typed_sink{&state}, 32);
	check(state,
		u"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef32");
	assert(state.scatter_calls == expected_scatter_calls);
	assert(state.scalar_calls == expected_scalar_calls);
}

} // namespace

int main()
{
	static_assert(
		::fast_io::details::decay::
			print_static_scatter_hot_first_bytes_available<byte_sink>);
	static_assert(
		::fast_io::details::decay::
			print_static_scatter_hot_first_available<typed_sink>);

	// A short all-known record is materialized in one contiguous DSAL array and
	// uses the scalar write-all protocol; it must not pay for scatter descriptors.
	{
		scripted_state<char> state{};
		::fast_io::fmt::print<"i = {}">(byte_sink{&state}, 32);
		check(state, "i = 32");
		assert(state.scatter_calls == 0u && state.scalar_calls == 1u);
	}
	{
		scripted_state<char16_t> state{};
		::fast_io::fmt::print<u"i = {}">(typed_sink{&state}, 32);
		check(state, u"i = 32");
		assert(state.scatter_calls == 0u && state.scalar_calls == 1u);
	}

	// A record above the compact threshold keeps its two immutable providers.
	// Complete first attempt, partial first provider, partial second provider,
	// and a legal zero-progress first attempt all produce the same record.
	run_byte_large(::std::array{::fast_io::io_scatter_status_t{2u, 0u}}, 1u, 0u);
	run_byte_large(::std::array{::fast_io::io_scatter_status_t{0u, 2u},
		::fast_io::io_scatter_status_t{1u, 0u}}, 2u, 1u);
	run_byte_large(::std::array{::fast_io::io_scatter_status_t{1u, 1u}}, 1u, 1u);
	run_byte_large(::std::array{::fast_io::io_scatter_status_t{0u, 0u},
		::fast_io::io_scatter_status_t{2u, 0u}}, 2u, 0u);

	run_typed_large(::std::array{::fast_io::io_scatter_status_t{2u, 0u}}, 1u, 0u);
	run_typed_large(::std::array{::fast_io::io_scatter_status_t{0u, 0u},
		::fast_io::io_scatter_status_t{2u, 0u}}, 2u, 0u);

#if defined(__linux__) && defined(__cpp_exceptions)
	bool bad_file_descriptor{};
	try
	{
		::fast_io::fmt::print<"i = {}">(
			::fast_io::posix_io_observer{-1}, 32);
	}
	catch (::fast_io::error const error)
	{
		bad_file_descriptor = error.domain == ::fast_io::posix_domain_value &&
			error.code == static_cast<::std::size_t>(EBADF);
	}
	assert(bad_file_descriptor);

	bool interrupted{};
	try
	{
		::fast_io::details::posix_scatter_write_error(
			-static_cast<::std::ptrdiff_t>(EINTR));
	}
	catch (::fast_io::error const error)
	{
		interrupted = error.domain == ::fast_io::posix_domain_value &&
			error.code == static_cast<::std::size_t>(EINTR);
	}
	assert(interrupted);
#endif
}
