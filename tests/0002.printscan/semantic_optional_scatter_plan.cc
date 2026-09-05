#include <cassert>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace
{

using scatter = ::fast_io::basic_io_scatter_t<char>;

struct capture_state
{
	::std::string output;
	::std::string operations;
	::std::size_t scatter_calls{};
	::std::size_t scalar_calls{};
	::std::size_t lock_calls{};
	::std::size_t unlock_calls{};
	bool locked{};
	bool lock_required{};
};

struct scatter_sink
{
	using output_char_type = char;
	capture_state *state{};
};

inline constexpr scatter_sink output_stream_ref_define(scatter_sink sink) noexcept
{
	return sink;
}

inline constexpr ::std::true_type print_semantic_optional_scatter_plan_stream(
	::fast_io::io_reserve_type_t<char, scatter_sink>) noexcept
{
	// This fixture has no status-print owner and its only native operation consumes the compact descriptor sequence.
	return {};
}

inline constexpr ::std::true_type
print_semantic_optional_scatter_barrier_plan_stream(
	::fast_io::io_reserve_type_t<char, scatter_sink>) noexcept
{
	// Direct-only fixture leaves are hard control boundaries; resuming on this observer preserves descriptor order.
	return {};
}

inline constexpr ::std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char, scatter_sink>) noexcept
{
	// The fixture consumes every descriptor and payload synchronously, matching the POSIX observer proof.
	return {};
}

inline void write_all_overflow_define(
	scatter_sink sink, char const *first, char const *last)
{
	assert(!sink.state->lock_required || sink.state->locked);
	++sink.state->scalar_calls;
	sink.state->operations.push_back('w');
	sink.state->output.append(
		first, static_cast<::std::size_t>(last - first));
}

inline void scatter_write_all_overflow_define(
	scatter_sink sink, scatter const *scatters, ::std::size_t count)
{
	assert(!sink.state->lock_required || sink.state->locked);
	++sink.state->scatter_calls;
	sink.state->operations.push_back('s');
	for (::std::size_t index{}; index != count; ++index)
	{
		assert(scatters[index].len == 0u || scatters[index].base != nullptr);
		sink.state->output.append(
			scatters[index].base, scatters[index].len);
	}
}

struct fallback_scatter_sink
{
	using output_char_type = char;
	capture_state *state{};
};

inline constexpr fallback_scatter_sink output_stream_ref_define(
	fallback_scatter_sink sink) noexcept
{
	return sink;
}

inline constexpr ::std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char, fallback_scatter_sink>) noexcept
{
	return {};
}

inline void write_all_overflow_define(
	fallback_scatter_sink sink, char const *first, char const *last)
{
	++sink.state->scalar_calls;
	sink.state->operations.push_back('w');
	sink.state->output.append(
		first, static_cast<::std::size_t>(last - first));
}

inline void scatter_write_all_overflow_define(
	fallback_scatter_sink sink, scatter const *scatters,
	::std::size_t count)
{
	++sink.state->scatter_calls;
	sink.state->operations.push_back('s');
	for (::std::size_t index{}; index != count; ++index)
	{
		sink.state->output.append(
			scatters[index].base, scatters[index].len);
	}
}

struct coalescing_sink
{
	using output_char_type = char;
	capture_state *state{};
};

inline constexpr coalescing_sink output_stream_ref_define(
	coalescing_sink sink) noexcept
{
	return sink;
}

inline constexpr ::std::true_type print_semantic_optional_scatter_plan_stream(
	::fast_io::io_reserve_type_t<char, coalescing_sink>) noexcept
{
	// The direct-write fixture has no status owner and declares the same complete-record coalescing policy as NT files.
	return {};
}

inline constexpr ::std::true_type
print_semantic_optional_scatter_barrier_plan_stream(
	::fast_io::io_reserve_type_t<char, coalescing_sink>) noexcept
{
	// The fixture's direct-only leaf ends one coalesced prefix and the same observer accepts the following prefix.
	return {};
}

inline constexpr ::std::size_t scatter_fallback_full_output_threshold(
	::fast_io::io_reserve_type_t<char, coalescing_sink>) noexcept
{
	return 1024u;
}

inline void write_all_overflow_define(
	coalescing_sink sink, char const *first, char const *last)
{
	++sink.state->scalar_calls;
	sink.state->operations.push_back('w');
	sink.state->output.append(
		first, static_cast<::std::size_t>(last - first));
}

struct fallback_coalescing_sink
{
	using output_char_type = char;
	capture_state *state{};
};

inline constexpr fallback_coalescing_sink output_stream_ref_define(
	fallback_coalescing_sink sink) noexcept
{
	return sink;
}

inline constexpr ::std::size_t scatter_fallback_full_output_threshold(
	::fast_io::io_reserve_type_t<char,
								 fallback_coalescing_sink>) noexcept
{
	return 1024u;
}

inline void write_all_overflow_define(
	fallback_coalescing_sink sink, char const *first,
	char const *last)
{
	++sink.state->scalar_calls;
	sink.state->operations.push_back('w');
	sink.state->output.append(
		first, static_cast<::std::size_t>(last - first));
}

struct whole_coalescing_sink
{
	using output_char_type = char;
	capture_state *state{};
};

inline constexpr whole_coalescing_sink output_stream_ref_define(
	whole_coalescing_sink sink) noexcept
{
	return sink;
}

inline constexpr ::std::true_type print_semantic_optional_scatter_plan_stream(
	::fast_io::io_reserve_type_t<char, whole_coalescing_sink>) noexcept
{
	// This POSIX-like fixture consumes native scatters synchronously and owns no whole-record status customization.
	return {};
}

inline constexpr ::std::true_type
print_semantic_optional_scatter_barrier_plan_stream(
	::fast_io::io_reserve_type_t<char, whole_coalescing_sink>) noexcept
{
	// Failed whole-record materialization may resume at the same width boundaries as the ordinary control scanner.
	return {};
}

