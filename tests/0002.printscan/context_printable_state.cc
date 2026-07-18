#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#if defined(__linux__)
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <fast_io.h>

namespace
{

struct capture_sink
{
	using output_char_type = char;
	::std::string *output;
};

inline constexpr capture_sink output_stream_ref_define(capture_sink sink) noexcept
{
	return sink;
}

inline void write_all_overflow_define(capture_sink sink, char const *first, char const *last)
{
	sink.output->append(first, last);
}

struct buffered_sink_state
{
	char *begin;
	char *current;
	char *end;
};

struct buffered_sink
{
	using output_char_type = char;
	buffered_sink_state *state;
};

inline constexpr buffered_sink output_stream_ref_define(buffered_sink sink) noexcept
{
	return sink;
}

[[maybe_unused]] inline constexpr char *obuffer_begin(buffered_sink sink) noexcept
{
	return sink.state->begin;
}

inline constexpr char *obuffer_curr(buffered_sink sink) noexcept
{
	return sink.state->current;
}

inline constexpr char *obuffer_end(buffered_sink sink) noexcept
{
	return sink.state->end;
}

inline constexpr void obuffer_set_curr(buffered_sink sink, char *current) noexcept
{
	sink.state->current = current;
}

inline void write_all_overflow_define(buffered_sink, char const *, char const *)
{
	// The test gives the reusable put area enough capacity for the complete payload. Reaching overflow would mean that
	// context dispatch stopped using the advertised buffered protocol.
	::fast_io::fast_terminate();
}

struct print_observation
{
	bool aligned{true};
	bool value_initialized{true};
	::std::size_t calls{};
};

struct large_context_value
{
	::std::string_view text;
	print_observation *observation;
};

struct lvalue_context_value
{
	::std::string_view text;
	print_observation *observation;

	// A nontrivial source is forwarded through `parameter<T&>`, exercising the semantic lvalue adapter rather than
	// the directly copied value path.
	inline ~lvalue_context_value()
	{}
};

inline ::std::size_t large_state_destructions{};

struct alignas(8192) large_print_context_state
{
	// Neither member has an initializer. The first producer call therefore proves that the owner used `State{}`
	// semantics instead of default-initializing indeterminate storage.
	::std::array<::std::byte, 64u * 1024u> working_set;
	::std::size_t position;

	inline large_print_context_state() = default;

private:
	template <typename value_type>
	inline ::fast_io::context_print_result<char *> emit(value_type const &value, char *first, char *last) noexcept
	{
		value.observation->aligned = value.observation->aligned &&
			(reinterpret_cast<::std::uintptr_t>(this) % alignof(large_print_context_state) == 0u);
		if (position == 0u)
		{
			value.observation->value_initialized = value.observation->value_initialized &&
				working_set.front() == ::std::byte{};
		}
		++value.observation->calls;
		auto const remaining{value.text.size() - position};
		auto const available{static_cast<::std::size_t>(last - first)};
		auto const count{remaining < available ? remaining : available};
		for (::std::size_t i{}; i != count; ++i)
		{
			first[i] = value.text[position + i];
		}
		position += count;
		return {first + count, position == value.text.size()};
	}

public:
	inline ::fast_io::context_print_result<char *> print_context_define(
		large_context_value value, char *first, char *last) noexcept
	{
		return emit(value, first, last);
	}

	inline ::fast_io::context_print_result<char *> print_context_define(
		lvalue_context_value const &value, char *first, char *last) noexcept
	{
		return emit(value, first, last);
	}

