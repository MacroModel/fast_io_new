#include <cassert>
#include <cstddef>
#include <string>
#include <type_traits>

#include <fast_io.h>

namespace semantic_status_free_record_test
{

struct exact_output
{
	using output_char_type = char;
};
struct other_output : exact_output
{};

inline constexpr ::std::true_type print_semantic_status_free_record(
	::fast_io::io_reserve_type_t<char, exact_output>, ::std::false_type,
	::fast_io::io_type_t<int const &>) noexcept
{
	return {};
}

struct false_output
{
	using output_char_type = char;
};
inline constexpr ::std::false_type print_semantic_status_free_record(
	::fast_io::io_reserve_type_t<char, false_output>, ::std::false_type,
	::fast_io::io_type_t<int &>) noexcept
{
	return {};
}

struct malformed_output
{
	using output_char_type = char;
};
inline constexpr bool print_semantic_status_free_record(
	::fast_io::io_reserve_type_t<char, malformed_output>, ::std::false_type,
	::fast_io::io_type_t<int &>) noexcept
{
	return true;
}

static_assert(::fast_io::semantic_status_free_record<false, char, exact_output, int const>);
static_assert(::fast_io::semantic_status_free_record<false, char, exact_output, int const &>);
static_assert(!::fast_io::semantic_status_free_record<false, char, exact_output, int>);
static_assert(!::fast_io::semantic_status_free_record<true, char, exact_output, int const>);
static_assert(!::fast_io::semantic_status_free_record<false, char8_t, exact_output, int const>);
static_assert(!::fast_io::semantic_status_free_record<false, char, exact_output const, int const>);
static_assert(!::fast_io::semantic_status_free_record<false, char, other_output, int const>);
static_assert(!::fast_io::semantic_status_free_record<false, char, false_output, int>);
static_assert(!::fast_io::semantic_status_free_record<false, char, malformed_output, int>);
static_assert(!::fast_io::semantic_status_free_record<false, char, exact_output>);

struct capture
{
	::std::string bytes;
	::std::string events;
	::std::size_t status_calls{};
};

enum class flavor
{
	proved,
	reference,
	source_owner,
	active_owner
};

template <flavor Kind>
struct output
{
	using output_char_type = char;
	capture *state;
};

template <flavor Kind>
inline constexpr output<Kind> output_stream_ref_define(output<Kind> out) noexcept
{
	return out;
}

template <flavor Kind>
inline constexpr ::std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char, output<Kind>>) noexcept
{
	return {};
}

// The reference destination deliberately retains the original selector. All destinations have the same primitive
// operations and ordinary policies. The two status owners below require a user leaf, so neither competes for the
// closed library-only records covered by this separate optional-scatter destination promise.
template <flavor Kind>
	requires(Kind != flavor::reference)
inline constexpr ::std::true_type print_semantic_optional_scatter_plan_stream(
	::fast_io::io_reserve_type_t<char, output<Kind>>) noexcept
{
	return {};
}

inline void append_extent(capture &state, char const *first, ::std::size_t size)
{
	state.events += ::std::to_string(size);
	state.events.push_back(',');
	if (size != 0u)
	{
		assert(first != nullptr);
		state.bytes.append(first, size);
	}
}

template <flavor Kind>
inline void write_all_overflow_define(output<Kind> out, char const *first, char const *last)
{
	out.state->events.push_back('W');
	append_extent(*out.state, first, static_cast<::std::size_t>(last - first));
	out.state->events.push_back(';');
}

template <flavor Kind>
inline void scatter_write_all_overflow_define(output<Kind> out,
											  ::fast_io::basic_io_scatter_t<char> const *scatters, ::std::size_t count)
{
	out.state->events.push_back('S');
	out.state->events += ::std::to_string(count);
	out.state->events.push_back(':');
	for (::std::size_t index{}; index != count; ++index)
	{
		append_extent(*out.state, scatters[index].base, scatters[index].len);
	}
	out.state->events.push_back(';');
}

struct reserve_leaf
{
	capture *state;
	char value;
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, reserve_leaf>) noexcept
{
	return 1u;
}

inline char *print_reserve_define(::fast_io::io_reserve_type_t<char, reserve_leaf>,
								  char *first, reserve_leaf const &leaf)
{
	leaf.state->events.push_back('R');
	leaf.state->events.push_back(leaf.value);
	*first = leaf.value;
	return first + 1u;
}

using choice = decltype(::fast_io::mnp::cond(false, "aa"));
using separator = ::fast_io::manipulators::chvw_t<char>;

// This proof covers one complete source graph only. No active record for the proved destination owns status. For
// source_owner, its only status overload below requires the original condition nodes and cannot match an active pack.
template <flavor Kind, bool Line>
	requires(Kind == flavor::proved || Kind == flavor::source_owner)
