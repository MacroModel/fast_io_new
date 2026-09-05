#include <array>
#include <concepts>
#include <cstdlib>

#include <fast_io_core.h>

namespace write_range_decay_transport_contract
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
	char *current{};
	char *last{};
};

struct output_state
{
	char *current{};
	char *last{};
};

struct substitutable_output
{
	using output_char_type = char;
	output_state *state{};
};

inline constexpr inline_output &output_stream_ref_define(
	inline_output &output) noexcept
{
	return output;
}

inline constexpr substitutable_output &output_stream_ref_define(
	substitutable_output &output) noexcept
{
	return output;
}

inline constexpr ::std::true_type stream_ref_value_transport_safe_define(
	::fast_io::io_type_t<substitutable_output>) noexcept
{
	// Every copy redirects cursor mutation into the same external state block.
	return {};
}

inline void write_all_overflow_define(
	inline_output &output, char const *first, char const *last) noexcept
{
	for (; first != last; ++first)
	{
		require(output.current != output.last);
		*output.current++ = *first;
	}
}

inline void write_all_overflow_define(
	substitutable_output output, char const *first, char const *last) noexcept
{
	for (; first != last; ++first)
	{
		require(output.state->current != output.state->last);
		*output.state->current++ = *first;
	}
}

using range_type = ::std::array<char, 3u>;
using owner_entry = void (*)(inline_output, range_type &);
using borrowed_entry = void (*)(inline_output &, range_type &);

/*
 * The unsuffixed entry is the historical value owner. Only the explicitly
 * named recursive graph may expose a reference parameter; range forwarding is
 * identical in both function types and therefore cannot hide the stream ABI.
 */
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::write_all_range_decay<
					   inline_output, range_type &>),
			  owner_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::
						   write_all_range_decay_borrowed_output<inline_output, range_type &>),
			  borrowed_entry>);
static_assert(!::fast_io::operations::defines::abi_value_output_stream_ref_result<
			  inline_output &>);

inline void verify_owner_and_dispatch() noexcept
{
	range_type source{'a', 'b', 'c'};

	char owner_storage[3]{};
	inline_output owner{owner_storage, owner_storage + 3};
	::fast_io::operations::decay::write_all_range_decay(owner, source);
	require(owner.current == owner_storage);
	require(owner_storage[0] == 'a' && owner_storage[1] == 'b' &&
			owner_storage[2] == 'c');

	char dispatch_storage[3]{};
	inline_output dispatch{dispatch_storage, dispatch_storage + 3};
	::fast_io::operations::decay::write_all_range_decay_dispatch(
		dispatch, source);
	require(dispatch.current == dispatch_storage + 3);
	require(dispatch_storage[0] == 'a' && dispatch_storage[1] == 'b' &&
			dispatch_storage[2] == 'c');

	char public_storage[3]{};
	inline_output public_output{public_storage, public_storage + 3};
	::fast_io::operations::write_all_range(public_output, source);
	require(public_output.current == public_storage + 3);
	require(public_storage[0] == 'a' && public_storage[1] == 'b' &&
			public_storage[2] == 'c');
}

inline void verify_substitutable_dispatch() noexcept
{
	range_type source{'x', 'y', 'z'};
	char storage[3]{};
	output_state state{storage, storage + 3};
	substitutable_output output{__builtin_addressof(state)};
	::fast_io::operations::decay::write_all_range_decay_dispatch(
		output, source);
	require(state.current == storage + 3);
	require(storage[0] == 'x' && storage[1] == 'y' && storage[2] == 'z');
}

} // namespace write_range_decay_transport_contract

int main()
{
	::write_range_decay_transport_contract::verify_owner_and_dispatch();
	::write_range_decay_transport_contract::verify_substitutable_dispatch();
}
