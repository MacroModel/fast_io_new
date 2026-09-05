#include <concepts>
#include <cstdlib>

#include <fast_io_core.h>

namespace finish_decay_transport_contract
{

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

struct inline_output
{
	using output_char_type = char;
	unsigned finishes{};
};

struct inline_input
{
	using input_char_type = char;
	unsigned finishes{};
};

struct substitutable_output
{
	using output_char_type = char;
	unsigned *finishes{};
};

struct substitutable_input
{
	using input_char_type = char;
	unsigned *finishes{};
};

inline constexpr inline_output &output_stream_ref_define(
	inline_output &stream) noexcept
{
	return stream;
}

inline constexpr inline_input &input_stream_ref_define(
	inline_input &stream) noexcept
{
	return stream;
}

inline constexpr substitutable_output &output_stream_ref_define(
	substitutable_output &stream) noexcept
{
	return stream;
}

inline constexpr substitutable_input &input_stream_ref_define(
	substitutable_input &stream) noexcept
{
	return stream;
}

inline constexpr ::std::true_type stream_ref_value_transport_safe_define(
	::fast_io::io_type_t<substitutable_output>) noexcept
{
	return {};
}

inline constexpr ::std::true_type stream_ref_value_transport_safe_define(
	::fast_io::io_type_t<substitutable_input>) noexcept
{
	return {};
}

inline void output_stream_finish_define(inline_output &stream) noexcept
{
	++stream.finishes;
}

inline void input_stream_drain_and_finish_define(inline_input &stream) noexcept
{
	++stream.finishes;
}

inline void output_stream_finish_define(substitutable_output stream) noexcept
{
	++*stream.finishes;
}

inline void input_stream_drain_and_finish_define(
	substitutable_input stream) noexcept
{
	++*stream.finishes;
}

using output_owner_entry = void (*)(inline_output);
using output_borrowed_entry = void (*)(inline_output &);
using input_owner_entry = void (*)(inline_input);
using input_borrowed_entry = void (*)(inline_input &);

/*
 * Terminal operations are particularly sensitive to identity: their only
 * visible effect may be stored in the observer itself. These function-type
 * checks distinguish the historical owner from the recursive borrow before
 * run-time checks exercise the mandatory-inline selector.
 */
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::output_stream_finish_decay<
					   inline_output>),
			  output_owner_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::output_stream_finish_decay_borrowed<
					   inline_output>),
			  output_borrowed_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::input_stream_drain_and_finish_decay<
					   inline_input>),
			  input_owner_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::
						   input_stream_drain_and_finish_decay_borrowed<inline_input>),
			  input_borrowed_entry>);

inline void verify_inline_identity() noexcept
{
	inline_output output{};
	::fast_io::operations::decay::output_stream_finish_decay(output);
	require(output.finishes == 0u);
	::fast_io::operations::decay::output_stream_finish_decay_dispatch(output);
	require(output.finishes == 1u);
	::fast_io::operations::output_stream_finish(output);
	require(output.finishes == 2u);

	inline_input input{};
	::fast_io::operations::decay::input_stream_drain_and_finish_decay(input);
	require(input.finishes == 0u);
	::fast_io::operations::decay::input_stream_drain_and_finish_decay_dispatch(
		input);
	require(input.finishes == 1u);
	::fast_io::operations::input_stream_drain_and_finish(input);
	require(input.finishes == 2u);
}

inline void verify_substitutable_values() noexcept
{
	unsigned output_finishes{};
	substitutable_output output{__builtin_addressof(output_finishes)};
	::fast_io::operations::decay::output_stream_finish_decay_dispatch(output);
	require(output_finishes == 1u);

	unsigned input_finishes{};
	substitutable_input input{__builtin_addressof(input_finishes)};
	::fast_io::operations::decay::input_stream_drain_and_finish_decay_dispatch(
		input);
	require(input_finishes == 1u);
}

} // namespace finish_decay_transport_contract

int main()
{
	::finish_decay_transport_contract::verify_inline_identity();
	::finish_decay_transport_contract::verify_substitutable_values();
}