inline constexpr ::std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char, whole_coalescing_sink>) noexcept
{
	return {};
}

inline constexpr ::std::size_t full_output_coalesce_threshold(
	::fast_io::io_reserve_type_t<char, whole_coalescing_sink>) noexcept
{
	return 64u;
}

inline constexpr ::std::size_t full_output_dynamic_coalesce_threshold(
	::fast_io::io_reserve_type_t<char, whole_coalescing_sink>) noexcept
{
	return 96u;
}

inline void write_all_overflow_define(
	whole_coalescing_sink sink, char const *first,
	char const *last)
{
	++sink.state->scalar_calls;
	sink.state->operations.push_back('w');
	sink.state->output.append(
		first, static_cast<::std::size_t>(last - first));
}

inline void scatter_write_all_overflow_define(
	whole_coalescing_sink sink, scatter const *scatters,
	::std::size_t count)
{
	++sink.state->scatter_calls;
	sink.state->operations.push_back('s');
	for (::std::size_t index{}; index != count; ++index)
	{
		sink.state->output.append(
			scatters[index].base, scatters[index].len);
	}
}

struct fallback_whole_coalescing_sink
{
	using output_char_type = char;
	capture_state *state{};
};

inline constexpr fallback_whole_coalescing_sink output_stream_ref_define(
	fallback_whole_coalescing_sink sink) noexcept
{
	return sink;
}

inline constexpr ::std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char,
		fallback_whole_coalescing_sink>) noexcept
{
	return {};
}

inline constexpr ::std::size_t full_output_coalesce_threshold(
	::fast_io::io_reserve_type_t<char,
		fallback_whole_coalescing_sink>) noexcept
{
	return 64u;
}

inline constexpr ::std::size_t full_output_dynamic_coalesce_threshold(
	::fast_io::io_reserve_type_t<char,
		fallback_whole_coalescing_sink>) noexcept
{
	return 96u;
}

inline void write_all_overflow_define(
	fallback_whole_coalescing_sink sink, char const *first,
	char const *last)
{
	++sink.state->scalar_calls;
	sink.state->operations.push_back('w');
	sink.state->output.append(
		first, static_cast<::std::size_t>(last - first));
}

inline void scatter_write_all_overflow_define(
	fallback_whole_coalescing_sink sink, scatter const *scatters,
	::std::size_t count)
{
	++sink.state->scatter_calls;
	sink.state->operations.push_back('s');
	for (::std::size_t index{}; index != count; ++index)
	{
		sink.state->output.append(
			scatters[index].base, scatters[index].len);
	}
}

struct lock_proxy
{
	capture_state *state{};

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

struct locked_sink
{
	using output_char_type = char;
	capture_state *state{};
};

inline constexpr locked_sink output_stream_ref_define(locked_sink sink) noexcept
{
	return sink;
}

inline constexpr lock_proxy output_stream_mutex_ref_define(
	locked_sink sink) noexcept
{
	return {sink.state};
}

inline constexpr scatter_sink output_stream_unlocked_ref_define(
	locked_sink sink) noexcept
{
	return {sink.state};
}

struct user_leaf
{};

struct barrier_leaf
{};

struct unmarked_barrier_leaf
{};

struct source_status_barrier_leaf
{};

template <::std::integral char_type, typename output>
inline constexpr void print_define(
	::fast_io::io_reserve_type_t<char_type, barrier_leaf>,
	output &&out, barrier_leaf)
{
	// Model a recursive user formatter: the leaf remains a direct-print control rather than a scatter component.
	::fast_io::operations::print_freestanding<false>(
		::std::forward<output>(out), "<B>");
}

template <::std::integral char_type, typename output>
inline constexpr void print_define(
	::fast_io::io_reserve_type_t<char_type, unmarked_barrier_leaf>,
	output &&out, unmarked_barrier_leaf)
{
	::fast_io::operations::print_freestanding<false>(
		::std::forward<output>(out), "<B>");
}

template <::std::integral char_type, typename output>
inline constexpr void print_define(
	::fast_io::io_reserve_type_t<char_type, source_status_barrier_leaf>,
	output &&out, source_status_barrier_leaf)
{
	::fast_io::operations::print_freestanding<false>(
		::std::forward<output>(out), "<S>");
}

template <::std::integral char_type>
inline constexpr ::std::true_type
print_semantic_optional_scatter_barrier_leaf(
	::fast_io::io_reserve_type_t<char_type, barrier_leaf>) noexcept
{
	// This type owns no status customization and only the direct print CPO above. Its recursive call completes before
	// the caller resumes, so preceding and following descriptor segments retain their ordinary observable boundaries.
	return {};
}

template <::std::integral char_type>
inline constexpr ::std::true_type
print_semantic_optional_scatter_barrier_leaf(
	::fast_io::io_reserve_type_t<char_type,
								 source_status_barrier_leaf>) noexcept
{
	// This deliberately dishonest fixture is paired with an exact source-record status overload below. The consumer's
	// mechanical source-graph guard must still reject the mixed optimization before relying on the provider promise.
	return {};
}

template <bool line, typename... Args>
	requires(
		(false || ... ||
		 ::std::same_as<
			 ::fast_io::details::decay::
				 print_runtime_scatter_plan_unwrapped_t<Args &>,
						 source_status_barrier_leaf>))
inline void status_print_define(scatter_sink, Args &...);

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, user_leaf>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, user_leaf>, char *iter,
	user_leaf) noexcept
{
	*iter = 'U';
	return iter + 1;
}

using optional_static_scatter = decltype(::fast_io::mnp::cond(true, "A1"));
using optional_character = decltype(::fast_io::mnp::cond(true, "D"));
using closed_static_scatter_choice =
	decltype(::fast_io::mnp::cond(true, "LONG", "R"));
