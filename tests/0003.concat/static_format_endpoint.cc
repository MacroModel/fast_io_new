#include <fast_io_device.h>
#include <fast_io_format.h>

#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>

namespace static_format_endpoint_test
{

struct dynamic_text
{
	::std::string_view value;
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, dynamic_text>, dynamic_text value) noexcept
{
	return value.value.size();
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, dynamic_text>, char *output,
	dynamic_text value) noexcept
{
	return ::fast_io::details::non_overlapped_copy_n(
		value.value.data(), value.value.size(), output);
}

struct runtime_scatter_text
{
	::std::string_view value;
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, runtime_scatter_text>,
	runtime_scatter_text value) noexcept
{
	return value.value.size();
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, runtime_scatter_text>, char *output,
	runtime_scatter_text value) noexcept
{
	return ::fast_io::details::non_overlapped_copy_n(
		value.value.data(), value.value.size(), output);
}

inline constexpr ::fast_io::reserve_scatters_size_result
print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, runtime_scatter_text>,
	runtime_scatter_text value) noexcept
{
	return {1u, value.value.size()};
}

inline constexpr ::fast_io::basic_reserve_scatters_define_result<char>
print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, runtime_scatter_text>,
	::fast_io::basic_io_scatter_t<char> *scatter, char *reserve,
	runtime_scatter_text value) noexcept
{
	char *const end{print_reserve_define(
		::fast_io::io_reserve_type<char, runtime_scatter_text>, reserve,
		value)};
	*scatter = {reserve, value.value.size()};
	return {scatter + 1u, end};
}

inline constexpr ::std::true_type print_borrowed_reserve_scatters_source(
	::fast_io::io_reserve_type_t<char, runtime_scatter_text>) noexcept
{
	return {};
}

struct write_state
{
	::std::array<char, 64u> bytes{};
	::std::array<char const *, 4u> sources{};
	::std::array<::std::size_t, 4u> sizes{};
	char const *source{};
	::std::size_t size{};
	::std::size_t calls{};
};

struct write_sink
{
	using output_char_type = char;
	write_state *state{};
};

struct scatter_state
{
	::std::array<char, 64u> bytes{};
	::std::array<char const *, 4u> sources{};
	::std::array<::std::size_t, 4u> sizes{};
	::std::size_t size{};
	::std::size_t count{};
	::std::size_t calls{};
};

struct scatter_sink
{
	using output_char_type = char;
	scatter_state *state{};
};

struct byte_scatter_sink
{
	using output_char_type = char;
	scatter_state *state{};
};

inline constexpr write_sink output_stream_ref_define(write_sink sink) noexcept
{
	return sink;
}

inline constexpr scatter_sink output_stream_ref_define(scatter_sink sink) noexcept
{
	return sink;
}

inline constexpr byte_scatter_sink output_stream_ref_define(
	byte_scatter_sink sink) noexcept
{
	return sink;
}

inline constexpr ::std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char, scatter_sink>) noexcept
{
	return {};
}

inline constexpr ::std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char, byte_scatter_sink>) noexcept
{
	return {};
}

inline void write_all_overflow_define(
	write_sink sink, char const *first, char const *last) noexcept
{
	auto &state{*sink.state};
	state.source = first;
	state.sources[state.calls] = first;
	state.sizes[state.calls] = static_cast<::std::size_t>(last - first);
	++state.calls;
	for (; first != last; ++first)
	{
		state.bytes[state.size++] = *first;
	}
}

inline void scatter_write_all_overflow_define(
	scatter_sink sink, ::fast_io::basic_io_scatter_t<char> const *scatters,
	::std::size_t count) noexcept
{
	auto &state{*sink.state};
	++state.calls;
	state.count = count;
	for (::std::size_t index{}; index != count; ++index)
	{
		state.sources[index] = scatters[index].base;
		state.sizes[index] = scatters[index].len;
		for (auto first{scatters[index].base},
			  last{first + scatters[index].len};
			 first != last; ++first)
		{
			state.bytes[state.size++] = *first;
		}
	}
}

