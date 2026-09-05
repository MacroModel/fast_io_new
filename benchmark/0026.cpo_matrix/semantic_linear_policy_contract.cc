// Build this unchanged against each include tree and compare complete stdout.
// No source declares optional-plan/barrier promises. The transcript captures the
// ordinary policy selected after semantic conditions have chosen their leaves.
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string>
#include <utility>

#include <fast_io.h>

// Select one four-condition family for a cheaper compile-resource probe.
// The default executes the complete differential contract. Family 8 checks
// whole-record status ownership and intentionally performs no primitive output.
// Families 9--11 keep every optional leaf closed and exercise mandatory dynamic,
// context and direct producers, including exceptions with observable prefixes.
#ifndef FAST_IO_LINEAR_POLICY_FAMILY
#define FAST_IO_LINEAR_POLICY_FAMILY -1
#endif
#if FAST_IO_LINEAR_POLICY_FAMILY < -1 || FAST_IO_LINEAR_POLICY_FAMILY > 11
#error "FAST_IO_LINEAR_POLICY_FAMILY must be -1 (all) or 0 through 11"
#endif

namespace semantic_linear_policy_contract
{

inline constexpr char borrowed_payload[]{"borrowed-payload"};

struct failure_injection
{
	char kind{};
	::std::size_t id{};
	::std::size_t occurrence{};
};

struct injected_failure
{
	failure_injection point;
};

struct capture
{
	::std::string bytes;
	::std::string events;
	::std::size_t status_calls{};
	::std::size_t primitive_calls{};
	failure_injection failure;
	::std::size_t matching_failure_events{};
};

inline void number(capture &state, ::std::size_t value)
{
	state.events += ::std::to_string(value);
	state.events.push_back(',');
}

inline void event(capture &state, char kind, ::std::size_t id, ::std::size_t size)
{
	state.events.push_back(kind);
	number(state, id);
	number(state, size);
	if (kind == state.failure.kind && id == state.failure.id &&
		++state.matching_failure_events == state.failure.occurrence)
	{
		throw injected_failure{state.failure};
	}
}

template <bool Status = false>
struct output
{
	using output_char_type = char;
	capture *state;
};

template <bool Status>
inline constexpr output<Status> output_stream_ref_define(output<Status> out) noexcept
{
	return out;
}

template <bool Status>
inline constexpr ::std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char, output<Status>>) noexcept
{
	return {};
}

template <bool Status>
inline constexpr ::std::true_type print_semantic_optional_scatter_plan_stream(
	::fast_io::io_reserve_type_t<char, output<Status>>) noexcept
{
	return {};
}

template <bool Status>
inline constexpr ::std::true_type print_semantic_optional_scatter_barrier_plan_stream(
	::fast_io::io_reserve_type_t<char, output<Status>>) noexcept
{
	return {};
}

inline void extent(capture &state, char const *first, ::std::size_t size)
{
	assert(size == 0u || first != nullptr);
	number(state, size);
	// Compare a stable provenance label, never process-dependent pointer values.
	state.events.push_back(first == borrowed_payload ? 'B' : 'T');
	if (size != 0u)
	{
		state.bytes.append(first, size);
	}
}

template <bool Status>
inline void write_all_overflow_define(output<Status> out, char const *first, char const *last)
{
	++out.state->primitive_calls;
	out.state->events.push_back('W');
	extent(*out.state, first, static_cast<::std::size_t>(last - first));
	out.state->events.push_back(';');
}

template <bool Status>
inline void scatter_write_all_overflow_define(output<Status> out,
											  ::fast_io::basic_io_scatter_t<char> const *scatters, ::std::size_t count)
{
	++out.state->primitive_calls;
	out.state->events.push_back('S');
	number(*out.state, count);
	for (::std::size_t index{}; index != count; ++index)
	{
		extent(*out.state, scatters[index].base, scatters[index].len);
	}
	out.state->events.push_back(';');
}