using optional_user_leaf =
	::fast_io::manipulators::condition<user_leaf, ::fast_io::io_null_t>;

template <typename T>
using normalized_source = ::std::remove_reference_t<decltype(
	::fast_io::details::decay::print_semantic_input_forward<char>(
		::std::declval<T &>()))>;

using normalized_optional = normalized_source<optional_static_scatter>;
using normalized_optional_character = normalized_source<optional_character>;
using normalized_closed_choice =
	normalized_source<closed_static_scatter_choice>;
using normalized_separator = normalized_source<char const[2]>;
using normalized_scalar = normalized_source<unsigned>;
using normalized_location = normalized_source<::std::source_location>;
using normalized_timestamp =
	normalized_source<::fast_io::basic_timestamp<0>>;
using normalized_iso8601 =
	normalized_source<::fast_io::iso8601_timestamp>;
using normalized_ip = normalized_source<::fast_io::ip>;
using normalized_barrier = normalized_source<barrier_leaf>;
using normalized_unmarked_barrier =
	normalized_source<unmarked_barrier_leaf>;
using normalized_source_status_barrier =
	normalized_source<source_status_barrier_leaf>;
using normalized_width = normalized_source<decltype(
	::fast_io::mnp::left("W", 4u))>;
using explicit_barrier_transport =
	::fast_io::parameter<barrier_leaf const &>;
using explicit_source_status_barrier_transport =
	::fast_io::parameter<source_status_barrier_leaf const &>;

// Reference transition system for the former type-list scan. Keeping it in the test, rather than sharing the new
// partition metadata, checks exact interval selection, empty boundaries, source rejection, and final-line ownership.
template <typename... Args>
using proof_list = ::fast_io::details::decay::print_semantic_active_record_type_list<Args...>;

template <bool Line, typename Output, typename Prefix, typename Remaining>
struct reference_barrier_scan;

template <bool Line, typename Output, typename... Prefix>
struct reference_barrier_scan<Line, Output, proof_list<Prefix...>, proof_list<>>
	: ::fast_io::details::decay::print_semantic_optional_scatter_barrier_segment_proof<
		  Line, char, Output, proof_list<Prefix...>>
{};

template <bool Line, typename Output, typename... Prefix, typename First, typename... Tail>
struct reference_barrier_scan<Line, Output, proof_list<Prefix...>, proof_list<First, Tail...>>
{
	inline static constexpr bool value{[]() consteval {
		if constexpr (::fast_io::details::decay::print_semantic_optional_scatter_boundary_argument_v<
						  Output, char, First>)
		{
			return ::fast_io::details::decay::print_semantic_optional_scatter_barrier_segment_proof<
					   false, char, Output, proof_list<Prefix...>>::value &&
				   reference_barrier_scan<Line, Output, proof_list<>, proof_list<Tail...>>::value;
		}
		else if constexpr (
			::fast_io::details::decay::print_semantic_optional_scatter_argument_v<char, First &> ||
			::fast_io::details::decay::print_semantic_optional_scatter_plain_argument_v<char, First &>)
		{
			return reference_barrier_scan<Line, Output, proof_list<Prefix..., First>, proof_list<Tail...>>::value;
		}
		else
		{
			return false;
		}
	}()};
};

template <typename Output, typename... Args>
consteval bool partition_preserves_output_proof()
{
	return []<bool... Line>(::std::integer_sequence<bool, Line...>) consteval {
		return ((reference_barrier_scan<Line, Output, proof_list<>, proof_list<Args...>>::value ==
				 ::fast_io::details::decay::print_semantic_optional_scatter_barrier_partition<
					 Line, char, Output, Args...>::value) &&
				...);
	}(::std::integer_sequence<bool, false, true>{});
}

template <typename... Args>
consteval bool partition_preserves_proof()
{
	return partition_preserves_output_proof<scatter_sink, Args...>() &&
		   partition_preserves_output_proof<coalescing_sink, Args...>() &&
		   partition_preserves_output_proof<whole_coalescing_sink, Args...>();
}

static_assert(partition_preserves_proof<>());
static_assert(partition_preserves_proof<normalized_barrier>());
static_assert(partition_preserves_proof<normalized_barrier, normalized_barrier>());
static_assert(partition_preserves_proof<normalized_optional, normalized_barrier, normalized_optional>());
static_assert(partition_preserves_proof<normalized_optional, normalized_unmarked_barrier, normalized_optional>());
static_assert(partition_preserves_proof<normalized_optional, explicit_barrier_transport, normalized_optional>());
static_assert(partition_preserves_proof<normalized_optional, normalized_barrier const, normalized_optional>());
static_assert(partition_preserves_proof<normalized_optional, normalized_width, normalized_optional>());
static_assert(partition_preserves_proof<normalized_optional, ::fast_io::io_null_t, normalized_barrier>());
static_assert(partition_preserves_proof<normalized_barrier,
										normalized_optional, normalized_optional, normalized_optional, normalized_optional,
										normalized_separator, normalized_separator, normalized_barrier>());
static_assert(partition_preserves_proof<normalized_barrier,
										normalized_optional, normalized_optional, normalized_optional, normalized_optional,
										normalized_separator, normalized_separator, normalized_separator, normalized_barrier>());
static_assert(partition_preserves_proof<normalized_optional, normalized_optional, normalized_optional, normalized_optional,
										normalized_separator, normalized_scalar, normalized_separator, normalized_width,
										normalized_optional, normalized_barrier, normalized_optional>());

static_assert(
	::fast_io::details::decay::
		print_semantic_optional_scatter_argument_v<
			char, optional_static_scatter &>);
static_assert(
	::fast_io::details::decay::
		print_semantic_optional_scatter_argument_v<
			char, optional_character &>);
static_assert(
	::fast_io::details::decay::
		print_semantic_optional_scatter_argument_v<
			char, closed_static_scatter_choice &>);
