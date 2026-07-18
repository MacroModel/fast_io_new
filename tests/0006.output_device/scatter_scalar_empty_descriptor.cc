#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <string>
#include <vector>

#include <fast_io_core.h>

namespace
{

enum class scalar_primitive
{
	character_all,
	character_some,
	byte_all,
	byte_some,
	character_pwrite_all,
	character_pwrite_some,
	byte_pwrite_all,
	byte_pwrite_some
};

struct scalar_call_state
{
	::std::vector<void const *> firsts;
	::std::vector<void const *> lasts;
	::std::vector<::std::size_t> sizes;
	::std::vector<::fast_io::intfpos_t> offsets;
	::std::string bytes;
};

template <typename char_type, scalar_primitive primitive>
struct scalar_sink
{
	using output_char_type = char_type;
	scalar_call_state *state;
};

template <typename char_type, scalar_primitive primitive>
inline constexpr scalar_sink<char_type, primitive>
output_stream_ref_define(scalar_sink<char_type, primitive> sink) noexcept
{
	return sink;
}

template <typename element_type>
inline void record_scalar_call(scalar_call_state &state, element_type const *first,
							   element_type const *last, ::fast_io::intfpos_t offset)
{
	// The test intentionally subtracts the pair supplied by the strategy.  Thus a null/null pair is not merely observed;
	// it is rejected under the same pointer-domain requirement imposed by real buffering and adapter implementations.
	assert(first != nullptr);
	assert(last != nullptr);
	auto const distance{last - first};
	assert(distance >= 0);
	auto const size{static_cast<::std::size_t>(distance)};
	state.firsts.push_back(first);
	state.lasts.push_back(last);
	state.sizes.push_back(size);
	state.offsets.push_back(offset);
	if constexpr (::std::same_as<element_type, char>)
	{
		state.bytes.append(first, size);
	}
	else if constexpr (::std::same_as<element_type, ::std::byte>)
	{
		state.bytes.append(reinterpret_cast<char const *>(first), size);
	}
}

template <typename char_type>
inline void write_all_overflow_define(
	scalar_sink<char_type, scalar_primitive::character_all> sink,
	char_type const *first, char_type const *last)
{
	record_scalar_call(*sink.state, first, last, 0);
}

template <typename char_type>
inline char_type const *write_some_overflow_define(
	scalar_sink<char_type, scalar_primitive::character_some> sink,
	char_type const *first, char_type const *last)
{
	record_scalar_call(*sink.state, first, last, 0);
	return last;
}

template <typename char_type>
inline void write_all_bytes_overflow_define(
	scalar_sink<char_type, scalar_primitive::byte_all> sink,
	::std::byte const *first, ::std::byte const *last)
{
	record_scalar_call(*sink.state, first, last, 0);
}

template <typename char_type>
inline ::std::byte const *write_some_bytes_overflow_define(
	scalar_sink<char_type, scalar_primitive::byte_some> sink,
	::std::byte const *first, ::std::byte const *last)
{
	record_scalar_call(*sink.state, first, last, 0);
	return last;
}

template <typename char_type>
inline void pwrite_all_overflow_define(
	scalar_sink<char_type, scalar_primitive::character_pwrite_all> sink,
	char_type const *first, char_type const *last, ::fast_io::intfpos_t offset)
{
	record_scalar_call(*sink.state, first, last, offset);
}

template <typename char_type>
inline char_type const *pwrite_some_overflow_define(
	scalar_sink<char_type, scalar_primitive::character_pwrite_some> sink,
	char_type const *first, char_type const *last, ::fast_io::intfpos_t offset)
{
	record_scalar_call(*sink.state, first, last, offset);
	return last;
}

template <typename char_type>
inline void pwrite_all_bytes_overflow_define(
	scalar_sink<char_type, scalar_primitive::byte_pwrite_all> sink,
	::std::byte const *first, ::std::byte const *last, ::fast_io::intfpos_t offset)
{
	record_scalar_call(*sink.state, first, last, offset);
}

template <typename char_type>
inline ::std::byte const *pwrite_some_bytes_overflow_define(
	scalar_sink<char_type, scalar_primitive::byte_pwrite_some> sink,
	::std::byte const *first, ::std::byte const *last, ::fast_io::intfpos_t offset)
{
	record_scalar_call(*sink.state, first, last, offset);
	return last;
}

consteval bool scalar_range_normalization_is_pointer_domain_safe()
{
	auto const null_empty{::fast_io::details::scatter_to_scalar_range<char>(nullptr, 0u)};
	char value{};
	auto const named_empty{::fast_io::details::scatter_to_scalar_range(&value, 0u)};
	auto const nonempty{::fast_io::details::scatter_to_scalar_range(&value, 1u)};
	return null_empty.first == null_empty.last &&
		   named_empty.first == &value && named_empty.last == &value &&
		   nonempty.first == &value && nonempty.last == &value + 1u;
}

static_assert(scalar_range_normalization_is_pointer_domain_safe());

inline void verify_character_state(scalar_call_state const &state, char const *preserved_empty_base,
								bool positional)
{
	assert(state.sizes == (::std::vector<::std::size_t>{0u, 1u, 0u, 1u, 0u}));
	assert(state.bytes == "AB");
	assert(state.firsts.size() == 5u);
	assert(state.firsts[0] != nullptr && state.firsts[0] == state.lasts[0]);
	assert(state.firsts[2] == preserved_empty_base && state.lasts[2] == preserved_empty_base);
	assert(state.firsts[4] != nullptr && state.firsts[4] == state.lasts[4]);
	if (positional)
	{
		assert(state.offsets ==
			   (::std::vector<::fast_io::intfpos_t>{37, 37, 38, 38, 39}));
	}
}

template <scalar_primitive primitive>
inline void verify_character_nonpositional(
	::std::array<::fast_io::basic_io_scatter_t<char>, 5u> const &scatters,
	char const *preserved_empty_base)
{
	scalar_call_state state;
	scalar_sink<char, primitive> sink{&state};
	auto const status{
		::fast_io::operations::scatter_write_some(sink, scatters.data(), scatters.size())};
	assert(status.position == scatters.size() && status.position_in_scatter == 0u);
	verify_character_state(state, preserved_empty_base, false);
	state = {};
	::fast_io::operations::scatter_write_all(sink, scatters.data(), scatters.size());
	verify_character_state(state, preserved_empty_base, false);
}

template <scalar_primitive primitive>
inline void verify_character_positional(
	::std::array<::fast_io::basic_io_scatter_t<char>, 5u> const &scatters,
	char const *preserved_empty_base)
{
	scalar_call_state state;
	scalar_sink<char, primitive> sink{&state};
	auto const status{::fast_io::operations::scatter_pwrite_some(
		sink, scatters.data(), scatters.size(), 37)};
	assert(status.position == scatters.size() && status.position_in_scatter == 0u);
	verify_character_state(state, preserved_empty_base, true);
	state = {};
	::fast_io::operations::scatter_pwrite_all(sink, scatters.data(), scatters.size(), 37);
	verify_character_state(state, preserved_empty_base, true);
}

inline void verify_byte_state(scalar_call_state const &state, void const *preserved_empty_base,
							 bool positional)
{
	assert(state.sizes == (::std::vector<::std::size_t>{0u, 1u, 0u, 1u, 0u}));
	assert(state.bytes == "AB");
	assert(state.firsts.size() == 5u);
	assert(state.firsts[0] != nullptr && state.firsts[0] == state.lasts[0]);
	assert(state.firsts[2] == preserved_empty_base && state.lasts[2] == preserved_empty_base);
	assert(state.firsts[4] != nullptr && state.firsts[4] == state.lasts[4]);
	if (positional)
	{
		assert(state.offsets ==
			   (::std::vector<::fast_io::intfpos_t>{37, 37, 38, 38, 39}));
	}
}

template <scalar_primitive primitive>
inline void verify_byte_nonpositional(
	::std::array<::fast_io::io_scatter_t, 5u> const &scatters,
	void const *preserved_empty_base)
{
	scalar_call_state state;
	scalar_sink<char, primitive> sink{&state};
	auto const status{::fast_io::operations::scatter_write_some_bytes(
		sink, scatters.data(), scatters.size())};
	assert(status.position == scatters.size() && status.position_in_scatter == 0u);
	verify_byte_state(state, preserved_empty_base, false);
	state = {};
	::fast_io::operations::scatter_write_all_bytes(sink, scatters.data(), scatters.size());
	verify_byte_state(state, preserved_empty_base, false);
}

template <scalar_primitive primitive>
inline void verify_byte_positional(
	::std::array<::fast_io::io_scatter_t, 5u> const &scatters,
	void const *preserved_empty_base)
{
	scalar_call_state state;
	scalar_sink<char, primitive> sink{&state};
	auto const status{::fast_io::operations::scatter_pwrite_some_bytes(
		sink, scatters.data(), scatters.size(), 37)};
	assert(status.position == scatters.size() && status.position_in_scatter == 0u);
	verify_byte_state(state, preserved_empty_base, true);
	state = {};
	::fast_io::operations::scatter_pwrite_all_bytes(sink, scatters.data(), scatters.size(), 37);
	verify_byte_state(state, preserved_empty_base, true);
}

template <scalar_primitive primitive, bool positional>
inline void verify_wide_typed_to_byte(
	::std::array<::fast_io::basic_io_scatter_t<char16_t>, 4u> const &scatters,
	char16_t const *preserved_empty_base)
{
	scalar_call_state state;
	scalar_sink<char16_t, primitive> sink{&state};
	auto const verify_state{[&]
	{
		assert(state.sizes ==
			   (::std::vector<::std::size_t>{0u, sizeof(char16_t), 0u, 0u}));
		assert(state.bytes.size() == sizeof(char16_t));
		assert(state.firsts[0] != nullptr && state.firsts[0] == state.lasts[0]);
		assert(state.firsts[2] == preserved_empty_base && state.lasts[2] == preserved_empty_base);
		assert(state.firsts[3] != nullptr && state.firsts[3] == state.lasts[3]);
		if constexpr (positional)
		{
			assert(state.offsets ==
				   (::std::vector<::fast_io::intfpos_t>{14, 14, 16, 16}));
		}
	}};
	if constexpr (positional)
	{
		auto const status{::fast_io::operations::scatter_pwrite_some(
			sink, scatters.data(), scatters.size(), 7)};
		assert(status.position == scatters.size() && status.position_in_scatter == 0u);
		verify_state();
		state = {};
		::fast_io::operations::scatter_pwrite_all(sink, scatters.data(), scatters.size(), 7);
	}
	else
	{
		auto const status{
			::fast_io::operations::scatter_write_some(sink, scatters.data(), scatters.size())};
		assert(status.position == scatters.size() && status.position_in_scatter == 0u);
		verify_state();
		state = {};
		::fast_io::operations::scatter_write_all(sink, scatters.data(), scatters.size());
	}
	verify_state();
}

} // namespace