template <bool Line, typename... Args>
inline void status_print_define(output<true> out, Args &&...)
{
	++out.state->status_calls;
	event(*out.state, 'O', Line, sizeof...(Args));
}

template <::std::size_t Size>
struct reserve
{
	capture *state;
	unsigned id;
	char value;
};

template <::std::size_t Size>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, reserve<Size>>) noexcept
{
	return Size;
}

template <::std::size_t Size>
inline char *print_reserve_define(::fast_io::io_reserve_type_t<char, reserve<Size>>,
								  char *first, reserve<Size> const &value)
{
	event(*value.state, 'R', value.id, Size);
	for (::std::size_t index{}; index != Size; ++index)
	{
		*first++ = value.value;
	}
	return first;
}

template <bool Bounded>
struct dynamic
{
	capture *state;
	unsigned id;
	::std::size_t size;
};

template <bool Bounded>
inline ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, dynamic<Bounded>>, dynamic<Bounded> const &value)
{
	event(*value.state, 'Q', value.id, value.size);
	return value.size;
}

template <bool Bounded>
inline char *print_reserve_define(::fast_io::io_reserve_type_t<char, dynamic<Bounded>>,
								  char *first, dynamic<Bounded> const &value)
{
	event(*value.state, 'D', value.id, value.size);
	for (::std::size_t index{}; index != value.size; ++index)
	{
		*first++ = 'd';
	}
	return first;
}

inline constexpr ::std::size_t print_reserve_static_stack_size(
	::fast_io::io_reserve_type_t<char, dynamic<true>>) noexcept
{
	return 24u;
}

struct context
{
	capture *state;
	unsigned id;
	::std::size_t size;
};

struct context_state
{
	::std::size_t position{};

	inline ::fast_io::context_print_result<char *> print_context_define(
		context const &value, char *first, char *last)
	{
		auto const capacity{static_cast<::std::size_t>(last - first)};
		event(*value.state, 'C', value.id, capacity);
		auto const remaining{value.size - position};
		auto const count{remaining < capacity ? remaining : capacity};
		for (::std::size_t index{}; index != count; ++index)
		{
			*first++ = static_cast<char>('a' + (position++ % 26u));
		}
		number(*value.state, count);
		number(*value.state, position == value.size);
		return {first, position == value.size};
	}
};

inline constexpr ::fast_io::io_type_t<context_state> print_context_type(
	::fast_io::io_reserve_type_t<char, context>) noexcept
{
	return {};
}

inline constexpr ::std::size_t print_context_static_buffer_size(
	::fast_io::io_reserve_type_t<char, context>) noexcept
{
	return 16u;
}

struct direct
{
	capture *state;
};

template <typename Output>
inline void print_define(::fast_io::io_reserve_type_t<char, direct>, Output &&out, direct value)
{
	event(*value.state, 'P', 0u, 0u);
	::fast_io::operations::print_freestanding<false>(::std::forward<Output>(out), "<direct>");
}

struct borrowed
{
	capture *state;
};

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, borrowed>, borrowed value)
{
	event(*value.state, 'N', 0u, sizeof(borrowed_payload) - 1u);
	return {borrowed_payload, sizeof(borrowed_payload) - 1u};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, borrowed>) noexcept
{
	return {};
}