static_assert(
	!::fast_io::details::decay::
		print_semantic_optional_scatter_argument_v<
			char, optional_user_leaf &>);
static_assert(
	::fast_io::semantic_optional_scatter_plan_stream<char, scatter_sink>);
static_assert(
	::fast_io::semantic_optional_scatter_barrier_plan_stream<
		char, scatter_sink>);
static_assert(
	::fast_io::semantic_optional_scatter_status_transparent_leaf<
		char, normalized_location>);
static_assert(
	::fast_io::semantic_optional_scatter_status_transparent_leaf<
		char, ::fast_io::basic_timestamp<0>>);
static_assert(
	::fast_io::semantic_optional_scatter_status_transparent_leaf<
		char, ::fast_io::iso8601_timestamp>);
static_assert(
	::fast_io::semantic_optional_scatter_status_transparent_leaf<
		char, normalized_ip>);
static_assert(
	!::fast_io::semantic_optional_scatter_plan_stream<char, locked_sink>);
static_assert(
	::fast_io::operations::decay::defines::
		has_complete_output_stream_mutex_protocol<locked_sink>);
static_assert(
	::fast_io::details::decay::
		print_semantic_optional_scatter_plan_available<
			false, char, scatter_sink,
			normalized_optional, normalized_separator,
			normalized_optional, normalized_separator,
			normalized_optional, normalized_separator,
			normalized_optional_character, normalized_separator,
			normalized_separator, normalized_scalar>());
static_assert(
	::fast_io::details::decay::
		print_semantic_optional_scatter_width_barrier_argument_v<
			coalescing_sink, char, normalized_width>);
static_assert(
	::fast_io::details::decay::
		print_semantic_optional_scatter_barrier_plan_available<
			false, char, coalescing_sink,
			normalized_optional, normalized_separator,
			normalized_optional, normalized_separator,
			normalized_optional, normalized_separator,
			normalized_optional, normalized_separator,
			normalized_width, normalized_separator,
			normalized_width, normalized_scalar>());
static_assert(
	!::fast_io::details::decay::
		print_semantic_optional_scatter_width_barrier_argument_v<
			whole_coalescing_sink, char, normalized_width>);
static_assert(
	::fast_io::details::decay::
		print_semantic_optional_scatter_coalescing_width_argument_v<
			whole_coalescing_sink, char, normalized_width>);
using normalized_width_scan =
	::fast_io::details::decay::
		print_semantic_optional_scatter_width_coalescing_scan<
			false, char, whole_coalescing_sink,
			::fast_io::details::decay::
				print_semantic_active_record_type_list<>,
			::fast_io::details::decay::
				print_semantic_active_record_type_list<
					normalized_optional, normalized_separator,
					normalized_optional, normalized_separator,
					normalized_optional, normalized_separator,
					normalized_optional,
					normalized_width, normalized_separator,
					normalized_width, normalized_separator,
					normalized_scalar, normalized_separator>>;
static_assert(normalized_width_scan::value);
static_assert(normalized_width_scan::has_linear_segment);
static_assert(
	::fast_io::details::decay::print_semantic_precise_size_ok<
		char,
		::fast_io::details::decay::
			print_semantic_optional_coalescing_component<char>>::value);
static_assert(
	::fast_io::operations::decay::
		print_semantic_optional_scatter_width_coalescing_plan_available<
			false, char, whole_coalescing_sink,
			normalized_optional, normalized_separator,
			normalized_optional, normalized_separator,
			normalized_optional, normalized_separator,
			normalized_optional,
			normalized_width, normalized_separator,
			normalized_width, normalized_separator,
			normalized_scalar, normalized_separator>());
static_assert(
	::fast_io::details::decay::
		print_semantic_optional_scatter_barrier_argument_v<
			scatter_sink, char, normalized_barrier>);
static_assert(
	::fast_io::details::decay::
		print_semantic_optional_scatter_barrier_argument_v<
			scatter_sink, char, explicit_barrier_transport>);
static_assert(
	!::fast_io::details::decay::
		print_semantic_optional_scatter_barrier_argument_v<
			scatter_sink, char, normalized_unmarked_barrier>);
static_assert(
	::fast_io::details::decay::
		print_semantic_optional_scatter_barrier_plan_available<
			false, char, scatter_sink,
			normalized_optional, normalized_separator,
			normalized_optional, normalized_separator,
			normalized_optional, normalized_separator,
			normalized_optional_character, normalized_separator,
			normalized_scalar, normalized_barrier,
			normalized_optional_character,
			normalized_separator>());
static_assert(
	::fast_io::details::decay::
		print_semantic_optional_scatter_barrier_plan_available<
			false, char, scatter_sink,
			normalized_optional, normalized_separator,
			normalized_optional, normalized_separator,
			normalized_optional, normalized_barrier,
			normalized_optional, normalized_separator,
			normalized_scalar>());
static_assert(
	!::fast_io::details::decay::
		print_semantic_optional_scatter_barrier_plan_available<
			false, char, scatter_sink,
			normalized_optional, normalized_separator,
			normalized_optional, normalized_separator,
			normalized_optional, normalized_separator,
			normalized_optional_character, normalized_separator,
			normalized_scalar,
			explicit_source_status_barrier_transport,
			normalized_optional_character,
			normalized_separator>());
static_assert(
	!::fast_io::details::decay::
		print_semantic_optional_scatter_barrier_plan_available<
			false, char, scatter_sink,
			normalized_optional, normalized_separator,
			normalized_optional, normalized_separator,
			normalized_optional, normalized_separator,
			normalized_optional_character, normalized_separator,
			normalized_scalar, normalized_source_status_barrier,
			normalized_optional_character,
			normalized_separator>());
