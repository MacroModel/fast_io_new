#include <cassert>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace
{

struct text_token
{};

inline constexpr ::std::size_t
	print_reserve_size(::fast_io::io_reserve_type_t<char, text_token>) noexcept
{
	return 1u;
}

inline constexpr char *
print_reserve_define(::fast_io::io_reserve_type_t<char, text_token>, char *destination, text_token) noexcept
{
	*destination = 'T';
	return destination + 1u;
}

struct output_state
{
	::std::string text;
	::std::size_t lock_calls{};
	::std::size_t unlock_calls{};
	::std::size_t outer_status_calls{};
	bool locked{};
};

struct exact_status_sink
{
	using output_char_type = char;
	output_state *state;
};

inline constexpr exact_status_sink output_stream_ref_define(exact_status_sink sink) noexcept
{
	return sink;
}

template <bool line>
inline void status_print_define(exact_status_sink sink, text_token)
{
	sink.state->text.append("status");
	if constexpr (line)
	{
		sink.state->text.push_back('\n');
	}
}

struct empty_status_sink
{
	using output_char_type = char;
	output_state *state;
};

inline constexpr empty_status_sink output_stream_ref_define(empty_status_sink sink) noexcept
{
	return sink;
}

template <bool line>
inline void status_print_define(empty_status_sink sink)
{
	sink.state->text.append(line ? "empty-line" : "empty");
}

struct selective_status_sink
{
	using output_char_type = char;
	output_state *state;
};

inline constexpr selective_status_sink output_stream_ref_define(selective_status_sink sink) noexcept
{
	return sink;
}

template <bool line>
	requires(!line)
inline void status_print_define(selective_status_sink sink, text_token)
{
	sink.state->text.push_back('S');
}

inline void write_all_overflow_define(
	selective_status_sink sink, char const *first, char const *last)
{
	sink.state->text.append(first, last);
}

struct false_positive_status_sink
{
	using output_char_type = char;
	output_state *state;
};

inline constexpr false_positive_status_sink
output_stream_ref_define(false_positive_status_sink sink) noexcept
{
	return sink;
}

// This is exactly the historical dummy probe and deliberately has no relation
// to the text_token expression used below.
template <bool line>
	requires(line)
inline void status_print_define(false_positive_status_sink, int) noexcept
{}

inline void write_all_overflow_define(
	false_positive_status_sink sink, char const *first, char const *last)
{
	sink.state->text.append(first, last);
}

struct unlocked_status_sink
{
	using output_char_type = char;
	output_state *state;
};

struct locked_status_sink
{
	using output_char_type = char;
	output_state *state;
};

struct counting_mutex_ref
{
	output_state *state;

	inline void lock() const noexcept
	{
		assert(!state->locked);
		state->locked = true;
		++state->lock_calls;
	}

	inline void unlock() const noexcept
	{
		assert(state->locked);
		state->locked = false;
		++state->unlock_calls;
	}
};

inline constexpr locked_status_sink output_stream_ref_define(locked_status_sink sink) noexcept
{
	return sink;
}

inline constexpr counting_mutex_ref output_stream_mutex_ref_define(locked_status_sink sink) noexcept
{
	return {sink.state};
}

inline constexpr unlocked_status_sink output_stream_unlocked_ref_define(locked_status_sink sink) noexcept
{
	return {sink.state};
}

template <bool line>
inline void status_print_define(unlocked_status_sink sink, text_token)
{
	assert(sink.state->locked);
	sink.state->text.append(line ? "locked-line" : "locked");
}

template <bool line>
inline void status_print_define(locked_status_sink sink, text_token)
{
	// A mutex-bearing object may accidentally expose a forwarding status CPO on the outer wrapper.  The dispatcher
	// must never select it: doing so would make the result depend on overload visibility and bypass synchronization.
	(void)line;
	++sink.state->outer_status_calls;
	sink.state->text.append("unlocked-outer-status");
}

struct runtime_reserve
{};

inline ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<char, runtime_reserve>) noexcept
{
	return 1u;
}

