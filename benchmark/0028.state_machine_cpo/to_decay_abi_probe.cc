#include <cstddef>
#include <cstdint>
#include <utility>

#include <fast_io_core.h>

namespace fast_io_to_decay_abi_probe
{

struct context_target
{
	::std::uint_least64_t digest{};
	::std::size_t size{};
	::std::size_t transitions{};
};

struct context_target_ref
{
	context_target *target{};
};

struct context_state
{
	::std::uint_least64_t digest{UINT64_C(14695981039346656037)};
	::std::size_t size{};
	::std::size_t transitions{};
};

inline constexpr context_target_ref scan_alias_define(
	::fast_io::io_alias_t, context_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

inline constexpr ::fast_io::io_type_t<context_state> scan_context_type(
	::fast_io::io_reserve_type_t<char, context_target_ref>) noexcept
{
	return {};
}

inline constexpr ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, context_target_ref>, context_state &state,
	char const *first, char const *last, context_target_ref) noexcept
{
	++state.transitions;
	for (; first != last; ++first)
	{
		state.digest =
			(state.digest ^ static_cast<unsigned char>(*first)) * UINT64_C(1099511628211);
		++state.size;
	}
	// Every printable object is one nonterminal state-machine fragment. This forces
	// the complete owned pack to remain live until the unique EOF transition.
	return {last, ::fast_io::parse_code::partial};
}

inline constexpr ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, context_target_ref>, context_state &state,
	context_target_ref target) noexcept
{
	target.target->digest = state.digest;
	target.target->size = state.size;
	target.target->transitions = state.transitions;
	return state.size == 0u ? ::fast_io::parse_code::end_of_file
							: ::fast_io::parse_code::ok;
}

inline constexpr ::std::true_type scan_context_result_in_range(
	::fast_io::io_reserve_type_t<char, context_target_ref>) noexcept
{
	// `scan_context_define` returns only its supplied `last`, which is in the
	// closed input range by construction.
	return {};
}

using scatter_type = ::fast_io::basic_io_scatter_t<char>;

template <::std::size_t... indices>
[[nodiscard]] inline context_target to_owned_pack(
	scatter_type const *sources, ::std::index_sequence<indices...>)
{
	// Each braced descriptor is a prvalue. Public source normalization and the
	// by-value decay entry therefore establish exactly one owner per component;
	// deeper helpers may only borrow those owners during this synchronous call.
	return ::fast_io::to<context_target>(
		scatter_type{sources[indices].base, sources[indices].len}...);
}

template <::std::size_t... indices>
inline void inplace_to_owned_pack(
	context_target &target, scatter_type const *sources,
	::std::index_sequence<indices...>)
{
	::fast_io::inplace_to(
		target, scatter_type{sources[indices].base, sources[indices].len}...);
}

[[nodiscard]] inline ::std::uint_least64_t observation(
	context_target const &target) noexcept
{
	return target.digest ^ static_cast<::std::uint_least64_t>(target.size) ^
		   (static_cast<::std::uint_least64_t>(target.transitions) << 48u);
}

} // namespace fast_io_to_decay_abi_probe

extern "C" [[gnu::noinline]] ::std::uint_least64_t
fast_io_to_decay_owned_p8(
	::fast_io_to_decay_abi_probe::scatter_type const *sources)
{
	auto const target{::fast_io_to_decay_abi_probe::to_owned_pack(
		sources, ::std::make_index_sequence<8u>{})};
	return ::fast_io_to_decay_abi_probe::observation(target);
}

extern "C" [[gnu::noinline]] ::std::uint_least64_t
fast_io_to_decay_owned_p32(
	::fast_io_to_decay_abi_probe::scatter_type const *sources)
{
	auto const target{::fast_io_to_decay_abi_probe::to_owned_pack(
		sources, ::std::make_index_sequence<32u>{})};
	return ::fast_io_to_decay_abi_probe::observation(target);
}

extern "C" [[gnu::noinline]] ::std::uint_least64_t
fast_io_inplace_to_decay_owned_p8(
	::fast_io_to_decay_abi_probe::scatter_type const *sources)
{
	::fast_io_to_decay_abi_probe::context_target target{};
	::fast_io_to_decay_abi_probe::inplace_to_owned_pack(
		target, sources, ::std::make_index_sequence<8u>{});
	return ::fast_io_to_decay_abi_probe::observation(target);
}

extern "C" [[gnu::noinline]] ::std::uint_least64_t
fast_io_inplace_to_decay_owned_p32(
	::fast_io_to_decay_abi_probe::scatter_type const *sources)
{
	::fast_io_to_decay_abi_probe::context_target target{};
	::fast_io_to_decay_abi_probe::inplace_to_owned_pack(
		target, sources, ::std::make_index_sequence<32u>{});
	return ::fast_io_to_decay_abi_probe::observation(target);
}