	inline ~large_print_context_state()
	{
		++large_state_destructions;
	}
};

inline constexpr ::fast_io::io_type_t<large_print_context_state>
print_context_type(::fast_io::io_reserve_type_t<char, large_context_value>) noexcept
{
	return {};
}

inline constexpr ::fast_io::io_type_t<large_print_context_state>
print_context_type(::fast_io::io_reserve_type_t<char, lvalue_context_value>) noexcept
{
	return {};
}

inline constexpr ::std::size_t
print_context_static_buffer_size(::fast_io::io_reserve_type_t<char, large_context_value>) noexcept
{
	return 3u;
}

inline constexpr ::std::size_t
print_context_static_buffer_size(::fast_io::io_reserve_type_t<char, lvalue_context_value>) noexcept
{
	return 3u;
}

static_assert(::fast_io::context_printable<char, large_context_value>);
static_assert(::fast_io::context_printable<char, lvalue_context_value>);
static_assert(!::fast_io::details::print_context_state_inline_v<large_print_context_state>);

struct constexpr_large_print_state
{
	::std::array<::std::byte, 4097u> working_set;
	::std::size_t phase;
};

static_assert(!::fast_io::details::print_context_state_inline_v<constexpr_large_print_state>);
static_assert(::fast_io::details::with_print_context_state<constexpr_large_print_state>(
	[](constexpr_large_print_state &state) constexpr {
		return state.phase == 0u && state.working_set.front() == ::std::byte{};
	}));

struct one_window_value
{
	::std::size_t *observed_window;
};

struct one_window_state
{
	inline constexpr ::fast_io::context_print_result<char *> print_context_define(
		one_window_value value, char *first, char *last) noexcept
	{
		*value.observed_window = static_cast<::std::size_t>(last - first);
		if (first == last)
		{
			return {first, false};
		}
		*first = 'X';
		return {first + 1, true};
	}
};

inline constexpr ::fast_io::io_type_t<one_window_state>
print_context_type(::fast_io::io_reserve_type_t<char, one_window_value>) noexcept
{
	return {};
}

inline constexpr ::std::size_t
print_context_static_buffer_size(::fast_io::io_reserve_type_t<char, one_window_value>) noexcept
{
	return 1u;
}

static_assert(::fast_io::context_printable<char, one_window_value>);
static_assert(::fast_io::details::decay::context_print_static_buffer_size_v<false, char, one_window_value> == 1u);
static_assert(::fast_io::details::decay::context_print_static_buffer_size_v<true, char, one_window_value> == 2u);

struct cv_print_value
{};

struct cv_print_state
{
	inline constexpr ::fast_io::context_print_result<char *> print_context_define(
		cv_print_value, char *first, char *) const noexcept
	{
		return {first, true};
	}
};

[[maybe_unused]] inline constexpr ::fast_io::io_type_t<cv_print_state const>
print_context_type(::fast_io::io_reserve_type_t<char, cv_print_value>) noexcept
{
	return {};
}

struct array_print_value
{};

struct array_print_state
{};

[[maybe_unused]] inline constexpr ::fast_io::io_type_t<array_print_state[2u]>
print_context_type(::fast_io::io_reserve_type_t<char, array_print_value>) noexcept
{
	return {};
}

struct reference_print_value
{};

struct reference_print_state
{
	inline constexpr ::fast_io::context_print_result<char *> print_context_define(
		reference_print_value, char *first, char *) noexcept
	{
		return {first, true};
	}
};

[[maybe_unused]] inline constexpr ::fast_io::io_type_t<reference_print_state &>
print_context_type(::fast_io::io_reserve_type_t<char, reference_print_value>) noexcept
{
	return {};
}

struct nondefault_print_value
{};

struct nondefault_print_state
{
	nondefault_print_state() = delete;

