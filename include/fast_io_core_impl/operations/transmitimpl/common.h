#pragma once

namespace fast_io
{

namespace operations::decay::defines
{

// Observer transport invariant shared by every transmit strategy:
// Public transmit functions retain each stream-ref result in a `decltype(auto)` local. A prvalue therefore has exactly
// one owner, whereas ABI-aware normalization can preserve a stable lvalue for a large, non-trivial, or noncopyable
// observer. Decay and main layers accept forwarding/reference parameters only as lifetime-preserving borrows: they
// never forward those named parameters into another owner. At a mutex edge, the unlocked CPO result is likewise
// retained before recursion. These rules are stronger than an ABI-size heuristic because no observer copy edge remains
// in transmit at all; `abi_value_*` is consequently neither needed nor truthful as an admission condition here.

/// @brief Requires every synchronization marker participating in a transmit to provide its complete directional
///        protocol.
/// @details Transmit recursively replaces a locked stream with its unlocked reference. A marker without the matching
///          unlocked CPO, a storable exact-void mutex proxy, character preservation, or immediate type progress can
///          never satisfy that recursion. Rejecting the pair in constraints keeps such partial protocols out of every
///          transmit body instead of allowing a later, operation-dependent substitution failure.
///
///          This concept deliberately does not claim that two independent mutex proxies have a globally safe lock
///          order. The current mutex vocabulary exposes only `lock()` and `unlock()`; it provides neither comparable
///          identity nor `try_lock()`, so deadlock-free dual acquisition cannot be proved at the concept layer.
template <typename output, typename input>
concept has_complete_transmit_mutex_protocols =
	(!::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<
		 ::std::remove_cvref_t<output>> ||
	 ::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
		 ::std::remove_cvref_t<output>>) &&
	(!::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<
		 ::std::remove_cvref_t<input>> ||
	 ::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<::std::remove_cvref_t<input>>);

} // namespace operations::decay::defines

namespace details
{

template <typename T>
concept transmit_integer_wrapper = requires(T t, ::std::size_t off, ::fast_io::uintfpos_t uoff) {
	transmit_integer_add_define(t, off);
	transmit_integer_assign_from_uintfpos_define(t, uoff);
};

template <::std::size_t sz>
inline constexpr ::std::size_t calculate_transmit_buffer_size() noexcept
{
#ifdef FAST_IO_BUFFER_SIZE
	static_assert(sz >= FAST_IO_BUFFER_SIZE);
	static_assert(FAST_IO_BUFFER_SIZE < SIZE_MAX);
	return FAST_IO_BUFFER_SIZE / sz;
#else
	if constexpr (sizeof(::std::size_t) <= sizeof(::std::uint_least16_t))
	{
		return 4096 / sz;
	}
	else
	{
		return 131072 / sz;
	}
#endif
}

template <::std::size_t sz>
inline constexpr ::std::size_t transmit_buffer_size_cache{calculate_transmit_buffer_size<sz>()};

} // namespace details

struct uintfpos_transmit_reference_wrapper
{
	::fast_io::uintfpos_t *pfpos{};
};

inline constexpr void transmit_integer_assign_from_uintfpos_define(uintfpos_transmit_reference_wrapper t,
																   ::std::size_t off) noexcept
{
	*t.pfpos = off;
}

inline constexpr void transmit_integer_add_define(uintfpos_transmit_reference_wrapper t, ::std::size_t off) noexcept
{
	using commontype = ::std::common_type_t<::fast_io::uintfpos_t, ::std::size_t>;
	commontype val{static_cast<commontype>(*t.pfpos)};
	constexpr commontype mx{::std::numeric_limits<::fast_io::uintfpos_t>::max()};
	if constexpr (sizeof(::fast_io::uintfpos_t) < sizeof(::std::size_t))
	{
		if (mx < off)
		{
			*t.pfpos = mx;
			return;
		}
	}
	commontype mxval{static_cast<commontype>(static_cast<commontype>(mx) - off)};
	if (mxval < val)
	{
		*t.pfpos = mx;
	}
	else
	{
		*t.pfpos = val + off;
	}
}

struct transmit_result
{
	::fast_io::uintfpos_t transmitted{};
	inline constexpr bool is_overflowed() noexcept
	{
		constexpr auto mxval{::std::numeric_limits<::fast_io::uintfpos_t>::max()};
		return transmitted == mxval;
	}
};

/// @feature concept:runtime_precise_size
template <::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<char_type, transmit_result>)
{
	constexpr ::std::size_t sz{print_reserve_size(::fast_io::io_reserve_type<char_type, ::fast_io::uintfpos_t>) + 1};
	return sz;
}

template <::std::integral char_type>
inline constexpr char_type *print_reserve_define(::fast_io::io_reserve_type_t<char_type, transmit_result>,
												 char_type *iter, transmit_result r)
{
	::fast_io::uintfpos_t transmittedsz{r.transmitted};
	constexpr auto mxval{::std::numeric_limits<::fast_io::uintfpos_t>::max()};
	iter = print_reserve_define(::fast_io::io_reserve_type<char_type, ::fast_io::uintfpos_t>, iter, transmittedsz);
	if (transmittedsz == mxval)
	{
		*iter = ::fast_io::char_literal_v<u8'+', char_type>;
		++iter;
	}
	return iter;
}

} // namespace fast_io