static_assert(
	::fast_io::details::decay::
		print_semantic_optional_scatter_plan_available<
			false, char, scatter_sink,
			normalized_optional, normalized_separator,
			normalized_optional, normalized_separator,
			normalized_optional, normalized_separator,
			normalized_optional_character, normalized_location,
			normalized_separator, normalized_scalar>());
static_assert(
	::fast_io::details::decay::
		print_semantic_optional_scatter_plan_available<
			false, char, scatter_sink,
			normalized_optional, normalized_separator,
			normalized_optional, normalized_timestamp,
			normalized_closed_choice, normalized_optional,
			normalized_separator,
			normalized_iso8601, normalized_optional,
			normalized_separator>());
static_assert(
	::fast_io::details::decay::
		print_semantic_optional_scatter_plan_available<
			false, char, coalescing_sink,
			normalized_optional, normalized_separator,
			normalized_optional, normalized_separator,
			normalized_optional, normalized_separator,
			normalized_optional_character, normalized_separator,
			normalized_separator, normalized_scalar>());

template <bool line, typename output>
inline void emit_record(output &&sink, unsigned mask)
{
	auto const first{(mask & 1u) != 0u};
	auto const second{(mask & 2u) != 0u};
	auto const third{(mask & 4u) != 0u};
	auto const fourth{(mask & 8u) != 0u};
	if constexpr (line)
	{
		::fast_io::io::println(
			::std::forward<output>(sink),
			::fast_io::mnp::cond(first, "A1"), "|",
			::fast_io::mnp::cond(second, "B2"), "|",
			::fast_io::mnp::cond(third, "C3"), "|",
			::fast_io::mnp::cond(fourth, "D"), "=", mask);
	}
	else
	{
		::fast_io::io::print(
			::std::forward<output>(sink),
			::fast_io::mnp::cond(first, "A1"), "|",
			::fast_io::mnp::cond(second, "B2"), "|",
			::fast_io::mnp::cond(third, "C3"), "|",
			::fast_io::mnp::cond(fourth, "D"), "=", mask);
	}
}

inline ::std::string expected_record(unsigned mask, bool line)
{
	::std::string expected;
	if ((mask & 1u) != 0u)
	{
		expected += "A1";
	}
	expected.push_back('|');
	if ((mask & 2u) != 0u)
	{
		expected += "B2";
	}
	expected.push_back('|');
	if ((mask & 4u) != 0u)
	{
		expected += "C3";
	}
	expected.push_back('|');
	if ((mask & 8u) != 0u)
	{
		expected.push_back('D');
	}
	expected.push_back('=');
	expected += ::std::to_string(mask);
	if (line)
	{
		expected.push_back('\n');
	}
	return expected;
}

template <bool line, typename output>
inline void emit_width_record(
	output &&sink, unsigned mask,
	::std::size_t first_width = 4u,
	::std::size_t second_width = 3u)
{
	auto const first{(mask & 1u) != 0u};
	auto const second{(mask & 2u) != 0u};
	auto const third{(mask & 4u) != 0u};
	auto const fourth{(mask & 8u) != 0u};
	if constexpr (line)
	{
		::fast_io::io::println(
			::std::forward<output>(sink),
			::fast_io::mnp::cond(first, "A"), "|",
			::fast_io::mnp::cond(second, "B"), "|",
			::fast_io::mnp::cond(third, "C"), "|",
			::fast_io::mnp::cond(fourth, "D"),
			::fast_io::mnp::left("W", first_width), ":",
			::fast_io::mnp::right("X", second_width), "=", mask,
			";");
	}
	else
	{
		::fast_io::io::print(
			::std::forward<output>(sink),
			::fast_io::mnp::cond(first, "A"), "|",
			::fast_io::mnp::cond(second, "B"), "|",
			::fast_io::mnp::cond(third, "C"), "|",
			::fast_io::mnp::cond(fourth, "D"),
			::fast_io::mnp::left("W", first_width), ":",
			::fast_io::mnp::right("X", second_width), "=", mask,
			";");
	}
}

inline ::std::string expected_width_record(
	unsigned mask, bool line, ::std::size_t first_width = 4u,
	::std::size_t second_width = 3u)
{
	::std::string expected;
	if ((mask & 1u) != 0u)
	{
		expected.push_back('A');
	}
	expected.push_back('|');
	if ((mask & 2u) != 0u)
	{
		expected.push_back('B');
	}
	expected.push_back('|');
	if ((mask & 4u) != 0u)
	{
		expected.push_back('C');
	}
	expected.push_back('|');
	if ((mask & 8u) != 0u)
	{
		expected.push_back('D');
	}
	expected.push_back('W');
	if (first_width > 1u)
	{
		expected.append(first_width - 1u, ' ');
	}
	expected.push_back(':');
	if (second_width > 1u)
	{
		expected.append(second_width - 1u, ' ');
	}
	expected.push_back('X');
	expected.push_back('=');
	expected += ::std::to_string(mask);
	expected.push_back(';');
	if (line)
	{
		expected.push_back('\n');
	}
	return expected;
}

template <bool line, typename marked_output, typename fallback_output>
inline void check_width_equivalence(
	marked_output marked_sink, capture_state &marked_state,
	fallback_output fallback_sink, capture_state &fallback_state,
	::std::size_t first_width = 4u,
	::std::size_t second_width = 3u,
	char const *expected_operations = nullptr)
{
	for (unsigned mask{}; mask != 16u; ++mask)
	{
		marked_state.output.clear();
		fallback_state.output.clear();
		marked_state.operations.clear();
		fallback_state.operations.clear();
		emit_width_record<line>(
			marked_sink, mask, first_width, second_width);
		emit_width_record<line>(
			fallback_sink, mask, first_width, second_width);
		auto const expected{expected_width_record(
			mask, line, first_width, second_width)};
		assert(marked_state.output == expected);
		assert(fallback_state.output == expected);
		assert(marked_state.operations == fallback_state.operations);
		if (expected_operations != nullptr)
		{
			assert(marked_state.operations == expected_operations);
		}
	}
}

