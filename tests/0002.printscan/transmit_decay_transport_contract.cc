#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <type_traits>

#include <fast_io_core.h>

namespace transmit_decay_transport_contract
{

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

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

inline constexpr inline_input &input_stream_ref_define(inline_input &stream) noexcept
{
	return stream;
}

inline constexpr inline_output &output_stream_ref_define(inline_output &stream) noexcept
{
	return stream;
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

inline void read_all_underflow_define(inline_input &stream, char *first,
									  char *last) noexcept
{
	for (; first != last; ++first)
	{
		require(stream.current != stream.last);
		*first = *stream.current;
		++stream.current;
	}
}

inline ::std::byte *read_some_bytes_underflow_define(
	inline_input &stream, ::std::byte *first, ::std::byte *last) noexcept
{
	if (first == last || stream.current == stream.last)
	{
		return first;
	}
	*first = static_cast<::std::byte>(
		static_cast<unsigned char>(*stream.current));
	++stream.current;
	return first + 1;
}

inline void read_all_bytes_underflow_define(
	inline_input &stream, ::std::byte *first, ::std::byte *last) noexcept
{
	for (; first != last; ++first)
	{
		require(stream.current != stream.last);
		*first = static_cast<::std::byte>(
			static_cast<unsigned char>(*stream.current));
		++stream.current;
	}
}

inline void write_all_overflow_define(inline_output &stream, char const *first,
									  char const *last) noexcept
{
	for (; first != last; ++first)
	{
		require(stream.current != stream.last);
		*stream.current = *first;
		++stream.current;
	}
}

inline void write_all_bytes_overflow_define(
	inline_output &stream, ::std::byte const *first,
	::std::byte const *last) noexcept
{
	for (; first != last; ++first)
	{
		require(stream.current != stream.last);
		*stream.current = static_cast<char>(::std::to_integer<unsigned char>(*first));
		++stream.current;
	}
}

using progress_ref = ::fast_io::uintfpos_transmit_reference_wrapper;
using count_type = ::fast_io::uintfpos_t;
using counted_all_owner = void (*)(inline_output, inline_input, count_type);
using counted_all_borrowed = void (*)(inline_output &, inline_input &, count_type);
using counted_some_owner = count_type (*)(inline_output, inline_input, count_type);
using counted_some_borrowed = count_type (*)(inline_output &, inline_input &, count_type);
using eof_owner = ::fast_io::transmit_result (*)(inline_output, inline_input);
using eof_borrowed = ::fast_io::transmit_result (*)(inline_output &, inline_input &);
using generic_owner = void (*)(inline_output, inline_input, progress_ref);
using generic_borrowed = void (*)(inline_output &, inline_input &, progress_ref &);

/*
 * Function types are the portable portion of the ABI contract. Every
 * unsuffixed decay specialization owns values exactly as the historical API
 * did, while the sole recursive graph exposes references explicitly. Covering
 * typed/byte and counted/EOF forms prevents one family from silently retaining
 * forwarding-reference transport.
 */
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transmit_all_decay<inline_output,
																		 inline_input>),
			  counted_all_owner>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transmit_all_decay_borrowed<
					   inline_output, inline_input>),
			  counted_all_borrowed>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transmit_some_decay<inline_output,
																		  inline_input>),
			  counted_some_owner>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transmit_some_decay_borrowed<
					   inline_output, inline_input>),
			  counted_some_borrowed>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transmit_bytes_all_decay<
					   inline_output, inline_input>),
			  counted_all_owner>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transmit_bytes_all_decay_borrowed<
					   inline_output, inline_input>),
			  counted_all_borrowed>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transmit_bytes_some_decay<
					   inline_output, inline_input>),
			  counted_some_owner>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transmit_bytes_some_decay_borrowed<
					   inline_output, inline_input>),
			  counted_some_borrowed>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transmit_until_eof_decay<
					   inline_output, inline_input>),
			  eof_owner>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transmit_until_eof_decay_borrowed<
					   inline_output, inline_input>),
			  eof_borrowed>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transmit_bytes_until_eof_decay<
					   inline_output, inline_input>),
			  eof_owner>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transmit_bytes_until_eof_decay_borrowed<
					   inline_output, inline_input>),
			  eof_borrowed>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transmit_until_eof_generic_decay<
					   inline_output, inline_input, progress_ref>),
			  generic_owner>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transmit_until_eof_generic_decay_borrowed<
					   inline_output, inline_input, progress_ref>),
			  generic_borrowed>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transmit_bytes_until_eof_generic_decay<
					   inline_output, inline_input, progress_ref>),
			  generic_owner>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::
						   transmit_bytes_until_eof_generic_decay_borrowed<
							   inline_output, inline_input, progress_ref>),
			  generic_borrowed>);