template <unsigned Family, bool Line, bool Status = false>
void emit(output<Status> out, unsigned mask, ::std::size_t length)
{
	auto *state{out.state};
	auto const first{(mask & 1u) != 0u};
	auto const second{(mask & 2u) != 0u};
	auto const third{(mask & 4u) != 0u};
	auto const fourth{(mask & 8u) != 0u};
	reserve<2u> small{state, 1u, 'r'};
	reserve<17u> large{state, 2u, 's'};
	context ctx{state, 3u, length};
	dynamic<true> bounded{state, 4u, length};
	dynamic<false> unbounded{state, 5u, length};
	dynamic<true> bounded_tail{state, 6u, length};
	context ctx_tail{state, 7u, length};
	direct dir{state};
	borrowed native{state};
	if constexpr (Family == 0u)
	{
		// One-character conditions are reserve/chvw leaves; longer alternatives
		// are static scatters. A false condition disappears between reserve runs.
		::fast_io::operations::print_freestanding<Line>(out,
														::fast_io::mnp::cond(first, "A"), small,
														::fast_io::mnp::cond(second, "BB"), small,
														::fast_io::mnp::cond(third, "CCCC"), small,
														::fast_io::mnp::cond(fourth, "DDDDDDDDD"), small, dir);
	}
	else if constexpr (Family == 1u)
	{
		// Context capture must span null and reserve siblings. The optional
		// 17-byte reserve can change the maximum contiguous reserve burst.
		::fast_io::operations::print_freestanding<Line>(out,
														::fast_io::mnp::cond(first, "A"), small,
														::fast_io::mnp::cond(second, large), small, ctx,
														::fast_io::mnp::cond(third, "C"), small,
														::fast_io::mnp::cond(fourth, large), large);
	}
	else if constexpr (Family == 2u)
	{
		// Static literal scatters still expose reserve formatting to the context
		// scanner. Their selected extents affect capture bursts and windows even
		// when the output-aware scatter scanner would retain their pointers.
		::fast_io::operations::print_freestanding<Line>(out,
														::fast_io::mnp::cond(first, "A"), small,
														::fast_io::mnp::cond(second, "BB"), ctx,
														::fast_io::mnp::cond(third, "CCCC"), small,
														::fast_io::mnp::cond(fourth, "DDDDDDDDD"), small);
	}
	else if constexpr (Family == 3u)
	{
		// The dynamic producer advertises only a stack hint, so values above
		// that hint remain valid and must fall back with the original query order.
		::fast_io::operations::print_freestanding<Line>(out,
														::fast_io::mnp::cond(first, "A"), small,
														::fast_io::mnp::cond(second, bounded), ctx,
														::fast_io::mnp::cond(third, "C"), bounded,
														::fast_io::mnp::cond(fourth, "D"), small);
	}
	else if constexpr (Family == 4u)
	{
		::fast_io::operations::print_freestanding<Line>(out,
														::fast_io::mnp::cond(first, "A"), small,
														::fast_io::mnp::cond(second, "BB"), unbounded, ctx,
														::fast_io::mnp::cond(third, "CCCC"), small,
														::fast_io::mnp::cond(fourth, "DDDDDDDDD"), ctx);
	}
	else if constexpr (Family == 5u)
	{
		::fast_io::operations::print_freestanding<Line>(out,
														::fast_io::mnp::cond(first, "A"), small, ctx,
														::fast_io::mnp::cond(second, native), small,
														::fast_io::mnp::cond(third, dir), small,
														::fast_io::mnp::cond(fourth, "D"), ctx);
	}
	else if constexpr (Family == 6u)
	{
		// Zero or one active leaf must retain empty-record/single-control policy.
		::fast_io::operations::print_freestanding<Line>(out,
														::fast_io::mnp::cond(first, "A"),
														::fast_io::mnp::cond(second, ctx),
														::fast_io::mnp::cond(third, "C"),
														::fast_io::mnp::cond(fourth, "D"));
	}
	else if constexpr (Family == 7u)
	{
		// Closed choices also change protocol and arity after normalization.
		::fast_io::operations::print_freestanding<Line>(out,
														::fast_io::mnp::cond(first, "A", "BB"), small,
														::fast_io::mnp::cond(second, "CCCC", "DDDDDDDDD"), ctx,
														::fast_io::mnp::cond(third, "C", ::fast_io::mnp::pack()), small,
														::fast_io::mnp::cond(fourth, "D", "EE"));
	}
	else if constexpr (Family == 9u)
	{
		// All dynamic providers are mandatory. The first run must query both
		// sizes before materialization, whether its selected leaves make it
		// reserve-only or a mixture of reserves and native static scatters.
		::fast_io::operations::print_freestanding<Line>(out,
														::fast_io::mnp::cond(first, "A"), small, bounded,
														::fast_io::mnp::cond(second, "BB"), large,
														::fast_io::mnp::cond(third, "CCCC"), bounded_tail,
														::fast_io::mnp::cond(fourth, "DDDDDDDDD"), small, dir);
	}
	else if constexpr (Family == 10u)
	{
		// Mandatory context changes this entire run to capture. Selected static
		// leaves change its maximum reserve burst (34 through 43 characters),
		// while each dynamic producer resets that burst and is queried in place.
		::fast_io::operations::print_freestanding<Line>(out,
														::fast_io::mnp::cond(first, "A"), large, large, bounded,
														::fast_io::mnp::cond(second, "BB"), ctx, small,
														::fast_io::mnp::cond(third, "CCCC"), bounded_tail, large,
														::fast_io::mnp::cond(fourth, "DDDDDDDDD"), large);
	}
	else if constexpr (Family == 11u)
	{
		// Direct output ends the initial capture run. The borrowed scatter then
		// ends the next capture opportunity, so bounded dynamic materialization
		// precedes a separate final context run. Later exceptions retain earlier
		// completed groups and the direct producer's output.
		::fast_io::operations::print_freestanding<Line>(out,
														::fast_io::mnp::cond(first, "A"), small, ctx,
														::fast_io::mnp::cond(second, "BB"), large, dir,
														::fast_io::mnp::cond(third, "CCCC"), native, bounded,
														::fast_io::mnp::cond(fourth, "DDDDDDDDD"), small, ctx_tail, large);
	}
}

