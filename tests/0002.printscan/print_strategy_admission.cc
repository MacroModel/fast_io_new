#include <fast_io.h>

#include <cassert>
#include <cstddef>

namespace print_strategy_test
{

struct sink_state
{
	char buffer[64]{};
	::std::size_t size{};
	::std::size_t writes{};
	::std::size_t scatter_writes{};
};

struct native_scatter_sink
{
	using output_char_type = char;
	sink_state *state{};
};

inline constexpr native_scatter_sink output_stream_ref_define(native_scatter_sink sink) noexcept
{
	return sink;
}

inline void write_all_overflow_define(native_scatter_sink sink, char const *first, char const *last) noexcept
{
	++sink.state->writes;
	while (first != last)
	{
		sink.state->buffer[sink.state->size++] = *first++;
	}
}

inline void scatter_write_all_overflow_define(
	native_scatter_sink sink, ::fast_io::basic_io_scatter_t<char> const *scatters,
	::std::size_t count) noexcept
{
	++sink.state->scatter_writes;
	for (::std::size_t i{}; i != count; ++i)
	{
		write_all_overflow_define(sink, scatters[i].base, scatters[i].base + scatters[i].len);
	}
}

inline constexpr ::std::size_t full_output_coalesce_threshold(
	::fast_io::io_reserve_type_t<char, native_scatter_sink>) noexcept
{
	return 64u;
}

struct actual_only
{
	char value{};
};

inline void print_define(
	::fast_io::io_reserve_type_t<char, actual_only>, native_scatter_sink sink,
	actual_only value) noexcept
{
	write_all_overflow_define(sink, __builtin_addressof(value.value),
							  __builtin_addressof(value.value) + 1);
}

struct dummy_only
{};

inline void print_define(
	::fast_io::io_reserve_type_t<char, dummy_only>,
	::fast_io::details::dummy_buffer_output_stream<char>, dummy_only) noexcept
{}

struct nontrivial_actual
{
	char value{};
	~nontrivial_actual() {}
};

inline void print_define(
	::fast_io::io_reserve_type_t<char, nontrivial_actual>, native_scatter_sink sink,
	nontrivial_actual &value) noexcept
{
	write_all_overflow_define(sink, __builtin_addressof(value.value),
							  __builtin_addressof(value.value) + 1);
}

struct dual_protocol
{
	::std::size_t *reserve_size_calls{};
	::std::size_t *reserve_define_calls{};
	::std::size_t *scatter_define_calls{};
};

inline ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, dual_protocol>, dual_protocol value) noexcept
{
	++*value.reserve_size_calls;
	return 1u;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, dual_protocol>, char *iter,
	dual_protocol value) noexcept
{
	++*value.reserve_define_calls;
	*iter++ = 'R';
	return iter;
}

inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, dual_protocol>) noexcept
{
	return {1u, 0u};
}

inline ::fast_io::basic_reserve_scatters_define_result<char> print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, dual_protocol>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	dual_protocol value) noexcept
{
	++*value.scatter_define_calls;
	static constexpr char text[]{'S'};
	*scatters = {text, 1u};
	return {scatters + 1, reserve};
}

struct staged_counters
{
	::std::size_t reserve_defines{};
	::std::size_t prepares{};
	::std::size_t staged_defines{};
};

struct staged_value
{
	char value{};
	staged_counters *counters{};
};

struct staged_state
{
	char value{};
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, staged_value>) noexcept
{
	return 1u;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, staged_value>, char *iter,
	staged_value value) noexcept
{
	++value.counters->reserve_defines;
	*iter++ = value.value;
	return iter;
}

inline constexpr ::fast_io::io_type_t<staged_state> print_staged_type(
	::fast_io::io_reserve_type_t<char, staged_value>) noexcept
{
	return {};
}

inline constexpr ::std::size_t print_staged_width(
	::fast_io::io_reserve_type_t<char, staged_value>) noexcept
{
	return 2u;
}

inline constexpr bool print_staged_eligible(
	::fast_io::io_reserve_type_t<char, staged_value>, staged_value const &) noexcept
{
	return true;
}

inline staged_state print_staged_prepare(
	::fast_io::io_reserve_type_t<char, staged_value>, staged_value const &value) noexcept
{
	++value.counters->prepares;
	return {value.value};
}

inline char *print_staged_define(
	::fast_io::io_reserve_type_t<char, staged_value>, char *iter,
	staged_value const &value, staged_state const &state) noexcept
{
	++value.counters->staged_defines;
	*iter++ = state.value;
	return iter;
}

// The staged protocol is specified on a const source expression. This hostile overload set proves that emission does
// not accidentally rediscover a mutable-lvalue CPO after the concept has admitted the const, non-throwing operation.
struct const_only_staged_value
{
	char value{};
};

struct const_only_staged_state
{
	char value{};
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, const_only_staged_value>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, const_only_staged_value>, char *iter,
	const_only_staged_value value) noexcept
{
	*iter++ = value.value;
	return iter;
}

inline constexpr ::fast_io::io_type_t<const_only_staged_state> print_staged_type(
	::fast_io::io_reserve_type_t<char, const_only_staged_value>) noexcept
{
	return {};
}

inline constexpr ::std::size_t print_staged_width(
	::fast_io::io_reserve_type_t<char, const_only_staged_value>) noexcept
{
	return 2u;
}