template <bool line, typename output>
inline void emit_time_record(output &&sink, unsigned mask)
{
	::fast_io::basic_timestamp<0> timestamp{123, 0u};
	::fast_io::iso8601_timestamp iso8601{
		2024, 7u, 25u, 12u, 34u, 56u, 0u, 0};
	auto const first{(mask & 1u) != 0u};
	auto const second{(mask & 2u) != 0u};
	auto const third{(mask & 4u) != 0u};
	auto const fourth{(mask & 8u) != 0u};
	auto selected_word{
		::fast_io::mnp::cond((mask & 16u) != 0u, "LONG", "R")};
	if constexpr (line)
	{
		::fast_io::io::println(
			::std::forward<output>(sink),
			::fast_io::mnp::cond(first, "A"), "|",
			::fast_io::mnp::cond(second, "B"), timestamp,
			selected_word, ::fast_io::mnp::cond(third, "C"),
			":", iso8601,
			::fast_io::mnp::cond(fourth, "D"), "!");
	}
	else
	{
		::fast_io::io::print(
			::std::forward<output>(sink),
			::fast_io::mnp::cond(first, "A"), "|",
			::fast_io::mnp::cond(second, "B"), timestamp,
			selected_word, ::fast_io::mnp::cond(third, "C"),
			":", iso8601,
			::fast_io::mnp::cond(fourth, "D"), "!");
	}
}

inline ::std::string expected_time_record(unsigned mask, bool line)
{
	::std::string expected;
	if ((mask & 1u) != 0u)
	{
		expected.push_back('A');
	}
	expected.push_back('|');
	if ((mask & 2u) != 0u)
	{
		expected.push_back('B');
	}
	expected += "123";
	expected += (mask & 16u) != 0u ? "LONG" : "R";
	if ((mask & 4u) != 0u)
	{
		expected.push_back('C');
	}
	expected += ":2024-07-25T12:34:56Z";
	if ((mask & 8u) != 0u)
	{
		expected.push_back('D');
	}
	expected.push_back('!');
	if (line)
	{
		expected.push_back('\n');
	}
	return expected;
}

template <bool line, typename marked_output, typename fallback_output>
inline void check_time_equivalence(
	marked_output marked_sink, capture_state &marked_state,
	fallback_output fallback_sink, capture_state &fallback_state)
{
	for (unsigned mask{}; mask != 32u; ++mask)
	{
		marked_state.output.clear();
		fallback_state.output.clear();
		marked_state.operations.clear();
		fallback_state.operations.clear();
		emit_time_record<line>(marked_sink, mask);
		emit_time_record<line>(fallback_sink, mask);
		auto const expected{expected_time_record(mask, line)};
		assert(marked_state.output == expected);
		assert(fallback_state.output == expected);
		assert(marked_state.operations == fallback_state.operations);
	}
}

template <bool line, typename output, typename Barrier>
inline void emit_split_barrier_record(
	output &&sink, unsigned mask, Barrier barrier)
{
	auto const first{(mask & 1u) != 0u};
	auto const second{(mask & 2u) != 0u};
	auto const third{(mask & 4u) != 0u};
	auto const fourth{(mask & 8u) != 0u};
	if constexpr (line)
	{
		::fast_io::io::println(
			::std::forward<output>(sink),
			::fast_io::mnp::cond(first, "A"), "|",
			::fast_io::mnp::cond(second, "B"), "|",
			::fast_io::mnp::cond(third, "C"), barrier,
			::fast_io::mnp::cond(fourth, "D"), "=", mask);
	}
	else
	{
		::fast_io::io::print(
			::std::forward<output>(sink),
			::fast_io::mnp::cond(first, "A"), "|",
			::fast_io::mnp::cond(second, "B"), "|",
			::fast_io::mnp::cond(third, "C"), barrier,
			::fast_io::mnp::cond(fourth, "D"), "=", mask);
	}
}

inline ::std::string expected_split_barrier_record(
	unsigned mask, bool line)
{
	::std::string expected;
	if ((mask & 1u) != 0u)
	{
		expected.push_back('A');
	}
	expected.push_back('|');
	if ((mask & 2u) != 0u)
	{
		expected.push_back('B');
	}
	expected.push_back('|');
	if ((mask & 4u) != 0u)
	{
		expected.push_back('C');
	}
	expected += "<B>";
	if ((mask & 8u) != 0u)
	{
		expected.push_back('D');
	}
	expected.push_back('=');
	expected += ::std::to_string(mask);
	if (line)
	{
		expected.push_back('\n');
	}
	return expected;
}

template <bool line, typename marked_output, typename fallback_output>
inline void check_split_barrier_equivalence(
	marked_output marked_sink, capture_state &marked_state,
	fallback_output fallback_sink, capture_state &fallback_state)
{
	for (unsigned mask{}; mask != 16u; ++mask)
	{
		marked_state.output.clear();
		fallback_state.output.clear();
		marked_state.operations.clear();
		fallback_state.operations.clear();
		emit_split_barrier_record<line>(marked_sink, mask, barrier_leaf{});
		emit_split_barrier_record<line>(
			fallback_sink, mask, unmarked_barrier_leaf{});
		auto const expected{expected_split_barrier_record(mask, line)};
		assert(marked_state.output == expected);
		assert(fallback_state.output == expected);
		assert(marked_state.operations == fallback_state.operations);
	}
}