inline void scatter_write_all_bytes_overflow_define(
	byte_scatter_sink sink, ::fast_io::io_scatter_t const *scatters,
	::std::size_t count) noexcept
{
	auto &state{*sink.state};
	++state.calls;
	state.count = count;
	for (::std::size_t index{}; index != count; ++index)
	{
		auto const source{static_cast<char const *>(scatters[index].base)};
		state.sources[index] = source;
		state.sizes[index] = scatters[index].len;
		::fast_io::details::non_overlapped_copy_n(
			source, scatters[index].len, state.bytes.data() + state.size);
		state.size += scatters[index].len;
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

struct partial_status_state
{
	char const *prefix{};
	char const *dynamic{};
	char const *suffix{};
	::std::size_t dynamic_size{};
	::std::size_t calls{};
};

struct partial_status_sink
{
	using output_char_type = char;
	partial_status_state *state{};
};

inline constexpr status_sink output_stream_ref_define(status_sink sink) noexcept
{
	return sink;
}

inline constexpr partial_status_sink output_stream_ref_define(
	partial_status_sink sink) noexcept
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

template <bool line>
inline void status_print_define(
	partial_status_sink sink,
	::fast_io::manipulators::static_scatter_t<char, 4u> prefix,
	::fast_io::basic_io_scatter_t<char> dynamic,
	::fast_io::manipulators::static_scatter_t<char, 1u> suffix) noexcept
{
	static_assert(!line);
	sink.state->prefix = prefix.base;
	sink.state->dynamic = dynamic.base;
	sink.state->suffix = suffix.base;
	sink.state->dynamic_size = dynamic.len;
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

struct precision_status_state
{
	::std::array<char, 64u> put_area{};
	char *current{put_area.data()};
	::std::size_t calls{};
};

struct precision_status_sink
{
	using output_char_type = char;
	precision_status_state *state{};
};

inline constexpr precision_status_sink output_stream_ref_define(
	precision_status_sink sink) noexcept
{
	return sink;
}

inline constexpr char *obuffer_begin(precision_status_sink sink) noexcept
{
	return sink.state->put_area.data();
}

inline constexpr char *obuffer_curr(precision_status_sink sink) noexcept
{
	return sink.state->current;
}

inline constexpr char *obuffer_end(precision_status_sink sink) noexcept
{
	return sink.state->put_area.data() + sink.state->put_area.size();
}

inline constexpr void obuffer_set_curr(
	precision_status_sink sink, char *current) noexcept
{
	sink.state->current = current;
}

template <bool line, ::std::size_t extent, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
inline void status_print_define(
	precision_status_sink sink,
	::fast_io::manipulators::static_scatter_t<char, extent>,
	::fast_io::manipulators::format_scalar_t<
		scalar_type, base_prefix_size, space_sign>) noexcept
{
	static_assert(!line);
	static_assert(extent == 27u);
	++sink.state->calls;
}

struct short_precision_state
{
	::std::array<char, 8u> put_area{};
	char *current{put_area.data()};
	write_state write{};
};

struct short_precision_sink
{
	using output_char_type = char;
	short_precision_state *state{};
};

inline constexpr short_precision_sink output_stream_ref_define(
	short_precision_sink sink) noexcept
{
	return sink;
}

inline constexpr char *obuffer_begin(short_precision_sink sink) noexcept
{
	return sink.state->put_area.data();
}

inline constexpr char *obuffer_curr(short_precision_sink sink) noexcept
{
	return sink.state->current;
}

inline constexpr char *obuffer_end(short_precision_sink sink) noexcept
{
	return sink.state->put_area.data() + sink.state->put_area.size();
}

inline constexpr void obuffer_set_curr(
	short_precision_sink sink, char *current) noexcept
{
	sink.state->current = current;
}

inline void flush_short_precision(short_precision_sink sink) noexcept
{
	char *const first{sink.state->put_area.data()};
	char *const last{sink.state->current};
	if (first != last)
	{
		write_all_overflow_define(
			write_sink{__builtin_addressof(sink.state->write)}, first, last);
		sink.state->current = first;
	}
}

inline void output_stream_buffer_flush_define(
	short_precision_sink sink) noexcept
{
	flush_short_precision(sink);
}

inline void write_all_overflow_define(
	short_precision_sink sink, char const *first, char const *last) noexcept
{
	flush_short_precision(sink);
	if (first != last)
	{
		write_all_overflow_define(
			write_sink{__builtin_addressof(sink.state->write)}, first, last);
	}
}

inline constexpr ::fast_io::fmt::basic_fixed_string format_literal{"a{1}c{0}"};
using first_static_type = decltype(::fast_io::fmt::static_arg<"d">);
using second_static_type = decltype(::fast_io::fmt::static_arg<"b">);
using static_program = ::fast_io::fmt::details::compiled_static_format_program<
	format_literal, ::fast_io::fmt::brace_fmt_t,
	first_static_type, second_static_type>;
static_assert(static_program::size == 4u);

inline constexpr ::fast_io::fmt::basic_fixed_string partial_format_literal{
	"A{}B{}C"};
using partial_static_type = decltype(::fast_io::fmt::static_arg<42u>);
using partial_prefix = ::fast_io::fmt::details::compiled_static_format_run<
	partial_format_literal, ::fast_io::fmt::brace_fmt_t, 0u, 3u,
	partial_static_type, ::std::string_view>;
using partial_suffix = ::fast_io::fmt::details::compiled_static_format_run<
	partial_format_literal, ::fast_io::fmt::brace_fmt_t, 4u, 1u,
	partial_static_type, ::std::string_view>;
static_assert(partial_prefix::size == 4u);
static_assert(partial_suffix::size == 1u);
static_assert(::std::string_view{partial_prefix::storage.data(),
								partial_prefix::size} == "A42B");
static_assert(::std::string_view{partial_suffix::storage.data(),
								partial_suffix::size} == "C");

inline constexpr ::fast_io::fmt::basic_fixed_string precision_format_literal{
	"user={} id={:08x} score={:.2f}"};
inline constexpr ::std::string_view precision_expected{
	"user=xxx id=0000002a score=3.14"};
inline constexpr char precision_prefix[]{
	"user=xxx id=0000002a score="};

inline constexpr ::std::array aggregate_values{1u, 15u, 255u};
inline constexpr ::fast_io::fmt::basic_fixed_string aggregate_format{
	"values={}"};
using aggregate_static_type =
	decltype(::fast_io::fmt::static_arg<aggregate_values>);
using aggregate_static_program =
	::fast_io::fmt::details::compiled_static_format_program<
		aggregate_format, ::fast_io::fmt::brace_fmt_t,
		aggregate_static_type>;
static_assert(::std::string_view{
				  aggregate_static_program::storage.data(),
				  aggregate_static_program::size} == "values=[1, 15, 255]");

} // namespace static_format_endpoint_test

int main()
{
	using namespace static_format_endpoint_test;
	auto const expected_source{static_program::storage.data()};

	char output[32u]{};
	::fast_io::obuffer_view buffer{output, output + 32u};
	::fast_io::fmt::print<format_literal>(
		buffer, ::fast_io::fmt::static_arg<"d">,
		::fast_io::fmt::static_arg<"b">);
	if (::std::string_view{output, buffer.size()} != "abcd")
	{
		return 1;
	}

	auto const concatenated{::fast_io::fmt::concat_std<format_literal>(
		::fast_io::fmt::static_arg<"d">,
		::fast_io::fmt::static_arg<"b">)};
	auto const concatenated_fast_io{
		::fast_io::fmt::concat_fast_io<format_literal>(
			::fast_io::fmt::static_arg<"d">,
			::fast_io::fmt::static_arg<"b">)};
	if (concatenated != "abcd" ||
		::std::string_view{concatenated_fast_io.data(),
						   concatenated_fast_io.size()} != "abcd")
	{
		return 2;
	}

	write_state fmt_write{};
	::fast_io::fmt::print<format_literal>(
		write_sink{__builtin_addressof(fmt_write)},
		::fast_io::fmt::static_arg<"d">,
		::fast_io::fmt::static_arg<"b">);
	if (fmt_write.calls != 1u || fmt_write.source != expected_source ||
		::std::string_view{fmt_write.bytes.data(), fmt_write.size} != "abcd")
	{
		return 3;
	}

	status_state status{};
	::fast_io::fmt::print<format_literal>(
		status_sink{__builtin_addressof(status)},
		::fast_io::fmt::static_arg<"d">,
		::fast_io::fmt::static_arg<"b">);
	if (status.calls != 1u)
	{
		return 4;
	}
	::fast_io::fmt::print<"{}">(
		status_sink{__builtin_addressof(status)},
		::fast_io::fmt::static_arg<"">);
	if (status.calls != 2u)
	{
		return 5;
	}

	write_state empty_write{};
	::fast_io::fmt::print<"{}">(
		write_sink{__builtin_addressof(empty_write)},
		::fast_io::fmt::static_arg<"">);
	if (empty_write.calls != 0u || empty_write.size != 0u)
	{
		return 6;
	}

	locked_state empty_locked{};
	::fast_io::fmt::print<"{}">(
		locked_sink{__builtin_addressof(empty_locked)},
		::fast_io::fmt::static_arg<"">);
	if (empty_locked.locks != 1u || empty_locked.unlocks != 1u ||
		empty_locked.locked || empty_locked.write.calls != 0u)
	{
		return 7;
	}

	locked_state locked{};
	::fast_io::fmt::print<format_literal>(
		locked_sink{__builtin_addressof(locked)},
		::fast_io::fmt::static_arg<"d">,
		::fast_io::fmt::static_arg<"b">);
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

	write_state aggregate_write{};
	::fast_io::fmt::print<aggregate_format>(
		write_sink{__builtin_addressof(aggregate_write)},
		::fast_io::fmt::static_arg<aggregate_values>);
	if (aggregate_write.calls != 1u ||
		aggregate_write.source != aggregate_static_program::storage.data() ||
		::std::string_view{
			aggregate_write.bytes.data(), aggregate_write.size} !=
			"values=[1, 15, 255]")
	{
		return 11;
	}

	constexpr ::std::string_view partial_dynamic{"run"};
	write_state partial_write{};
	::fast_io::fmt::print<partial_format_literal>(
		write_sink{__builtin_addressof(partial_write)},
		::fast_io::fmt::static_arg<42u>,
		::std::string_view{partial_dynamic});
	if (partial_write.calls != 3u ||
		::std::string_view{partial_write.bytes.data(), partial_write.size} !=
			"A42BrunC")
	{
		return 12;
	}

	partial_status_state partial_status{};
	::fast_io::fmt::print<partial_format_literal>(
		partial_status_sink{__builtin_addressof(partial_status)},
		::fast_io::fmt::static_arg<42u>,
		::std::string_view{partial_dynamic});
	if (partial_status.calls != 1u ||
		partial_status.prefix != partial_prefix::storage.data() ||
		partial_status.dynamic != partial_dynamic.data() ||
		partial_status.dynamic_size != partial_dynamic.size() ||
		partial_status.suffix != partial_suffix::storage.data())
	{
		return 13;
	}

	scatter_state partial_scatter{};
	::fast_io::fmt::print<partial_format_literal>(
		scatter_sink{__builtin_addressof(partial_scatter)},
		::fast_io::fmt::static_arg<42u>,
		::std::string_view{partial_dynamic});
	if (partial_scatter.calls != 1u || partial_scatter.count != 3u ||
		partial_scatter.sources[0] != partial_prefix::storage.data() ||
		partial_scatter.sources[1] != partial_dynamic.data() ||
		partial_scatter.sources[2] != partial_suffix::storage.data() ||
		partial_scatter.sizes[0] != partial_prefix::size ||
		partial_scatter.sizes[1] != partial_dynamic.size() ||
		partial_scatter.sizes[2] != partial_suffix::size ||
		::std::string_view{partial_scatter.bytes.data(),
						   partial_scatter.size} != "A42BrunC")
	{
		return 14;
	}

	locked_state partial_locked{};
	::fast_io::fmt::print<partial_format_literal>(
		locked_sink{__builtin_addressof(partial_locked)},
		::fast_io::fmt::static_arg<42u>,
		::std::string_view{partial_dynamic});
	if (partial_locked.locks != 1u || partial_locked.unlocks != 1u ||
		partial_locked.locked || partial_locked.write.calls != 3u ||
		::std::string_view{partial_locked.write.bytes.data(),
						   partial_locked.write.size} != "A42BrunC")
	{
		return 15;
	}

	static constexpr char policy_prefix[]{'A', 'B'};
	constexpr ::std::string_view policy_tail{"tail"};
	auto const policy_fixed{
		::fast_io::manipulators::static_scatter_t<char, 2u>{policy_prefix}};
	write_state policy_write{};
	::fast_io::io::print(
		write_sink{__builtin_addressof(policy_write)}, policy_fixed, 42u,
		policy_tail);
	if (policy_write.calls != 2u || policy_write.sizes[0] != 4u ||
		policy_write.sizes[1] != 4u ||
		::std::string_view{policy_write.bytes.data(), policy_write.size} !=
			"AB42tail")
	{
		return 16;
	}

	scatter_state policy_scatter{};
	::fast_io::io::print(
		scatter_sink{__builtin_addressof(policy_scatter)}, policy_fixed, 42u,
		policy_tail);
	if (policy_scatter.calls != 1u || policy_scatter.count != 3u ||
		policy_scatter.sources[0] != policy_prefix ||
		policy_scatter.sizes[0] != 2u || policy_scatter.sizes[1] != 2u ||
		policy_scatter.sources[2] != policy_tail.data() ||
		policy_scatter.sizes[2] != policy_tail.size() ||
		::std::string_view{policy_scatter.bytes.data(), policy_scatter.size} !=
			"AB42tail")
	{
		return 17;
	}

	write_state dynamic_write{};
	::fast_io::io::print(
		write_sink{__builtin_addressof(dynamic_write)}, policy_fixed,
		dynamic_text{"dyn"}, policy_tail);
	if (dynamic_write.calls != 2u || dynamic_write.sizes[0] != 5u ||
		dynamic_write.sizes[1] != policy_tail.size() ||
		::std::string_view{dynamic_write.bytes.data(), dynamic_write.size} !=
			"ABdyntail")
	{
		return 18;
	}

	scatter_state dynamic_scatter{};
	::fast_io::io::print(
		scatter_sink{__builtin_addressof(dynamic_scatter)}, policy_fixed,
		dynamic_text{"dyn"}, policy_tail);
	if (dynamic_scatter.calls != 1u || dynamic_scatter.count != 3u ||
		dynamic_scatter.sources[0] != policy_prefix ||
		dynamic_scatter.sizes[0] != 2u || dynamic_scatter.sizes[1] != 3u ||
		dynamic_scatter.sources[2] != policy_tail.data() ||
		dynamic_scatter.sizes[2] != policy_tail.size() ||
		::std::string_view{dynamic_scatter.bytes.data(),
						   dynamic_scatter.size} != "ABdyntail")
	{
		return 19;
	}

	scatter_state runtime_scatter{};
	::fast_io::io::print(
		scatter_sink{__builtin_addressof(runtime_scatter)}, policy_fixed,
		runtime_scatter_text{"dyn"}, policy_tail);
	if (runtime_scatter.calls != 1u || runtime_scatter.count != 3u ||
		runtime_scatter.sources[0] != policy_prefix ||
		runtime_scatter.sizes[0] != 2u || runtime_scatter.sizes[1] != 3u ||
		runtime_scatter.sources[2] != policy_tail.data() ||
		runtime_scatter.sizes[2] != policy_tail.size() ||
		::std::string_view{runtime_scatter.bytes.data(),
						   runtime_scatter.size} != "ABdyntail")
	{
		return 20;
	}

	scatter_state byte_policy_scatter{};
	::fast_io::io::print(
		byte_scatter_sink{__builtin_addressof(byte_policy_scatter)},
		policy_fixed, 42u, policy_tail);
	if (byte_policy_scatter.calls != 1u ||
		byte_policy_scatter.count != 3u ||
		byte_policy_scatter.sources[0] != policy_prefix ||
		byte_policy_scatter.sizes[0] != 2u ||
		byte_policy_scatter.sizes[1] != 2u ||
		byte_policy_scatter.sources[2] != policy_tail.data() ||
		byte_policy_scatter.sizes[2] != policy_tail.size() ||
		::std::string_view{byte_policy_scatter.bytes.data(),
						   byte_policy_scatter.size} != "AB42tail")
	{
		return 21;
	}

	scatter_state byte_dynamic_scatter{};
	::fast_io::io::print(
		byte_scatter_sink{__builtin_addressof(byte_dynamic_scatter)},
		policy_fixed, dynamic_text{"dyn"}, policy_tail);
	if (byte_dynamic_scatter.calls != 1u ||
		byte_dynamic_scatter.count != 3u ||
		byte_dynamic_scatter.sources[0] != policy_prefix ||
		byte_dynamic_scatter.sizes[0] != 2u ||
		byte_dynamic_scatter.sizes[1] != 3u ||
		byte_dynamic_scatter.sources[2] != policy_tail.data() ||
		byte_dynamic_scatter.sizes[2] != policy_tail.size() ||
		::std::string_view{byte_dynamic_scatter.bytes.data(),
						   byte_dynamic_scatter.size} != "ABdyntail")
	{
		return 22;
	}

	scatter_state byte_runtime_scatter{};
	::fast_io::io::print(
		byte_scatter_sink{__builtin_addressof(byte_runtime_scatter)},
		policy_fixed, runtime_scatter_text{"dyn"}, policy_tail);
	if (byte_runtime_scatter.calls != 1u ||
		byte_runtime_scatter.count != 3u ||
		byte_runtime_scatter.sources[0] != policy_prefix ||
		byte_runtime_scatter.sizes[0] != 2u ||
		byte_runtime_scatter.sizes[1] != 3u ||
		byte_runtime_scatter.sources[2] != policy_tail.data() ||
		byte_runtime_scatter.sizes[2] != policy_tail.size() ||
		::std::string_view{byte_runtime_scatter.bytes.data(),
						   byte_runtime_scatter.size} != "ABdyntail")
	{
		return 23;
	}

	precision_status_state precision_status{};
	::fast_io::fmt::print<precision_format_literal>(
		precision_status_sink{__builtin_addressof(precision_status)},
		::fast_io::fmt::static_arg<"xxx">,
		::fast_io::fmt::static_arg<42u>, 3.14);
	if (precision_status.calls != 1u ||
		precision_status.current != precision_status.put_area.data())
	{
		return 24;
	}

	short_precision_state short_precision{};
	short_precision_sink short_precision_output{
		__builtin_addressof(short_precision)};
	::fast_io::fmt::print<precision_format_literal>(
		short_precision_output, ::fast_io::fmt::static_arg<"xxx">,
		::fast_io::fmt::static_arg<42u>, 3.14);
	// The complete record does not fit the eight-byte put area.  The whole-record
	// overflow strategy writes it once and leaves no buffered prefix behind;
	// two historical overflow calls are not part of the sink's semantics.
	if (short_precision.current != short_precision.put_area.data())
	{
		return 25;
	}
	flush_short_precision(short_precision_output);
	if (short_precision.write.calls != 1u ||
		short_precision.current != short_precision.put_area.data() ||
		short_precision.write.size != precision_expected.size() ||
		::std::string_view{short_precision.write.bytes.data(),
						  short_precision.write.size} != precision_expected)
	{
		return 25;
	}

	locked_state precision_locked{};
	::fast_io::fmt::print<precision_format_literal>(
		locked_sink{__builtin_addressof(precision_locked)},
		::fast_io::fmt::static_arg<"xxx">,
		::fast_io::fmt::static_arg<42u>, 3.14);
	if (precision_locked.locks != 1u || precision_locked.unlocks != 1u ||
		precision_locked.locked || precision_locked.outer_status_calls != 0u ||
		::std::string_view{precision_locked.write.bytes.data(),
						  precision_locked.write.size} != precision_expected)
	{
		return 26;
	}

	char precision_line_storage[64u]{};
	::fast_io::obuffer_view precision_line_buffer{
		precision_line_storage, precision_line_storage + 64u};
	using precision_scalar_type =
		decltype(::fast_io::mnp::fixed(3.14, 2u));
	::fast_io::io::println(
		precision_line_buffer,
		::fast_io::manipulators::static_scatter_t<
			char, sizeof(precision_prefix) - 1u>{precision_prefix},
		::fast_io::manipulators::format_scalar_t<
			precision_scalar_type, 0u, false>{
			::fast_io::mnp::fixed(3.14, 2u)});
	if (::std::string_view{precision_line_storage,
						  precision_line_buffer.size()} !=
		"user=xxx id=0000002a score=3.14\n")
	{
		return 27;
	}
}