int main()
{
	char payload[]{'A', 'B'};
	::std::array<::fast_io::basic_io_scatter_t<char>, 5u> const character_scatters{
		::fast_io::basic_io_scatter_t<char>{nullptr, 0u},
		::fast_io::basic_io_scatter_t<char>{payload, 1u},
		::fast_io::basic_io_scatter_t<char>{payload + 1u, 0u},
		::fast_io::basic_io_scatter_t<char>{payload + 1u, 1u},
		::fast_io::basic_io_scatter_t<char>{nullptr, 0u}};
	::std::array<::fast_io::io_scatter_t, 5u> const byte_scatters{
		::fast_io::io_scatter_t{nullptr, 0u}, ::fast_io::io_scatter_t{payload, 1u},
		::fast_io::io_scatter_t{payload + 1u, 0u},
		::fast_io::io_scatter_t{payload + 1u, 1u}, ::fast_io::io_scatter_t{nullptr, 0u}};

	verify_character_nonpositional<scalar_primitive::character_all>(character_scatters, payload + 1u);
	verify_character_nonpositional<scalar_primitive::character_some>(character_scatters, payload + 1u);
	verify_character_positional<scalar_primitive::character_pwrite_all>(character_scatters, payload + 1u);
	verify_character_positional<scalar_primitive::character_pwrite_some>(character_scatters, payload + 1u);
	verify_byte_nonpositional<scalar_primitive::byte_all>(byte_scatters, payload + 1u);
	verify_byte_nonpositional<scalar_primitive::byte_some>(byte_scatters, payload + 1u);
	verify_byte_positional<scalar_primitive::byte_pwrite_all>(byte_scatters, payload + 1u);
	verify_byte_positional<scalar_primitive::byte_pwrite_some>(byte_scatters, payload + 1u);

	char16_t wide_payload{u'Q'};
	::std::array<::fast_io::basic_io_scatter_t<char16_t>, 4u> const wide_scatters{
		::fast_io::basic_io_scatter_t<char16_t>{nullptr, 0u},
		::fast_io::basic_io_scatter_t<char16_t>{&wide_payload, 1u},
		::fast_io::basic_io_scatter_t<char16_t>{&wide_payload, 0u},
		::fast_io::basic_io_scatter_t<char16_t>{nullptr, 0u}};
	verify_wide_typed_to_byte<scalar_primitive::byte_all, false>(wide_scatters, &wide_payload);
	verify_wide_typed_to_byte<scalar_primitive::byte_some, false>(wide_scatters, &wide_payload);
	verify_wide_typed_to_byte<scalar_primitive::byte_pwrite_all, true>(wide_scatters, &wide_payload);
	verify_wide_typed_to_byte<scalar_primitive::byte_pwrite_some, true>(wide_scatters, &wide_payload);
}