template <bool line, typename output, typename Barrier>
inline void emit_barrier_record(
	output &&sink, unsigned mask, Barrier barrier)
{
	auto const first{(mask & 1u) != 0u};
	auto const second{(mask & 2u) != 0u};
	auto const third{(mask & 4u) != 0u};
	auto const fourth{(mask & 8u) != 0u};
	auto const fifth{(mask & 16u) != 0u};
	if constexpr (line)
	{
		::fast_io::io::println(
			::std::forward<output>(sink),
			::fast_io::mnp::cond(first, "A1"), "|",
			::fast_io::mnp::cond(second, "B2"), "|",
			::fast_io::mnp::cond(third, "C3"), "|",
			::fast_io::mnp::cond(fourth, "D"), "=", mask,
			barrier, ::fast_io::mnp::cond(fifth, "E"), "!");
	}
	else
	{
		::fast_io::io::print(
			::std::forward<output>(sink),
			::fast_io::mnp::cond(first, "A1"), "|",
			::fast_io::mnp::cond(second, "B2"), "|",
			::fast_io::mnp::cond(third, "C3"), "|",
			::fast_io::mnp::cond(fourth, "D"), "=", mask,
			barrier, ::fast_io::mnp::cond(fifth, "E"), "!");
	}
}

inline ::std::string expected_barrier_record(
	unsigned mask, bool line)
{
	auto expected{expected_record(mask & 15u, false)};
	// The prefix prints the complete mask value, not only its four condition bits.
	auto const prefix_digits{::std::to_string(mask & 15u)};
	auto const complete_digits{::std::to_string(mask)};
	expected.replace(expected.size() - prefix_digits.size(),
				 prefix_digits.size(), complete_digits);
	expected += "<B>";
	if ((mask & 16u) != 0u)
	{
		expected.push_back('E');
	}
	expected.push_back('!');
	if (line)
	{
		expected.push_back('\n');
	}
	return expected;
}

template <bool line, typename marked_output, typename fallback_output,
		  typename MarkedBarrier = barrier_leaf,
		  typename FallbackBarrier = unmarked_barrier_leaf>
inline void check_barrier_equivalence(
	marked_output marked_sink, capture_state &marked_state,
	fallback_output fallback_sink, capture_state &fallback_state,
	MarkedBarrier marked_barrier = {},
	FallbackBarrier fallback_barrier = {})
{
	for (unsigned mask{}; mask != 32u; ++mask)
	{
		marked_state.output.clear();
		fallback_state.output.clear();
		marked_state.operations.clear();
		fallback_state.operations.clear();
		auto const marked_scatter_calls{marked_state.scatter_calls};
		auto const marked_scalar_calls{marked_state.scalar_calls};
		auto const fallback_scatter_calls{fallback_state.scatter_calls};
		auto const fallback_scalar_calls{fallback_state.scalar_calls};
		emit_barrier_record<line>(marked_sink, mask, marked_barrier);
		emit_barrier_record<line>(fallback_sink, mask,
								   fallback_barrier);
		auto const expected{expected_barrier_record(mask, line)};
		assert(marked_state.output == expected);
		assert(fallback_state.output == expected);
		assert(marked_state.operations == fallback_state.operations);
		assert(marked_state.scatter_calls - marked_scatter_calls ==
			   fallback_state.scatter_calls - fallback_scatter_calls);
		assert(marked_state.scalar_calls - marked_scalar_calls ==
			   fallback_state.scalar_calls - fallback_scalar_calls);
	}
}

template <bool line, typename marked_output, typename fallback_output>
inline void check_matrix_equivalence(
	marked_output marked_sink, capture_state &marked_state,
	fallback_output fallback_sink, capture_state &fallback_state)
{
	for (unsigned mask{}; mask != 16u; ++mask)
	{
		marked_state.output.clear();
		fallback_state.output.clear();
		marked_state.operations.clear();
		fallback_state.operations.clear();
		emit_record<line>(marked_sink, mask);
		emit_record<line>(fallback_sink, mask);
		auto const expected{expected_record(mask, line)};
		assert(marked_state.output == expected);
		assert(fallback_state.output == expected);
		assert(marked_state.operations == fallback_state.operations);
	}
}

template <bool line, typename output>
inline void check_matrix_output(output sink, capture_state &state)
{
	for (unsigned mask{}; mask != 16u; ++mask)
	{
		state.output.clear();
		emit_record<line>(sink, mask);
		assert(state.output == expected_record(mask, line));
	}
}

} // namespace