[[maybe_unused]] inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, runtime_reserve>, char *destination, runtime_reserve) noexcept
{
	return destination + 1u;
}

struct zero_reserve
{};

inline constexpr ::std::size_t
	print_reserve_size(::fast_io::io_reserve_type_t<char, zero_reserve>) noexcept
{
	return 0u;
}

[[maybe_unused]] inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, zero_reserve>, char *destination, zero_reserve) noexcept
{
	return destination;
}

struct oversized_reserve
{};

inline constexpr ::std::size_t
	print_reserve_size(::fast_io::io_reserve_type_t<char, oversized_reserve>) noexcept
{
	return static_cast<::std::size_t>(PTRDIFF_MAX);
}

[[maybe_unused]] inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, oversized_reserve>, char *destination, oversized_reserve) noexcept
{
	return destination;
}

template <typename tag_type>
inline constexpr ::fast_io::basic_reserve_scatters_define_result<char>
print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, tag_type>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve, tag_type) noexcept
{
	return {scatters, reserve};
}

struct runtime_static_scatters
{};

inline ::fast_io::reserve_scatters_size_result
	print_reserve_scatters_size(
		::fast_io::io_reserve_type_t<char, runtime_static_scatters>) noexcept
{
	return {1u, 0u};
}

struct zero_static_scatters
{};

inline constexpr ::fast_io::reserve_scatters_size_result
	print_reserve_scatters_size(
		::fast_io::io_reserve_type_t<char, zero_static_scatters>) noexcept
{
	return {0u, 0u};
}

struct oversized_static_scatters
{};

inline constexpr ::fast_io::reserve_scatters_size_result
	print_reserve_scatters_size(
		::fast_io::io_reserve_type_t<char, oversized_static_scatters>) noexcept
{
	return {static_cast<::std::size_t>(PTRDIFF_MAX), 0u};
}

struct valid_static_scatters
{};

inline constexpr ::fast_io::reserve_scatters_size_result
	print_reserve_scatters_size(
		::fast_io::io_reserve_type_t<char, valid_static_scatters>) noexcept
{
	return {1u, 0u};
}

struct oversized_scatter_allocation
{};

inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, oversized_scatter_allocation>) noexcept
{
	return {
		(::std::numeric_limits<::std::size_t>::max)() /
				sizeof(::fast_io::basic_io_scatter_t<char>) +
			1u,
		0u};
}

struct oversized_wide_reserve_allocation
{};

inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char32_t, oversized_wide_reserve_allocation>) noexcept
{
	return {
		1u,
		(::std::numeric_limits<::std::size_t>::max)() / sizeof(char32_t) + 1u};
}

inline constexpr ::fast_io::basic_reserve_scatters_define_result<char32_t>
print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char32_t, oversized_wide_reserve_allocation>,
	::fast_io::basic_io_scatter_t<char32_t> *scatters, char32_t *reserve,
	oversized_wide_reserve_allocation) noexcept
{
	return {scatters, reserve};
}

struct synthetic_transcoder
{
	using from_char_type = char;
	using to_char_type = char;
	using from_value_type = char;
	using to_value_type = char;
};

inline constexpr ::fast_io::transcode_result<char, char> transcode_decay_define(
	synthetic_transcoder, char const *from_first, char const *, char *to_first, char *) noexcept
{
	return {from_first, to_first};
}

inline constexpr ::std::size_t transcode_min_tosize_decay_define(
	::fast_io::transcode_reserve_t<synthetic_transcoder>) noexcept
{
	return 1u;
}

inline constexpr char const *transcode_imaginary_decay_define(
	synthetic_transcoder, char const *from_first, char const *, ::std::size_t) noexcept
{
	return from_first;
}

using recognized_transcode_request =
	::fast_io::manipulators::basic_transcoder_t<char, synthetic_transcoder>;

struct borrowed_text_source
{
	char const *data;
	::std::size_t size;
};