inline void dump(unsigned family, bool line, unsigned mask, ::std::size_t length, capture const &state)
{
	::std::printf("family=%u line=%u mask=%u length=%zu bytes=%zu\n",
				  family, static_cast<unsigned>(line), mask, length, state.bytes.size());
	for (unsigned char value : state.bytes)
	{
		::std::printf("%02x", static_cast<unsigned>(value));
	}
	::std::putchar('\n');
	::std::fwrite(state.events.data(), 1u, state.events.size(), stdout);
	::std::putchar('\n');
}

struct failure_case
{
	failure_injection point;
	::std::size_t length;
	bool requires_visible_prefix;
};

template <unsigned Family>
void check_exceptions()
{
	constexpr auto cases{[] {
		if constexpr (Family == 9u)
		{
			return ::std::array{
				failure_case{{'Q', 4u, 1u}, 0u, false},
				failure_case{{'Q', 6u, 1u}, 33u, false},
				failure_case{{'D', 4u, 1u}, 24u, false},
				failure_case{{'R', 2u, 1u}, 25u, false},
				failure_case{{'P', 0u, 1u}, 97u, true},
				// Overflow must still query the second dynamic provider before
				// falling back over the complete suffix. The repeated first query
				// throws before any impossible allocation or buffer write.
				failure_case{{'Q', 4u, 2u}, (::std::numeric_limits<::std::size_t>::max)(), true}};
		}
		else if constexpr (Family == 10u)
		{
			return ::std::array{
				failure_case{{'R', 2u, 1u}, 0u, false},
				failure_case{{'Q', 4u, 1u}, 24u, false},
				failure_case{{'Q', 4u, 2u}, 97u, true},
				failure_case{{'D', 4u, 1u}, 97u, true},
				failure_case{{'C', 3u, 1u}, 17u, false},
				failure_case{{'C', 3u, 2u}, 97u, true},
				failure_case{{'Q', 6u, 1u}, 33u, true},
				failure_case{{'D', 6u, 1u}, 24u, false}};
		}
		else
		{
			static_assert(Family == 11u);
			return ::std::array{
				failure_case{{'P', 0u, 1u}, 17u, true},
				failure_case{{'N', 0u, 1u}, 24u, true},
				failure_case{{'Q', 4u, 1u}, 25u, true},
				failure_case{{'D', 4u, 1u}, 32u, true},
				failure_case{{'C', 7u, 1u}, 33u, true},
				failure_case{{'C', 7u, 2u}, 97u, true},
				failure_case{{'R', 2u, 2u}, 97u, true}};
		}
	}()};
	for (auto const &test : cases)
	{
		for (unsigned mask{}; mask != 16u; ++mask)
		{
			for (bool line : {false, true})
			{
				capture state;
				state.failure = test.point;
				bool caught{};
				try
				{
					if (line)
					{
						emit<Family, true>(output<>{&state}, mask, test.length);
					}
					else
					{
						emit<Family, false>(output<>{&state}, mask, test.length);
					}
				}
				catch (injected_failure const &failure)
				{
					caught = true;
					assert(failure.point.kind == test.point.kind && failure.point.id == test.point.id &&
						   failure.point.occurrence == test.point.occurrence);
					state.events.push_back('X');
					state.events.push_back(failure.point.kind);
					number(state, failure.point.id);
					number(state, failure.point.occurrence);
				}
				assert(caught);
				assert(state.status_calls == 0u);
				assert(state.matching_failure_events == test.point.occurrence);
				assert(!test.requires_visible_prefix || !state.bytes.empty());
				::std::printf("throw=%c id=%zu occurrence=%zu\n",
							  test.point.kind, test.point.id, test.point.occurrence);
				dump(Family, line, mask, test.length, state);
			}
		}
	}
}