	inline constexpr ::fast_io::context_print_result<char *> print_context_define(
		nondefault_print_value, char *first, char *) noexcept
	{
		return {first, true};
	}
};

[[maybe_unused]] inline constexpr ::fast_io::io_type_t<nondefault_print_state>
print_context_type(::fast_io::io_reserve_type_t<char, nondefault_print_value>) noexcept
{
	return {};
}

static_assert(!::fast_io::context_printable<char, cv_print_value>);
static_assert(!::fast_io::context_printable<char, array_print_value>);
static_assert(!::fast_io::context_printable<char, reference_print_value>);
static_assert(!::fast_io::context_printable<char, nondefault_print_value>);

struct no_progress_value
{};

struct no_progress_state
{
	inline constexpr ::fast_io::context_print_result<char *> print_context_define(
		no_progress_value, char *first, char *) noexcept
	{
		return {first, false};
	}
};

inline constexpr ::fast_io::io_type_t<no_progress_state>
print_context_type(::fast_io::io_reserve_type_t<char, no_progress_value>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::size_t
print_context_static_buffer_size(::fast_io::io_reserve_type_t<char, no_progress_value>) noexcept
{
	return 1u;
}

struct out_of_range_value
{};

struct out_of_range_state
{
	inline ::fast_io::context_print_result<char *> print_context_define(
		out_of_range_value, char *, char *last) noexcept
	{
		// Form the invalid result by address conversion rather than out-of-bounds pointer arithmetic; the producer does
		// not access it, and the dispatcher must reject it before committing the cursor.
		auto const address{reinterpret_cast<::std::uintptr_t>(last) + sizeof(char)};
		return {reinterpret_cast<char *>(address), true};
	}
};

inline constexpr ::fast_io::io_type_t<out_of_range_state>
print_context_type(::fast_io::io_reserve_type_t<char, out_of_range_value>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::size_t
print_context_static_buffer_size(::fast_io::io_reserve_type_t<char, out_of_range_value>) noexcept
{
	return 1u;
}

static_assert(::fast_io::context_printable<char, no_progress_value>);
static_assert(::fast_io::context_printable<char, out_of_range_value>);

inline void test_large_state_all_dispatch_sites()
{
	auto const destruction_base{large_state_destructions};

	print_observation unbuffered_observation;
	::std::string unbuffered_output;
	::fast_io::print(capture_sink{__builtin_addressof(unbuffered_output)}, "[",
		large_context_value{"abcdef", __builtin_addressof(unbuffered_observation)}, "]");
	assert(unbuffered_output == "[abcdef]");
	assert(unbuffered_observation.aligned && unbuffered_observation.value_initialized);
	assert(unbuffered_observation.calls != 0u);
	assert(large_state_destructions == destruction_base + 1u);

	char storage[64u]{};
	buffered_sink_state buffered_state{storage, storage, storage + sizeof(storage)};
	print_observation buffered_observation;
	::fast_io::println(buffered_sink{__builtin_addressof(buffered_state)},
		large_context_value{"buffered", __builtin_addressof(buffered_observation)});
	assert(::std::string_view(storage, static_cast<::std::size_t>(buffered_state.current - storage)) ==
		"buffered\n");
	assert(buffered_observation.aligned && buffered_observation.value_initialized);
	assert(large_state_destructions == destruction_base + 2u);

	print_observation concat_observation;
	auto const concatenated{::fast_io::concat_std(
		large_context_value{"concat", __builtin_addressof(concat_observation)})};
	assert(concatenated == "concat");
	assert(concat_observation.aligned && concat_observation.value_initialized);
	// concat's generic fallback is intentionally covered here: a completed state lifetime proves that it delegates to
	// context printing rather than treating the protocol as an exact-size string producer.
	assert(large_state_destructions == destruction_base + 3u);

	print_observation lvalue_observation;
	lvalue_context_value lvalue{"lvalue", __builtin_addressof(lvalue_observation)};
	::std::string lvalue_output;
	::fast_io::print(capture_sink{__builtin_addressof(lvalue_output)}, lvalue);
	assert(lvalue_output == "lvalue");
	assert(lvalue_observation.aligned && lvalue_observation.value_initialized);
	// The parameter adapter embeds the large child state; one destruction proves that the shared owner handled the
	// aggregate instead of constructing the child directly in the print frame.
	assert(large_state_destructions == destruction_base + 4u);
}

inline void test_one_character_window_with_newline()
{
	::std::size_t observed_window{};
	::std::string output;
	::fast_io::println(capture_sink{__builtin_addressof(output)},
		one_window_value{__builtin_addressof(observed_window)});
	assert(output == "X\n");
	assert(observed_window == 1u);
}

#if defined(__linux__)
template <typename callback_type>
inline void expect_contract_termination(callback_type callback)
{
	auto const child{::fork()};
	assert(child >= 0);
	if (child == 0)
	{
		// Contract tests intentionally trap. Disable core files so a successful regression run leaves no large artifact.
		::rlimit limit{};
		::setrlimit(RLIMIT_CORE, __builtin_addressof(limit));
		callback();
		::_exit(0);
	}
	int status{};
	auto const waited{::waitpid(child, __builtin_addressof(status), 0)};
	assert(waited == child);
	assert(!WIFEXITED(status) || WEXITSTATUS(status) != 0);
}

inline void test_malformed_producers_terminate()
{
	expect_contract_termination([] {
		::std::string output;
		::fast_io::print(capture_sink{__builtin_addressof(output)}, no_progress_value{});
	});
	expect_contract_termination([] {
		::std::string output;
		::fast_io::print(capture_sink{__builtin_addressof(output)}, out_of_range_value{});
	});
}
#endif

} // namespace

int main()
{
	test_large_state_all_dispatch_sites();
	test_one_character_window_with_newline();
#if defined(__linux__)
	test_malformed_producers_terminate();
#endif
}
