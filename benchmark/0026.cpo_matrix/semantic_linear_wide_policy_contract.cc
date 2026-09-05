// Build unchanged against the baseline and candidate, then compare stdout.
// Modes 0/1 use typed char/char32_t scatters and check constant evaluation.
// Modes 2/3 expose byte scatters plus typed scalar writes for char/char32_t.
// All optional leaves are closed literals/chvw/null; custom producers are mandatory.
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <initializer_list>
#include <type_traits>

#include <fast_io.h>

#ifndef FAST_IO_LINEAR_WIDE_MODE
#define FAST_IO_LINEAR_WIDE_MODE -1
#endif
#if FAST_IO_LINEAR_WIDE_MODE < -1 || FAST_IO_LINEAR_WIDE_MODE > 3
#error "FAST_IO_LINEAR_WIDE_MODE must be -1 (all) or 0 through 3"
#endif

namespace semantic_linear_wide_policy_contract
{

template <typename Char>
struct capture
{
	Char units[512]{};
	::std::size_t size{};
	::std::size_t events[512]{};
	::std::size_t event_count{};

	constexpr void event(char kind, ::std::size_t value)
	{
		assert(event_count + 2u <= 512u);
		events[event_count++] = static_cast<unsigned char>(kind);
		events[event_count++] = value;
	}

	constexpr void append(Char const *first, ::std::size_t count)
	{
		assert(size + count <= 512u);
		for (::std::size_t index{}; index != count; ++index)
		{
			units[size++] = first[index];
		}
	}
};

template <typename Char, bool Bytes>
struct output
{
	using output_char_type = Char;
	capture<Char> *state;
};

template <typename Char, bool Bytes>
inline constexpr output<Char, Bytes> output_stream_ref_define(output<Char, Bytes> out) noexcept
{
	return out;
}

template <typename Char, bool Bytes>
inline constexpr ::std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<Char, output<Char, Bytes>>) noexcept
{
	return {};
}

template <typename Char, bool Bytes>
inline constexpr ::std::true_type print_semantic_optional_scatter_plan_stream(
	::fast_io::io_reserve_type_t<Char, output<Char, Bytes>>) noexcept
{
	return {};
}

template <typename Char, bool Bytes>
inline constexpr void write_all_overflow_define(output<Char, Bytes> out, Char const *first, Char const *last)
{
	auto const size{static_cast<::std::size_t>(last - first)};
	out.state->event('W', size);
	out.state->append(first, size);
}

template <typename Char>
inline constexpr void scatter_write_all_overflow_define(output<Char, false> out,
														::fast_io::basic_io_scatter_t<Char> const *scatters, ::std::size_t count)
{
	out.state->event('S', count);
	for (::std::size_t index{}; index != count; ++index)
	{
		out.state->event('E', scatters[index].len);
		out.state->append(scatters[index].base, scatters[index].len);
	}
}

template <typename Char>
inline void scatter_write_all_bytes_overflow_define(output<Char, true> out,
													::fast_io::io_scatter_t const *scatters, ::std::size_t count)
{
	out.state->event('B', count);
	for (::std::size_t index{}; index != count; ++index)
	{
		// The byte length includes complete output code units, including a
		// separate newline descriptor. Never interpret it as a character count.
		assert(scatters[index].len % sizeof(Char) == 0u);
		out.state->event('E', scatters[index].len);
		out.state->append(static_cast<Char const *>(scatters[index].base), scatters[index].len / sizeof(Char));
	}
}

template <typename Char>
inline constexpr Char reserve_unit{static_cast<Char>(sizeof(Char) == 1u ? 'r' : 0x4e2du)};
template <typename Char>
inline constexpr Char dynamic_unit{static_cast<Char>(sizeof(Char) == 1u ? 'd' : 0x1f642u)};

template <typename Char>
struct reserve
{
	capture<Char> *state;
};

template <typename Char>
inline constexpr ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<Char, reserve<Char>>) noexcept
{
	return 2u;
}

template <typename Char>
inline constexpr Char *print_reserve_define(::fast_io::io_reserve_type_t<Char, reserve<Char>>,
											Char *first, reserve<Char> value)
{
	value.state->event('R', 2u);
	*first++ = reserve_unit<Char>;
	*first++ = reserve_unit<Char>;
	return first;
}

template <typename Char>
struct dynamic
{
	capture<Char> *state;
	::std::size_t size;
};

template <typename Char>
inline constexpr ::std::size_t print_reserve_static_stack_size(
	::fast_io::io_reserve_type_t<Char, dynamic<Char>>) noexcept
{
	return 8u;
}

template <typename Char>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<Char, dynamic<Char>>, dynamic<Char> value)
{
	value.state->event('Q', value.size);
	return value.size;
}

template <typename Char>
inline constexpr Char *print_reserve_define(::fast_io::io_reserve_type_t<Char, dynamic<Char>>,
											Char *first, dynamic<Char> value)
{
	value.state->event('D', value.size);
	for (::std::size_t index{}; index != value.size; ++index)
	{
		*first++ = dynamic_unit<Char>;
	}
	return first;
}

template <typename Char>
struct context
{
	capture<Char> *state;
	::std::size_t size;
};

template <typename Char>
struct context_state
{
	::std::size_t position{};

	inline constexpr ::fast_io::context_print_result<Char *> print_context_define(
		context<Char> value, Char *first, Char *last)
	{
		value.state->event('C', static_cast<::std::size_t>(last - first));
		while (first != last && position != value.size)
		{
			*first++ = static_cast<Char>('a' + position++ % 26u);
		}
		value.state->event('F', position);
		return {first, position == value.size};
	}
};

