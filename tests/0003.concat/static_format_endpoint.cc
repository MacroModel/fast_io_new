#include <fast_io_device.h>
#include <fast_io_format.h>

#include <array>
#include <cstddef>
#include <string_view>

namespace static_format_endpoint_test
{

struct write_state
{
	::std::array<char, 64u> bytes{};
	char const *source{};
	::std::size_t size{};
	::std::size_t calls{};
};

struct write_sink
{
	using output_char_type = char;
	write_state *state{};
};

inline constexpr write_sink output_stream_ref_define(write_sink sink) noexcept
{
	return sink;
}

inline void write_all_overflow_define(
	write_sink sink, char const *first, char const *last) noexcept
{
	auto &state{*sink.state};
	state.source = first;
	++state.calls;
	for (; first != last; ++first)
	{
		state.bytes[state.size++] = *first;
	}
}

struct status_state
{
	::std::size_t calls{};
};

struct status_sink
{
	using output_char_type = char;
	status_state *state{};
};

inline constexpr status_sink output_stream_ref_define(status_sink sink) noexcept
{
	return sink;
}

template <bool line>
inline void status_print_define(
	status_sink sink,
	::fast_io::manipulators::static_scatter_t<char, 4u>) noexcept
{
	static_assert(!line);
	++sink.state->calls;
}

template <bool line>
inline void status_print_define(
	status_sink sink,
	::fast_io::io_null_t) noexcept
{
	static_assert(!line);
	++sink.state->calls;
}

struct locked_state
{
	write_state write{};
	::std::size_t locks{};
	::std::size_t unlocks{};
	::std::size_t outer_status_calls{};
	bool locked{};
};

struct unlocked_sink
{
	using output_char_type = char;
	locked_state *state{};
};

struct locked_sink
{
	using output_char_type = char;
	locked_state *state{};
};

struct lock_proxy
{
	locked_state *state{};

	inline void lock() noexcept
	{
		if (state->locked) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		state->locked = true;
		++state->locks;
	}

	inline void unlock() noexcept
	{
		if (!state->locked) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		state->locked = false;
		++state->unlocks;
	}
};

inline constexpr locked_sink output_stream_ref_define(locked_sink sink) noexcept
{
	return sink;
}

inline constexpr lock_proxy output_stream_mutex_ref_define(locked_sink sink) noexcept
{
	return {sink.state};
}

inline constexpr unlocked_sink output_stream_unlocked_ref_define(locked_sink sink) noexcept
{
	return {sink.state};
}

inline void write_all_overflow_define(
	unlocked_sink sink, char const *first, char const *last) noexcept
{
	if (!sink.state->locked) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	write_all_overflow_define(
		write_sink{__builtin_addressof(sink.state->write)}, first, last);
}

template <bool line>
inline void status_print_define(
	locked_sink sink,
	::fast_io::manipulators::static_scatter_t<char, 4u>) noexcept
{
	static_assert(!line);
	++sink.state->outer_status_calls;
}

inline constexpr ::fast_io::fmt::basic_fixed_string format_literal{"a{1}c{0}"};
using first_static_type = decltype(::fast_io::fmt::static_arg<"d">());
using second_static_type = decltype(::fast_io::fmt::static_arg<"b">());
using static_program = ::fast_io::fmt::details::compiled_static_format_program<
	format_literal, ::fast_io::fmt::brace_fmt_t,
	first_static_type, second_static_type>;
static_assert(static_program::size == 4u);

} // namespace static_format_endpoint_test

int main()
{
	using namespace static_format_endpoint_test;
	auto const expected_source{static_program::storage.data()};

	char output[32u]{};
	::fast_io::obuffer_view buffer{output, output + 32u};
	::fast_io::fmt::print<format_literal>(
		buffer, ::fast_io::fmt::static_arg<"d">(),
		::fast_io::fmt::static_arg<"b">());
	if (::std::string_view{output, buffer.size()} != "abcd")
	{
		return 1;
	}

	auto const concatenated{::fast_io::fmt::concat_std<format_literal>(
		::fast_io::fmt::static_arg<"d">(),
		::fast_io::fmt::static_arg<"b">())};
	if (concatenated != "abcd")
	{
		return 2;
	}

	write_state fmt_write{};
	::fast_io::fmt::print<format_literal>(
		write_sink{__builtin_addressof(fmt_write)},
		::fast_io::fmt::static_arg<"d">(),
		::fast_io::fmt::static_arg<"b">());
	if (fmt_write.calls != 1u || fmt_write.source != expected_source ||
		::std::string_view{fmt_write.bytes.data(), fmt_write.size} != "abcd")
	{
		return 3;
	}

	status_state status{};
	::fast_io::fmt::print<format_literal>(
		status_sink{__builtin_addressof(status)},
		::fast_io::fmt::static_arg<"d">(),
		::fast_io::fmt::static_arg<"b">());
	if (status.calls != 1u)
	{
		return 4;
	}
	::fast_io::fmt::print<"{}">(
		status_sink{__builtin_addressof(status)},
		::fast_io::fmt::static_arg<"">());
	if (status.calls != 2u)
	{
		return 5;
	}

	write_state empty_write{};
	::fast_io::fmt::print<"{}">(
		write_sink{__builtin_addressof(empty_write)},
		::fast_io::fmt::static_arg<"">());
	if (empty_write.calls != 0u || empty_write.size != 0u)
	{
		return 6;
	}

	locked_state empty_locked{};
	::fast_io::fmt::print<"{}">(
		locked_sink{__builtin_addressof(empty_locked)},
		::fast_io::fmt::static_arg<"">());
	if (empty_locked.locks != 1u || empty_locked.unlocks != 1u ||
		empty_locked.locked || empty_locked.write.calls != 0u)
	{
		return 7;
	}

	locked_state locked{};
	::fast_io::fmt::print<format_literal>(
		locked_sink{__builtin_addressof(locked)},
		::fast_io::fmt::static_arg<"d">(),
		::fast_io::fmt::static_arg<"b">());
	if (locked.locks != 1u || locked.unlocks != 1u || locked.locked ||
		locked.outer_status_calls != 0u || locked.write.calls != 1u ||
		locked.write.source != expected_source ||
		::std::string_view{locked.write.bytes.data(), locked.write.size} != "abcd")
	{
		return 8;
	}

	static constexpr char source[]{'a', 'b', 'c', 'd'};
	write_state direct{};
	::fast_io::io::print(
		write_sink{__builtin_addressof(direct)},
		::fast_io::manipulators::static_scatter_t<char, 4u>{source});
	if (direct.calls != 1u || direct.source != source ||
		::std::string_view{direct.bytes.data(), direct.size} != "abcd")
	{
		return 9;
	}

	write_state line{};
	::fast_io::io::println(
		write_sink{__builtin_addressof(line)},
		::fast_io::manipulators::static_scatter_t<char, 4u>{source});
	if (line.calls != 1u ||
		::std::string_view{line.bytes.data(), line.size} != "abcd\n")
	{
		return 10;
	}
}