inline constexpr ::std::true_type print_semantic_status_free_record(
	::fast_io::io_reserve_type_t<char, output<Kind>>, ::std::bool_constant<Line>,
	::fast_io::io_type_t<choice &>, ::fast_io::io_type_t<reserve_leaf &>,
	::fast_io::io_type_t<choice &>, ::fast_io::io_type_t<separator &>,
	::fast_io::io_type_t<choice &>, ::fast_io::io_type_t<reserve_leaf &>,
	::fast_io::io_type_t<choice &>) noexcept
{
	return {};
}

template <bool Line>
inline void status_print_define(output<flavor::source_owner> out,
								choice &, reserve_leaf &, choice &, separator &, choice &, reserve_leaf &, choice &)
{
	++out.state->status_calls;
	out.state->events.push_back('O');
	out.state->bytes += Line ? "source\n" : "source";
}

// Only mask zero has this active shape. This destination supplies no whole-source status-free promise.
template <bool Line>
inline void status_print_define(output<flavor::active_owner> out,
								reserve_leaf &, separator &, reserve_leaf &)
{
	++out.state->status_calls;
	out.state->events.push_back('A');
	out.state->bytes += Line ? "active\n" : "active";
}

template <bool Line, flavor Kind>
inline constexpr bool has_record_proof{
	::fast_io::semantic_status_free_record<Line, char, output<Kind>,
										   choice, reserve_leaf, choice, separator, choice, reserve_leaf, choice>};

static_assert(has_record_proof<false, flavor::proved>);
static_assert(has_record_proof<true, flavor::source_owner>);
static_assert(!has_record_proof<false, flavor::reference>);
static_assert(!has_record_proof<false, flavor::active_owner>);

#if __cplusplus > 202302L && ((defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 11) || (defined(__clang__) && __clang_major__ >= 13))
static_assert(::fast_io::operations::decay::print_semantic_linear_control_plan_available<
			  false, char, output<flavor::proved>, choice, reserve_leaf, choice, separator, choice, reserve_leaf, choice>());
static_assert(!::fast_io::operations::decay::print_semantic_linear_control_plan_available<
			  false, char, output<flavor::reference>, choice, reserve_leaf, choice, separator, choice, reserve_leaf, choice>());
static_assert(!::fast_io::operations::decay::print_semantic_linear_control_plan_available<
			  false, char, output<flavor::active_owner>, choice, reserve_leaf, choice, separator, choice, reserve_leaf, choice>());
#endif

template <flavor Kind, bool Line>
void run(capture &state, unsigned mask)
{
	auto first{::fast_io::mnp::cond((mask & 1u) != 0u, "aa")};
	auto second{::fast_io::mnp::cond((mask & 2u) != 0u, "bb")};
	auto third{::fast_io::mnp::cond((mask & 4u) != 0u, "cc")};
	auto fourth{::fast_io::mnp::cond((mask & 8u) != 0u, "dd")};
	reserve_leaf x{&state, 'X'};
	reserve_leaf y{&state, 'Y'};
	separator delimiter{'|'};
	output<Kind> out{&state};
	// Start at the existing stable-reference boundary: the semantic nodes are already factory-normalized, and every
	// source remains the same named object through source status, condition selection, and active status dispatch.
	::fast_io::operations::decay::print_freestanding_decay_impl<Line>(
		out, first, x, second, delimiter, third, y, fourth);
}

template <bool Line>
void check(unsigned mask)
{
	::std::string expected;
	if ((mask & 1u) != 0u)
	{
		expected += "aa";
	}
	expected += 'X';
	if ((mask & 2u) != 0u)
	{
		expected += "bb";
	}
	expected += '|';
	if ((mask & 4u) != 0u)
	{
		expected += "cc";
	}
	expected += 'Y';
	if ((mask & 8u) != 0u)
	{
		expected += "dd";
	}
	if constexpr (Line)
	{
		expected += '\n';
	}

	capture reference;
	capture proved;
	run<flavor::reference, Line>(reference, mask);
	run<flavor::proved, Line>(proved, mask);
	assert(reference.bytes == expected);
	assert(proved.bytes == expected);
	assert(proved.events == reference.events);
	assert(proved.status_calls == 0u);

	capture source;
	run<flavor::source_owner, Line>(source, mask);
	assert(source.status_calls == 1u);
	assert(source.events == "O");
	assert(source.bytes == (Line ? "source\n" : "source"));

	capture active;
	run<flavor::active_owner, Line>(active, mask);
	if (mask == 0u)
	{
		assert(active.status_calls == 1u);
		assert(active.events == "A");
		assert(active.bytes == (Line ? "active\n" : "active"));
	}
	else
	{
		assert(active.status_calls == 0u);
		assert(active.bytes == expected);
		assert(active.events == reference.events);
	}
}

} // namespace semantic_status_free_record_test

int main()
{
	for (unsigned mask{}; mask != 16u; ++mask)
	{
		semantic_status_free_record_test::check<false>(mask);
		semantic_status_free_record_test::check<true>(mask);
	}
}