inline constexpr bool print_staged_eligible(
	::fast_io::io_reserve_type_t<char, const_only_staged_value>,
	const_only_staged_value const &) noexcept
{
	return true;
}

inline constexpr const_only_staged_state print_staged_prepare(
	::fast_io::io_reserve_type_t<char, const_only_staged_value>,
	const_only_staged_value const &value) noexcept
{
	return {value.value};
}

inline constexpr char *print_staged_define(
	::fast_io::io_reserve_type_t<char, const_only_staged_value>, char *iter,
	const_only_staged_value const &, const_only_staged_state const &state) noexcept
{
	*iter++ = state.value;
	return iter;
}

inline char *print_staged_define(
	::fast_io::io_reserve_type_t<char, const_only_staged_value>, char *,
	const_only_staged_value &, const_only_staged_state const &) = delete;

struct oversized_staged_state
{
	char storage[4096]{};
};

struct oversized_staged_value
{};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, oversized_staged_value>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, oversized_staged_value>, char *iter,
	oversized_staged_value) noexcept
{
	*iter++ = 'X';
	return iter;
}

inline constexpr ::fast_io::io_type_t<oversized_staged_state> print_staged_type(
	::fast_io::io_reserve_type_t<char, oversized_staged_value>) noexcept
{
	return {};
}

inline constexpr ::std::size_t print_staged_width(
	::fast_io::io_reserve_type_t<char, oversized_staged_value>) noexcept
{
	return 2u;
}

inline constexpr bool print_staged_eligible(
	::fast_io::io_reserve_type_t<char, oversized_staged_value>,
	oversized_staged_value const &) noexcept
{
	return true;
}

inline constexpr oversized_staged_state print_staged_prepare(
	::fast_io::io_reserve_type_t<char, oversized_staged_value>,
	oversized_staged_value const &) noexcept
{
	return {};
}

inline constexpr char *print_staged_define(
	::fast_io::io_reserve_type_t<char, oversized_staged_value>, char *iter,
	oversized_staged_value const &, oversized_staged_state const &) noexcept
{
	*iter++ = 'X';
	return iter;
}

using dummy_pack = ::fast_io::manipulators::pack_t<dummy_only>;
using actual_pack = ::fast_io::manipulators::pack_t<actual_only>;
using wrapped_nontrivial = decltype(
	::fast_io::io_print_forward<char>(::std::declval<nontrivial_actual &>()));

static_assert(::fast_io::printable<char, dummy_only>);
static_assert(!::fast_io::details::direct_printable_to<char, native_scatter_sink, dummy_only>);
static_assert(!::fast_io::printable<char, actual_only>);
static_assert(::fast_io::details::direct_printable_to<char, native_scatter_sink, actual_only>);
static_assert(::fast_io::details::direct_printable_to<char, native_scatter_sink, wrapped_nontrivial>);
static_assert(!::fast_io::operations::decay::defines::print_freestanding_okay<
	native_scatter_sink, dummy_only>);
static_assert(::fast_io::operations::decay::defines::print_freestanding_okay<
	native_scatter_sink, actual_only>);
static_assert(!::fast_io::operations::decay::defines::print_freestanding_okay<
	native_scatter_sink, dummy_pack>);
static_assert(::fast_io::operations::decay::defines::print_freestanding_okay<
	native_scatter_sink, actual_pack>);
static_assert(::fast_io::operations::decay::print_semantic_staged_group<
	char, staged_value, staged_value>::available);
static_assert(::fast_io::staged_printable<char, const_only_staged_value>);
static_assert(::fast_io::operations::decay::print_semantic_staged_group<
	char, const_only_staged_value, const_only_staged_value>::available);
static_assert(!::fast_io::operations::decay::print_semantic_staged_group<
	char, oversized_staged_value, oversized_staged_value>::available);

} // namespace print_strategy_test

int main()
{
	using namespace print_strategy_test;
	sink_state state{};
	native_scatter_sink sink{__builtin_addressof(state)};

	::fast_io::print(sink, actual_only{'A'});
	nontrivial_actual nontrivial{'N'};
	::fast_io::print(sink, nontrivial);
	assert(state.size == 2u && state.buffer[0] == 'A' && state.buffer[1] == 'N');

	::std::size_t reserve_size_calls{};
	::std::size_t reserve_define_calls{};
	::std::size_t scatter_define_calls{};
	::fast_io::print(sink, dual_protocol{__builtin_addressof(reserve_size_calls),
										__builtin_addressof(reserve_define_calls),
										__builtin_addressof(scatter_define_calls)});
	assert(reserve_size_calls == 0u && reserve_define_calls == 0u);
	assert(scatter_define_calls == 1u && state.buffer[2] == 'S');

	staged_counters counters{};
	::fast_io::print(sink, staged_value{'x', __builtin_addressof(counters)},
					 staged_value{'y', __builtin_addressof(counters)});
	assert(counters.reserve_defines == 0u);
	assert(counters.prepares == 2u && counters.staged_defines == 2u);
	assert(state.buffer[3] == 'x' && state.buffer[4] == 'y');

	::fast_io::print(sink, const_only_staged_value{'C'}, const_only_staged_value{'V'});
	assert(state.buffer[5] == 'C' && state.buffer[6] == 'V');
}
