#include <concepts>
#include <cstddef>
#include <cstdlib>

#include <fast_io_core.h>

namespace fast_io_transmit_decay_abi_probe
{

struct input_state
{
	char const *current{};
	char const *last{};
};

struct output_state
{
	char *current{};
	char *last{};
};

struct safe_input
{
	using input_char_type = char;
	input_state *state{};
};

struct safe_output
{
	using output_char_type = char;
	output_state *state{};
};

struct inline_input
{
	using input_char_type = char;
	char const *current{};
	char const *last{};
};

struct inline_output
{
	using output_char_type = char;
	char *current{};
	char *last{};
};

// Copies of the safe proxies are substitutable because all cursor mutation is
// redirected into their external state blocks. The inline proxies deliberately
// omit this proof even though their representations are small and trivial.
inline constexpr ::std::true_type stream_ref_value_transport_safe_define(
	::fast_io::io_type_t<safe_input>) noexcept
{
	return {};
}

inline constexpr ::std::true_type stream_ref_value_transport_safe_define(
	::fast_io::io_type_t<safe_output>) noexcept
{
	return {};
}

inline constexpr safe_input &input_stream_ref_define(safe_input &stream) noexcept
{
	return stream;
}

inline constexpr safe_output &output_stream_ref_define(safe_output &stream) noexcept
{
	return stream;
}

inline constexpr inline_input &input_stream_ref_define(inline_input &stream) noexcept
{
	return stream;
}

inline constexpr inline_output &output_stream_ref_define(inline_output &stream) noexcept
{
	return stream;
}

inline char *read_some_underflow_define(safe_input stream, char *first,
										char *last) noexcept
{
	if (first == last || stream.state->current == stream.state->last)
	{
		return first;
	}
	*first = *stream.state->current;
	++stream.state->current;
	return first + 1;
}

inline char *read_some_underflow_define(inline_input &stream, char *first,
										char *last) noexcept
{
	if (first == last || stream.current == stream.last)
	{
		return first;
	}
	*first = *stream.current;
	++stream.current;
	return first + 1;
}

inline void write_all_overflow_define(safe_output stream, char const *first,
									  char const *last) noexcept
{
	for (; first != last; ++first)
	{
		if (stream.state->current == stream.state->last)
		{
			::std::abort();
		}
		*stream.state->current = *first;
		++stream.state->current;
	}
}

inline void write_all_overflow_define(inline_output &stream,
									  char const *first,
									  char const *last) noexcept
{
	for (; first != last; ++first)
	{
		if (stream.current == stream.last)
		{
			::std::abort();
		}
		*stream.current = *first;
		++stream.current;
	}
}

using count_type = ::fast_io::uintfpos_t;
using owner_entry = count_type (*)(safe_output, safe_input, count_type);
using borrowed_entry = count_type (*)(safe_output &, safe_input &, count_type);
using output_value_entry = count_type (*)(safe_output, inline_input &, count_type);
using input_value_entry = count_type (*)(inline_output &, safe_input, count_type);

static_assert(::fast_io::operations::defines::stream_ref_value_transport_safe<
			  safe_output>);
static_assert(::fast_io::operations::defines::stream_ref_value_transport_safe<
			  safe_input>);
static_assert(!::fast_io::operations::defines::stream_ref_value_transport_safe<
			  inline_output>);
static_assert(!::fast_io::operations::defines::stream_ref_value_transport_safe<
			  inline_input>);

#if defined(__aarch64__) || defined(__x86_64__) || defined(_M_X64)
static_assert(::fast_io::operations::defines::abi_value_output_stream_ref_result<
			  safe_output &>);
static_assert(::fast_io::operations::defines::abi_value_input_stream_ref_result<
			  safe_input &>);
#endif

/*
 * Taking each address makes the source-level ABI contract independently
 * inspectable. The owner has two genuine aggregate value parameters, the
 * borrowed entry has two object addresses, and the two mixed transport
 * specializations prove that one unmarked inline cursor cannot demote the
 * opposite safe proxy. On AArch64 and SysV x86-64 the one-word safe proxy is
 * consequently carried in its ordinary integer argument register.
 */
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transmit_some_decay<safe_output,
																		  safe_input>),
			  owner_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transmit_some_decay_borrowed<
					   safe_output, safe_input>),
			  borrowed_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transmit_stream_pair_count_transport<
					   true, true,
					   &::fast_io::operations::decay::transmit_some_decay_borrowed<safe_output,
																				   safe_input>,
					   safe_output, safe_input>),
			  owner_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transmit_stream_pair_count_transport<
					   true, false,
					   &::fast_io::operations::decay::transmit_some_decay_borrowed<safe_output,
																				   inline_input>,
					   safe_output, inline_input>),
			  output_value_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transmit_stream_pair_count_transport<
					   false, true,
					   &::fast_io::operations::decay::transmit_some_decay_borrowed<inline_output,
																				   safe_input>,
					   inline_output, safe_input>),
			  input_value_entry>);

} // namespace fast_io_transmit_decay_abi_probe