template <typename Char>
inline constexpr ::fast_io::io_type_t<context_state<Char>> print_context_type(
	::fast_io::io_reserve_type_t<Char, context<Char>>) noexcept
{
	return {};
}

template <typename Char>
inline constexpr ::std::size_t print_context_static_buffer_size(
	::fast_io::io_reserve_type_t<Char, context<Char>>) noexcept
{
	return 16u;
}

template <typename Char>
struct direct
{
	capture<Char> *state;
};

template <typename Char, typename Output>
inline constexpr void print_define(::fast_io::io_reserve_type_t<Char, direct<Char>>, Output out, direct<Char> value)
{
	value.state->event('P', 1u);
	::fast_io::operations::print_freestanding<false>(out, ::fast_io::mnp::chvw(static_cast<Char>('!')));
}

template <typename Char>
inline constexpr Char literal_one[]{static_cast<Char>('A'), Char{}};
template <typename Char>
inline constexpr Char literal_two[]{static_cast<Char>('B'), static_cast<Char>('B'), Char{}};
template <typename Char>
inline constexpr Char literal_three[]{static_cast<Char>('C'), static_cast<Char>('C'), static_cast<Char>('C'), static_cast<Char>('C'), Char{}};
template <typename Char>
inline constexpr Char literal_four[]{static_cast<Char>('D'), static_cast<Char>('D'), static_cast<Char>('D'), static_cast<Char>('D'), static_cast<Char>('D'), Char{}};

template <bool Context, bool Line, typename Char, bool Bytes>
inline constexpr void emit(output<Char, Bytes> out, unsigned mask, ::std::size_t length)
{
	reserve<Char> fixed{out.state};
	dynamic<Char> sized{out.state, length};
	::std::conditional_t<Context, context<Char>, direct<Char>> barrier{[&] {
		if constexpr (Context)
		{
			return context<Char>{out.state, length};
		}
		else
		{
			return direct<Char>{out.state};
		}
	}()};
	// A nullable last static scatter distinguishes a separate newline
	// descriptor from a newline appended to the preceding reserve group.
	::fast_io::operations::print_freestanding<Line>(out,
													::fast_io::mnp::cond((mask & 1u) != 0u, literal_one<Char>), fixed,
													::fast_io::mnp::cond((mask & 2u) != 0u, literal_two<Char>), sized, barrier,
													::fast_io::mnp::cond((mask & 4u) != 0u, literal_three<Char>), fixed,
													::fast_io::mnp::cond((mask & 8u) != 0u, literal_four<Char>));
}

template <typename Char, bool Context>
inline constexpr bool constant_evaluation_contract()
{
	capture<Char> state;
	emit<Context, true>(output<Char, false>{&state}, 15u, 3u);
	char const *expected{Context ? "ArrBBdddabcCCCCrrDDDDD\n" : "ArrBBddd!CCCCrrDDDDD\n"};
	::std::size_t position{};
	for (; expected[position] != '\0'; ++position)
	{
		auto const value{expected[position] == 'r' ? reserve_unit<Char> : expected[position] == 'd' ? dynamic_unit<Char>
																									: static_cast<Char>(expected[position])};
		if (position == state.size || state.units[position] != value)
		{
			return false;
		}
	}
	return position == state.size && state.event_count != 0u;
}

#if FAST_IO_LINEAR_WIDE_MODE == -1 || FAST_IO_LINEAR_WIDE_MODE == 0
static_assert(constant_evaluation_contract<char, false>());
static_assert(constant_evaluation_contract<char, true>());
#endif
#if FAST_IO_LINEAR_WIDE_MODE == -1 || FAST_IO_LINEAR_WIDE_MODE == 1
static_assert(constant_evaluation_contract<char32_t, false>());
static_assert(constant_evaluation_contract<char32_t, true>());
#endif

template <unsigned Mode, bool Context>
void check()
{
	using Char = ::std::conditional_t<Mode % 2u == 0u, char, char32_t>;
	for (::std::size_t length : {0u, 1u, 8u, 9u, 31u, 32u, 33u})
	{
		for (unsigned mask : {0u, 5u, 10u, 15u})
		{
			for (bool line : {false, true})
			{
				capture<Char> state;
				output<Char, (Mode >= 2u)> out{&state};
				if (line)
				{
					emit<Context, true>(out, mask, length);
				}
				else
				{
					emit<Context, false>(out, mask, length);
				}
				::std::printf("mode=%u context=%u line=%u mask=%u length=%zu units=%zu\n",
							  Mode, static_cast<unsigned>(Context), static_cast<unsigned>(line), mask, length, state.size);
				for (::std::size_t index{}; index != state.size; ++index)
				{
					::std::printf("%08x,", static_cast<unsigned>(state.units[index]));
				}
				::std::putchar('\n');
				for (::std::size_t index{}; index != state.event_count; ++index)
				{
					::std::printf("%zu,", state.events[index]);
				}
				::std::putchar('\n');
			}
		}
	}
}

} // namespace semantic_linear_wide_policy_contract

int main()
{
	using namespace semantic_linear_wide_policy_contract;
#if FAST_IO_LINEAR_WIDE_MODE == -1
	check<0u, false>();
	check<0u, true>();
	check<1u, false>();
	check<1u, true>();
	check<2u, false>();
	check<2u, true>();
	check<3u, false>();
	check<3u, true>();
#else
	check<FAST_IO_LINEAR_WIDE_MODE, false>();
	check<FAST_IO_LINEAR_WIDE_MODE, true>();
#endif
}