static_assert(::std::is_trivially_copyable_v<inline_input>);
static_assert(::std::is_trivially_copyable_v<inline_output>);
static_assert(!::fast_io::operations::defines::stream_ref_value_transport_safe<
			  inline_input>);
static_assert(!::fast_io::operations::defines::stream_ref_value_transport_safe<
			  inline_output>);
static_assert(!::fast_io::operations::defines::abi_value_input_stream_ref_result<
			  inline_input &>);
static_assert(!::fast_io::operations::defines::abi_value_output_stream_ref_result<
			  inline_output &>);

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

struct immovable_input
{
	using input_char_type = char;
	input_state *state{};

	inline explicit constexpr immovable_input(input_state *value) noexcept
		: state(value)
	{}
	immovable_input(immovable_input const &) = delete;
	immovable_input(immovable_input &&) = delete;
};

struct immovable_output
{
	using output_char_type = char;
	output_state *state{};

	inline explicit constexpr immovable_output(output_state *value) noexcept
		: state(value)
	{}
	immovable_output(immovable_output const &) = delete;
	immovable_output(immovable_output &&) = delete;
};

struct input_source
{
	input_state *state{};
};

struct output_source
{
	output_state *state{};
};

inline constexpr immovable_input input_stream_ref_define(input_source &source) noexcept
{
	return immovable_input{source.state};
}

inline constexpr immovable_output output_stream_ref_define(output_source &source) noexcept
{
	return immovable_output{source.state};
}

inline char *read_some_underflow_define(immovable_input &stream, char *first,
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

inline void write_all_overflow_define(immovable_output &stream,
									  char const *first,
									  char const *last) noexcept
{
	for (; first != last; ++first)
	{
		require(stream.state->current != stream.state->last);
		*stream.state->current = *first;
		++stream.state->current;
	}
}

struct immovable_progress
{
	::fast_io::uintfpos_t *value{};

	inline explicit constexpr immovable_progress(
		::fast_io::uintfpos_t *value_parameter) noexcept
		: value(value_parameter)
	{}
	immovable_progress(immovable_progress const &) = delete;
	immovable_progress(immovable_progress &&) = delete;
};

inline constexpr void transmit_integer_add_define(
	immovable_progress &progress, ::std::size_t amount) noexcept
{
	*progress.value += amount;
}

inline constexpr void transmit_integer_assign_from_uintfpos_define(
	immovable_progress &progress, ::fast_io::uintfpos_t value) noexcept
{
	*progress.value = value;
}

static_assert(::fast_io::details::transmit_integer_wrapper<immovable_progress>);

inline void test_value_owner_and_identity_dispatch() noexcept
{
	char const source[]{'A'};
	char owner_destination[1]{};
	inline_input owner_input{source, source + 1};
	inline_output owner_output{owner_destination, owner_destination + 1};

	require(::fast_io::operations::decay::transmit_some_decay(
				owner_output, owner_input, 1u) == 1u);
	// The explicit owner entry operates on its copies; payload storage is shared,
	// but the caller's inline cursors intentionally remain unchanged.
	require(owner_input.current == source);
	require(owner_output.current == owner_destination);
	require(owner_destination[0] == 'A');

	char dispatch_destination[1]{};
	inline_input dispatch_input{source, source + 1};
	inline_output dispatch_output{dispatch_destination,
								  dispatch_destination + 1};
	require(::fast_io::operations::decay::transmit_some_decay_dispatch(
				dispatch_output, dispatch_input, 1u) == 1u);
	require(dispatch_input.current == source + 1);
	require(dispatch_output.current == dispatch_destination + 1);
	require(dispatch_destination[0] == 'A');
}

inline void test_immovable_normalized_owners_and_progress() noexcept
{
	char const source[]{'Z'};
	char destination[1]{};
	input_state input{source, source + 1};
	output_state output{destination, destination + 1};
	input_source input_handle{__builtin_addressof(input)};
	output_source output_handle{__builtin_addressof(output)};
	::fast_io::uintfpos_t progress{};

	/*
	 * Each stream CPO and the public accumulator boundary create one immovable
	 * prvalue owner by guaranteed copy elision. The dispatch and recursive layers
	 * must borrow those owners; any attempted ABI copy is a substitution failure.
	 */
	::fast_io::operations::transmit_until_eof_generic(
		output_handle, input_handle,
		immovable_progress{__builtin_addressof(progress)});
	require(input.current == source + 1);
	require(output.current == destination + 1);
	require(destination[0] == 'Z');
	require(progress == 1u);
}

} // namespace transmit_decay_transport_contract

int main()
{
	::transmit_decay_transport_contract::test_value_owner_and_identity_dispatch();
	::transmit_decay_transport_contract::
		test_immovable_normalized_owners_and_progress();
}