inline constexpr ::fast_io::basic_io_scatter_t<char>
print_alias_define(::fast_io::io_alias_t, borrowed_text_source &source) noexcept
{
	return {source.data, source.size};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, borrowed_text_source>) noexcept
{
	return {};
}

template <typename source_type, typename transcoder_type>
concept can_make_transcode_request = requires(source_type &&source, transcoder_type &&transcoder) {
	::fast_io::manipulators::transcode(
		::std::forward<source_type>(source), ::std::forward<transcoder_type>(transcoder));
};

static_assert(::fast_io::reserve_printable<char, text_token>);
static_assert(!::fast_io::reserve_printable<char, runtime_reserve>);
static_assert(!::fast_io::reserve_printable<char, zero_reserve>);
static_assert(!::fast_io::reserve_printable<char, oversized_reserve>);

static_assert(!::fast_io::reserve_scatters_printable<char, runtime_static_scatters>);
static_assert(!::fast_io::reserve_scatters_printable<char, zero_static_scatters>);
static_assert(!::fast_io::reserve_scatters_printable<char, oversized_static_scatters>);
static_assert(!::fast_io::reserve_scatters_printable<char, oversized_scatter_allocation>);
static_assert(!::fast_io::reserve_scatters_printable<
			  char32_t, oversized_wide_reserve_allocation>);
static_assert(::fast_io::reserve_scatters_printable<char, valid_static_scatters>);

static_assert(::fast_io::transcode_imaginary_protocol<char, recognized_transcode_request>);
static_assert(!::fast_io::details::decay::print_freestanding_decay_param_okay_single<
			  char, recognized_transcode_request>::value);
static_assert(can_make_transcode_request<borrowed_text_source &, synthetic_transcoder &>);
static_assert(!can_make_transcode_request<borrowed_text_source, synthetic_transcoder &>);
static_assert(!can_make_transcode_request<borrowed_text_source &, synthetic_transcoder>);

static_assert(::fast_io::operations::decay::defines::has_status_print_define<
			  false, exact_status_sink, text_token>);
static_assert(::fast_io::operations::decay::defines::has_status_print_define<
			  true, exact_status_sink, text_token>);
static_assert(::fast_io::operations::decay::defines::has_status_print_define<
			  false, empty_status_sink>);
static_assert(::fast_io::operations::decay::defines::has_status_print_define<
			  false, selective_status_sink, text_token>);
static_assert(!::fast_io::operations::decay::defines::has_status_print_define<
			  true, selective_status_sink, text_token>);
static_assert(::fast_io::operations::decay::defines::has_status_print_define<
			  true, false_positive_status_sink, int>);
static_assert(!::fast_io::operations::decay::defines::has_status_print_define<
			  false, false_positive_status_sink, text_token>);

} // namespace

int main()
{
	output_state exact;
	::fast_io::print(exact_status_sink{__builtin_addressof(exact)}, text_token{});
	::fast_io::println(exact_status_sink{__builtin_addressof(exact)}, text_token{});
	assert(exact.text == "statusstatus\n");

	output_state empty;
	::fast_io::print(empty_status_sink{__builtin_addressof(empty)});
	::fast_io::println(empty_status_sink{__builtin_addressof(empty)});
	assert(empty.text == "emptyempty-line");

	output_state selective;
	::fast_io::print(selective_status_sink{__builtin_addressof(selective)}, text_token{});
	::fast_io::println(selective_status_sink{__builtin_addressof(selective)}, text_token{});
	assert(selective.text == "ST\n");

	output_state false_positive;
	::fast_io::print(false_positive_status_sink{__builtin_addressof(false_positive)}, text_token{});
	::fast_io::println(false_positive_status_sink{__builtin_addressof(false_positive)}, text_token{});
	assert(false_positive.text == "TT\n");

	output_state locked;
	::fast_io::println(locked_status_sink{__builtin_addressof(locked)}, text_token{});
	assert(locked.text == "locked-line");
	assert(locked.lock_calls == 1u && locked.unlock_calls == 1u && !locked.locked);
	assert(locked.outer_status_calls == 0u);
}