int main()
{
	capture_state direct_state;
	capture_state direct_reference_state;
	check_matrix_equivalence<false>(
		scatter_sink{__builtin_addressof(direct_state)}, direct_state,
		fallback_scatter_sink{
			__builtin_addressof(direct_reference_state)},
		direct_reference_state);
	check_matrix_equivalence<true>(
		scatter_sink{__builtin_addressof(direct_state)}, direct_state,
		fallback_scatter_sink{
			__builtin_addressof(direct_reference_state)},
		direct_reference_state);

	capture_state coalescing_state;
	capture_state coalescing_reference_state;
	check_matrix_equivalence<false>(
		coalescing_sink{__builtin_addressof(coalescing_state)},
		coalescing_state,
		fallback_coalescing_sink{
			__builtin_addressof(coalescing_reference_state)},
		coalescing_reference_state);
	check_matrix_equivalence<true>(
		coalescing_sink{__builtin_addressof(coalescing_state)},
		coalescing_state,
		fallback_coalescing_sink{
			__builtin_addressof(coalescing_reference_state)},
		coalescing_reference_state);
	check_time_equivalence<false>(
		coalescing_sink{__builtin_addressof(coalescing_state)},
		coalescing_state,
		fallback_coalescing_sink{
			__builtin_addressof(coalescing_reference_state)},
		coalescing_reference_state);
	check_time_equivalence<true>(
		coalescing_sink{__builtin_addressof(coalescing_state)},
		coalescing_state,
		fallback_coalescing_sink{
			__builtin_addressof(coalescing_reference_state)},
		coalescing_reference_state);
	check_width_equivalence<false>(
		coalescing_sink{__builtin_addressof(coalescing_state)},
		coalescing_state,
		fallback_coalescing_sink{
			__builtin_addressof(coalescing_reference_state)},
		coalescing_reference_state);
	check_width_equivalence<true>(
		coalescing_sink{__builtin_addressof(coalescing_state)},
		coalescing_state,
		fallback_coalescing_sink{
			__builtin_addressof(coalescing_reference_state)},
		coalescing_reference_state);

	capture_state whole_coalescing_state;
	capture_state whole_coalescing_reference_state;
	check_width_equivalence<false>(
		whole_coalescing_sink{
			__builtin_addressof(whole_coalescing_state)},
		whole_coalescing_state,
		fallback_whole_coalescing_sink{
			__builtin_addressof(whole_coalescing_reference_state)},
		whole_coalescing_reference_state, 4u, 3u, "w");
	check_width_equivalence<true>(
		whole_coalescing_sink{
			__builtin_addressof(whole_coalescing_state)},
		whole_coalescing_state,
		fallback_whole_coalescing_sink{
			__builtin_addressof(whole_coalescing_reference_state)},
		whole_coalescing_reference_state, 4u, 3u, "w");
	// Both whole-output limits reject this record. The optimized and generic paths must then expose exactly the same
	// width-delimited native-scatter/write sequence, including final newline ownership.
	check_width_equivalence<false>(
		whole_coalescing_sink{
			__builtin_addressof(whole_coalescing_state)},
		whole_coalescing_state,
		fallback_whole_coalescing_sink{
			__builtin_addressof(whole_coalescing_reference_state)},
		whole_coalescing_reference_state, 128u, 3u);
	check_width_equivalence<true>(
		whole_coalescing_sink{
			__builtin_addressof(whole_coalescing_state)},
		whole_coalescing_state,
		fallback_whole_coalescing_sink{
			__builtin_addressof(whole_coalescing_reference_state)},
		whole_coalescing_reference_state, 128u, 3u);

	capture_state locked_state;
	locked_state.lock_required = true;
	locked_sink locked{__builtin_addressof(locked_state)};
	check_matrix_output<false>(locked, locked_state);
	check_matrix_output<true>(locked, locked_state);
	assert(locked_state.lock_calls == 32u);
	assert(locked_state.unlock_calls == 32u);
	assert(!locked_state.locked);

	capture_state direct_barrier_state;
	capture_state direct_fallback_state;
	check_barrier_equivalence<false>(
		scatter_sink{__builtin_addressof(direct_barrier_state)},
		direct_barrier_state,
		scatter_sink{__builtin_addressof(direct_fallback_state)},
		direct_fallback_state);
	check_barrier_equivalence<true>(
		scatter_sink{__builtin_addressof(direct_barrier_state)},
		direct_barrier_state,
		scatter_sink{__builtin_addressof(direct_fallback_state)},
		direct_fallback_state);
	check_split_barrier_equivalence<false>(
		scatter_sink{__builtin_addressof(direct_barrier_state)},
		direct_barrier_state,
		scatter_sink{__builtin_addressof(direct_fallback_state)},
		direct_fallback_state);
	check_split_barrier_equivalence<true>(
		scatter_sink{__builtin_addressof(direct_barrier_state)},
		direct_barrier_state,
		scatter_sink{__builtin_addressof(direct_fallback_state)},
		direct_fallback_state);

	barrier_leaf wrapped_barrier;
	unmarked_barrier_leaf wrapped_unmarked_barrier;
	capture_state wrapped_barrier_state;
	capture_state wrapped_fallback_state;
	check_barrier_equivalence<false>(
		scatter_sink{__builtin_addressof(wrapped_barrier_state)},
		wrapped_barrier_state,
		scatter_sink{__builtin_addressof(wrapped_fallback_state)},
		wrapped_fallback_state,
		explicit_barrier_transport{wrapped_barrier},
		::fast_io::parameter<unmarked_barrier_leaf const &>{
			wrapped_unmarked_barrier});
	check_barrier_equivalence<true>(
		scatter_sink{__builtin_addressof(wrapped_barrier_state)},
		wrapped_barrier_state,
		scatter_sink{__builtin_addressof(wrapped_fallback_state)},
		wrapped_fallback_state,
		explicit_barrier_transport{wrapped_barrier},
		::fast_io::parameter<unmarked_barrier_leaf const &>{
			wrapped_unmarked_barrier});

	capture_state coalescing_barrier_state;
	capture_state coalescing_fallback_state;
	check_barrier_equivalence<false>(
		coalescing_sink{__builtin_addressof(coalescing_barrier_state)},
		coalescing_barrier_state,
		coalescing_sink{__builtin_addressof(coalescing_fallback_state)},
		coalescing_fallback_state);
	check_barrier_equivalence<true>(
		coalescing_sink{__builtin_addressof(coalescing_barrier_state)},
		coalescing_barrier_state,
		coalescing_sink{__builtin_addressof(coalescing_fallback_state)},
		coalescing_fallback_state);

	capture_state locked_barrier_state;
	capture_state locked_fallback_state;
	locked_barrier_state.lock_required = true;
	locked_fallback_state.lock_required = true;
	check_barrier_equivalence<false>(
		locked_sink{__builtin_addressof(locked_barrier_state)},
		locked_barrier_state,
		locked_sink{__builtin_addressof(locked_fallback_state)},
		locked_fallback_state);
	check_barrier_equivalence<true>(
		locked_sink{__builtin_addressof(locked_barrier_state)},
		locked_barrier_state,
		locked_sink{__builtin_addressof(locked_fallback_state)},
		locked_fallback_state);
	assert(locked_barrier_state.lock_calls == 64u);
	assert(locked_barrier_state.unlock_calls == 64u);
	assert(locked_fallback_state.lock_calls == 64u);
	assert(locked_fallback_state.unlock_calls == 64u);
	assert(!locked_barrier_state.locked);
	assert(!locked_fallback_state.locked);
}