extern "C"
{

	[[gnu::used]] ::fast_io_transmit_decay_abi_probe::owner_entry
		fast_io_transmit_decay_owner_entry{
			&::fast_io::operations::decay::transmit_some_decay<
				::fast_io_transmit_decay_abi_probe::safe_output,
				::fast_io_transmit_decay_abi_probe::safe_input>};

	[[gnu::used]] ::fast_io_transmit_decay_abi_probe::borrowed_entry
		fast_io_transmit_decay_borrowed_entry{
			&::fast_io::operations::decay::transmit_some_decay_borrowed<
				::fast_io_transmit_decay_abi_probe::safe_output,
				::fast_io_transmit_decay_abi_probe::safe_input>};

	[[gnu::used]] ::fast_io_transmit_decay_abi_probe::output_value_entry
		fast_io_transmit_decay_output_value_entry{
			&::fast_io::operations::decay::transmit_stream_pair_count_transport<
				true, false,
				&::fast_io::operations::decay::transmit_some_decay_borrowed<
					::fast_io_transmit_decay_abi_probe::safe_output,
					::fast_io_transmit_decay_abi_probe::inline_input>,
				::fast_io_transmit_decay_abi_probe::safe_output,
				::fast_io_transmit_decay_abi_probe::inline_input>};

	[[gnu::used]] ::fast_io_transmit_decay_abi_probe::input_value_entry
		fast_io_transmit_decay_input_value_entry{
			&::fast_io::operations::decay::transmit_stream_pair_count_transport<
				false, true,
				&::fast_io::operations::decay::transmit_some_decay_borrowed<
					::fast_io_transmit_decay_abi_probe::inline_output,
					::fast_io_transmit_decay_abi_probe::safe_input>,
				::fast_io_transmit_decay_abi_probe::inline_output,
				::fast_io_transmit_decay_abi_probe::safe_input>};

	[[gnu::used, gnu::noinline]] ::fast_io_transmit_decay_abi_probe::count_type
	fast_io_transmit_decay_owner_abi_probe(
		::fast_io_transmit_decay_abi_probe::safe_output output,
		::fast_io_transmit_decay_abi_probe::safe_input input,
		::fast_io_transmit_decay_abi_probe::count_type count)
	{
		return fast_io_transmit_decay_owner_entry(output, input, count);
	}

	[[gnu::used, gnu::noinline]] ::fast_io_transmit_decay_abi_probe::count_type
	fast_io_transmit_decay_named_dispatch_abi_probe(
		::fast_io_transmit_decay_abi_probe::safe_output &output,
		::fast_io_transmit_decay_abi_probe::safe_input &input,
		::fast_io_transmit_decay_abi_probe::count_type count)
	{
		return ::fast_io::operations::decay::transmit_some_decay_dispatch(output,
																		  input, count);
	}

} // extern "C"

int main()
{
	char const source[]{'Q'};
	char destination[1]{};
	::fast_io_transmit_decay_abi_probe::input_state input_state{
		source, source + 1};
	::fast_io_transmit_decay_abi_probe::output_state output_state{
		destination, destination + 1};
	::fast_io_transmit_decay_abi_probe::safe_input input{
		__builtin_addressof(input_state)};
	::fast_io_transmit_decay_abi_probe::safe_output output{
		__builtin_addressof(output_state)};

	return fast_io_transmit_decay_owner_abi_probe(output, input, 1u) == 1u &&
				   input_state.current == source + 1 &&
				   output_state.current == destination + 1 && destination[0] == 'Q'
			   ? 0
			   : 1;
}