template <unsigned Family>
void check()
{
	constexpr auto lengths{[] {
		if constexpr (Family >= 9u)
		{
			return ::std::array<::std::size_t, 17u>{
				0u, 1u, 15u, 16u, 17u, 23u, 24u, 25u, 31u, 32u, 33u, 34u, 35u, 42u, 43u, 44u, 97u};
		}
		else
		{
			return ::std::array<::std::size_t, 9u>{0u, 1u, 15u, 16u, 17u, 31u, 32u, 33u, 97u};
		}
	}()};
	for (::std::size_t length : lengths)
	{
		for (unsigned mask{}; mask != 16u; ++mask)
		{
			for (bool line : {false, true})
			{
				capture state;
				if (line)
				{
					emit<Family, true>(output<>{&state}, mask, length);
				}
				else
				{
					emit<Family, false>(output<>{&state}, mask, length);
				}
				assert(state.status_calls == 0u);
				dump(Family, line, mask, length, state);
			}
		}
	}
	if constexpr (Family >= 9u)
	{
		check_exceptions<Family>();
	}
}

void check_whole_record_status()
{
	for (unsigned mask{}; mask != 16u; ++mask)
	{
		capture state;
		emit<2u, false>(output<true>{&state}, mask, 97u);
		emit<2u, true>(output<true>{&state}, mask, 97u);
		// A whole-record customization owns the semantic source before provider
		// value queries and before lower-level context or scatter primitives.
		assert(state.status_calls == 2u);
		assert(state.primitive_calls == 0u);
		assert(state.bytes.empty());
		assert(state.events == "O0,8,O1,8,");
		dump(8u, false, mask, 97u, state);
	}
}

} // namespace semantic_linear_policy_contract

int main()
{
	using namespace semantic_linear_policy_contract;
#if FAST_IO_LINEAR_POLICY_FAMILY == -1
	check<0u>();
	check<1u>();
	check<2u>();
	check<3u>();
	check<4u>();
	check<5u>();
	check<6u>();
	check<7u>();
	check_whole_record_status();
	check<9u>();
	check<10u>();
	check<11u>();
#elif FAST_IO_LINEAR_POLICY_FAMILY == 8
	check_whole_record_status();
#else
	check<FAST_IO_LINEAR_POLICY_FAMILY>();
#endif
}
