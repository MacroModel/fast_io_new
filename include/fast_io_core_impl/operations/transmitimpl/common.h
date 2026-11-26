#pragma once

/*
 * Shared admission, storage, and progress vocabulary for transmit (IO level).
 *
 * This file validates the two directional stream references and their complete
 * mutex projections, defines transfer-count adaptation, and selects bounded
 * fallback buffer sizes. Public transmit operations own each normalized
 * observer once; every recursive or buffered continuation borrows those stable
 * objects.
 */

namespace fast_io
{

namespace operations::decay::defines
{

/*
 * Observer transport invariant shared by every transmit strategy:
 *
 * Public transmit functions retain each stream-ref result in a `decltype(auto)`
 * local. A prvalue therefore has exactly one normalization owner, while a
 * mutable CPO lvalue keeps its original identity. Let Vout and Vin denote the corresponding
 * `abi_value_{output,input}_stream_ref_result<T &>` propositions, and define
 *
 *     P(V, T) = T  if V is true, otherwise T &.
 *
 * The mandatory-inline dispatch boundary selects
 * `operation(P(Vout, O), P(Vin, I), ...)`. Thus the two directions are
 * independent: an identity-sensitive input cannot force a proven output proxy
 * back to reference transport, or vice versa. Each V proposition includes both
 * the explicit ADL `stream_ref_value_transport_safe_define` substitution proof
 * and the target ABI argument policy; size or triviality alone is insufficient.
 *
 * The selected transport entry immediately borrows its named parameters into
 * one recursive implementation. Mutex recursion also stores every unlocked CPO
 * result once and continues through that borrowed implementation. Consequently
 * inline cursors, noncopyable observers, and immovable prvalue owners retain
 * exact identity, while explicitly substitutable small proxies can still cross
 * an outlined boundary as true value parameters. Final register allocation
 * remains the compiler's ABI decision. The historical unsuffixed `*_decay`
 * functions remain separate true by-value owner entries.
 */

/// @brief Maps one independently proved transmit direction to its ABI parameter form.
template <bool value_transport, typename observer>
using transmit_stream_transport_parameter =
	::std::conditional_t<value_transport, observer, observer &>;

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

namespace operations::decay
{

/**
 * @brief Materializes the independently selected stream pair for a counted transmit.
 * @details The non-type operation denotes the single borrowed implementation. Value
 *          parameters become named owners here and are never copied again.
 */
template <bool output_value_transport, bool input_value_transport,
		  auto borrowed_operation, typename output, typename input>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr decltype(auto) transmit_stream_pair_count_transport(
	::fast_io::operations::decay::defines::transmit_stream_transport_parameter<
		output_value_transport, output>
		output_stream,
	::fast_io::operations::decay::defines::transmit_stream_transport_parameter<
		input_value_transport, input>
		input_stream,
	::fast_io::uintfpos_t count)
{
	return borrowed_operation(output_stream, input_stream, count);
}

/** @brief Selects the two counted-transmit parameter forms at the mandatory-inline boundary. */
template <auto borrowed_operation, typename output, typename input>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr decltype(auto)
transmit_stream_pair_count_dispatch(output &output_stream, input &input_stream,
									::fast_io::uintfpos_t count)
{
	return ::fast_io::operations::decay::transmit_stream_pair_count_transport<
		::fast_io::operations::defines::abi_value_output_stream_ref_result<output &>,
		::fast_io::operations::defines::abi_value_input_stream_ref_result<input &>,
		borrowed_operation, output, input>(output_stream, input_stream, count);
}

/**
 * @brief Materializes the independently selected stream pair for an unbounded transmit.
 * @details This overload carries no operation state; both parameter decisions remain
 *          visible in the specialized function type and therefore in its target ABI.
 */
template <bool output_value_transport, bool input_value_transport,
		  auto borrowed_operation, typename output, typename input>
inline constexpr decltype(auto) transmit_stream_pair_transport(
	::fast_io::operations::decay::defines::transmit_stream_transport_parameter<
		output_value_transport, output>
		output_stream,
	::fast_io::operations::decay::defines::transmit_stream_transport_parameter<
		input_value_transport, input>
		input_stream)
{
	return borrowed_operation(output_stream, input_stream);
}

/** @brief Selects the two unbounded-transmit parameter forms at the mandatory-inline boundary. */
template <auto borrowed_operation, typename output, typename input>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr decltype(auto)
transmit_stream_pair_dispatch(output &output_stream, input &input_stream)
{
	return ::fast_io::operations::decay::transmit_stream_pair_transport<
		::fast_io::operations::defines::abi_value_output_stream_ref_result<output &>,
		::fast_io::operations::defines::abi_value_input_stream_ref_result<input &>,
		borrowed_operation, output, input>(output_stream, input_stream);
}

/**
 * @brief Materializes a selected stream pair while borrowing one progress accumulator.
 * @details Progress adaptation has no stream-ref substitution marker. Its owner is
 *          therefore created only by the historical/public value boundary and every
 *          recursive continuation observes that same named accumulator by reference.
 */
template <bool output_value_transport, bool input_value_transport,
		  auto borrowed_operation, typename output, typename input,
		  typename accumulator>
inline constexpr decltype(auto) transmit_stream_pair_accumulator_transport(
	::fast_io::operations::decay::defines::transmit_stream_transport_parameter<
		output_value_transport, output>
		output_stream,
	::fast_io::operations::decay::defines::transmit_stream_transport_parameter<
		input_value_transport, input>
		input_stream,
	accumulator &result)
{
	return borrowed_operation(output_stream, input_stream, result);
}

/** @brief Selects both stream forms without changing the accumulator's identity. */
template <auto borrowed_operation, typename output, typename input,
		  typename accumulator>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr decltype(auto)
transmit_stream_pair_accumulator_dispatch(output &output_stream,
										  input &input_stream,
										  accumulator &result)
{
	return ::fast_io::operations::decay::transmit_stream_pair_accumulator_transport<
		::fast_io::operations::defines::abi_value_output_stream_ref_result<output &>,
		::fast_io::operations::defines::abi_value_input_stream_ref_result<input &>,
		borrowed_operation, output, input, accumulator>(output_stream, input_stream,
														result);
}

} // namespace operations::decay

namespace details
{

template <typename T>
concept transmit_integer_wrapper = requires(T t, ::std::size_t off, ::fast_io::uintfpos_t uoff) {
	transmit_integer_add_define(t, off);
	transmit_integer_assign_from_uintfpos_define(t, uoff);
};

/// @brief Converts transmit's configured byte budget into a nonzero endpoint-element count.
/// @details `sz` is the size of one typed endpoint element (or one for byte transmit).  The configured budget must hold
///          at least one complete element; the former reversed comparison rejected every ordinary budget larger than
///          `sz` and admitted configurations whose integer division produced zero elements.  This relation matches the
///          generic buffer and transcoder policies: validate bytes first, then perform exactly one unit conversion.
template <::std::size_t sz>
inline constexpr ::std::size_t calculate_transmit_buffer_size() noexcept
{
	static_assert(sz != 0u);
#ifdef FAST_IO_BUFFER_SIZE
	static_assert(sz <= FAST_IO_BUFFER_SIZE);
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

/** @brief Bounds the staging space when the environment supplies no allocator.
 * @details The caller bounds every read by the same capacity and commits a block
 * after the read. The explicit size also bounds clearing for sensitive routes.
 */
template <typename Unit, ::std::size_t Capacity>
struct transmit_stack_storage
{
	Unit storage[Capacity];
	Unit *ptr{storage};
	::std::size_t size;

	inline constexpr explicit transmit_stack_storage(::std::size_t count) noexcept : size{count} {}
	transmit_stack_storage(transmit_stack_storage const &) = delete;
	transmit_stack_storage &operator=(transmit_stack_storage const &) = delete;
};

template <typename Unit, ::std::size_t Capacity, typename Allocator = ::fast_io::native_thread_local_allocator>
using transmit_batch_storage = ::std::conditional_t<allocator_provider_available<Allocator>,
	local_operator_new_array_ptr<Unit, Allocator>, transmit_stack_storage<Unit, Capacity>>;

/** @brief Keeps the ordinary I/O budget with an allocator and bounds stack staging to 256 bytes otherwise. */
template <typename Unit, typename Output, typename Input, typename Allocator = ::fast_io::native_thread_local_allocator>
inline constexpr ::std::size_t transmit_batch_size = [] {
	constexpr auto source{input_transcode_bytes<Input>};
	constexpr auto sink{output_transcode_bytes<Output>};
	constexpr auto capacity{transmit_buffer_size_cache<sizeof(Unit)>};
	constexpr auto budget{allocator_provider_available<Allocator> ? capacity : transcode_buffer_units<Unit>(256u, capacity)};
	// An unbuffered device may need a syscall per batch. Keep the bulk I/O budget on that edge.
	if constexpr (source && sink)
	{
		return transcode_buffer_units<Unit>(source < sink ? sink : source, budget);
	}
	else
	{
		return budget;
	}
}();

template <bool Bytes, typename Output, typename Input>
concept transmit_borrowable_window =
	(input_transcode_bytes<Input> != 0 || output_transcode_bytes<Output> != 0) &&
	(borrowed_terminal_input<Input> || requires(Input &in, ::std::size_t n) {
		{ ibuffer_underflow_reserve_define(in, n) } -> ::std::same_as<bool>;
	}) && operations::decay::defines::has_ibuffer_basic_operations<Input> &&
	((Bytes && transcode_input_window<::std::byte, Input>) ||
	 (!Bytes && ::std::same_as<typename Input::input_char_type, typename Output::output_char_type>));

#if defined(__HERBCEPTIONS__)
/** @brief Selects the checked refill only where the existing borrowed window can use it. */
template <typename Input>
concept checked_transmit_refill = requires(Input &in) {
	{ ibuffer_underflow_checked_define(in, ::std::size_t{}) } -> ::std::same_as<bool>;
	requires throws((ibuffer_underflow_checked_define(in, ::std::size_t{})));
};

/** @brief Combines the checked refill effect with the borrowed-window proof. */
template <bool Bytes, typename Output, typename Input>
concept checked_transmit_window = transmit_borrowable_window<Bytes, Output, Input> && checked_transmit_refill<Input>;

/** @brief Detects the checked staged read for the selected typed or byte representation. */
template <typename Input, typename Unit>
concept checked_transmit_transcode_read = requires(Input &in, Unit *position) {
	{ transcode_read_some_checked_define(in, position, position) } -> ::std::same_as<Unit *>;
	requires throws((transcode_read_some_checked_define(in, position, position)));
};
#endif

/** @brief Recognizes exact reads with only a terminal get cursor and its EOF fallback.
 * @details A short exact read then consumes its remaining source prefix.
 * Direct native reads and seek/read providers retain their own semantics.
 */
template <bool Bytes, typename Input>
concept transmit_exact_terminal_cursor = ::fast_io::borrowed_terminal_input<Input> &&
	operations::decay::defines::has_ibuffer_basic_operations<Input> &&
	(!Bytes || sizeof(typename Input::input_char_type) == 1u) &&
	!operations::decay::defines::has_any_of_read_operations<Input> &&
	!operations::decay::defines::has_any_of_read_bytes_operations<Input> &&
	!(operations::decay::defines::has_input_or_io_stream_seek_define<Input> &&
		operations::decay::defines::has_any_of_pread_operations<Input>) &&
	!(operations::decay::defines::has_input_or_io_stream_seek_bytes_define<Input> &&
		operations::decay::defines::has_any_of_pread_bytes_operations<Input>);

#if defined(__HERBCEPTIONS__)
/** @brief Retains the ordinary byte-to-character fallback for one-byte scalar observers. */
template <typename Input, typename Unit>
using transmit_scalar_read_unit = ::std::conditional_t<::std::same_as<Unit, ::std::byte> &&
	sizeof(typename Input::input_char_type) == 1 &&
	!operations::decay::defines::has_any_of_read_bytes_operations<Input>, typename Input::input_char_type, Unit>;

template <typename Output, typename Unit>
using transmit_scalar_write_unit = ::std::conditional_t<::std::same_as<Unit, ::std::byte> &&
	sizeof(typename Output::output_char_type) == 1 &&
	!operations::decay::defines::has_any_of_write_bytes_operations<Output>, typename Output::output_char_type, Unit>;

/** @brief Discovers the selected unbuffered scalar read effect without calling the provider. */
template <typename Input, typename Unit>
inline constexpr bool transmit_scalar_read_throws = [] {
	using Leaf = transmit_scalar_read_unit<Input, Unit>;
	namespace definitions = operations::decay::defines;
	if constexpr (definitions::has_ibuffer_basic_operations<Input>) return false;
	else if constexpr (::std::same_as<Leaf, ::std::byte>)
	{
		if constexpr (definitions::has_read_some_bytes_underflow_define<Input>)
			return throws((read_some_bytes_underflow_define(::std::declval<Input &>(),
				static_cast<Leaf *>(nullptr), static_cast<Leaf *>(nullptr))));
		else return false;
	}
	else if constexpr (definitions::has_read_some_underflow_define<Input>)
		return throws((read_some_underflow_define(::std::declval<Input &>(),
			static_cast<Leaf *>(nullptr), static_cast<Leaf *>(nullptr))));
	else return false;
}();

/** @brief Discovers the selected unbuffered scalar completion effect without calling the provider. */
template <typename Output, typename Unit>
inline constexpr bool transmit_scalar_write_throws = [] {
	using Leaf = transmit_scalar_write_unit<Output, Unit>;
	namespace definitions = operations::decay::defines;
	if constexpr (definitions::has_obuffer_basic_operations<Output>) return false;
	else if constexpr (::std::same_as<Leaf, ::std::byte>)
	{
		if constexpr (definitions::has_write_all_bytes_overflow_define<Output>)
			return throws((write_all_bytes_overflow_define(::std::declval<Output &>(),
				static_cast<Leaf const *>(nullptr), static_cast<Leaf const *>(nullptr))));
		else return false;
	}
	else if constexpr (definitions::has_write_all_overflow_define<Output>)
		return throws((write_all_overflow_define(::std::declval<Output &>(),
			static_cast<Leaf const *>(nullptr), static_cast<Leaf const *>(nullptr))));
	else return false;
}();

/** @brief Recognizes exact reads completed by repeated scalar reads in the selected domain.
 * @details An explicit completion CPO retains its own failure contract. A scalar
 * fallback instead defines EOF by zero progress, which the caller can report in
 * its chosen exception ABI.
 */
template <bool Bytes, typename Input>
inline constexpr bool transmit_exact_uses_scalar_read = [] {
	using Unit = ::std::conditional_t<Bytes, ::std::byte, typename Input::input_char_type>;
	using Leaf = transmit_scalar_read_unit<Input, Unit>;
	namespace definitions = operations::decay::defines;
	if constexpr (::std::same_as<Leaf, ::std::byte>)
	{
		return !definitions::has_read_all_bytes_underflow_define<Input> &&
			!definitions::has_scatter_read_all_bytes_underflow_define<Input> &&
			definitions::has_read_some_bytes_underflow_define<Input>;
	}
	else
	{
		return !definitions::has_read_all_underflow_define<Input> &&
			!definitions::has_scatter_read_all_underflow_define<Input> &&
			definitions::has_read_some_underflow_define<Input>;
	}
}();

/** @brief Recognizes a typed exact read supplied entirely by scalar byte progress.
 * @details An unbuffered observer with no typed read primitive takes the
 * ordinary exact byte fallback. Its byte reader may split one code unit
 * across calls, so accumulate byte progress before publishing any element.
 */
template <bool Bytes, typename Input>
inline constexpr bool transmit_exact_typed_byte_eof =
	!Bytes && !operations::decay::defines::has_ibuffer_basic_operations<Input> &&
	!operations::decay::defines::has_any_of_read_operations<Input> &&
	operations::decay::defines::has_read_some_bytes_underflow_define<Input> &&
	!operations::decay::defines::has_read_all_bytes_underflow_define<Input> &&
	!operations::decay::defines::has_scatter_read_all_bytes_underflow_define<Input>;

/** @brief Recognizes an unbuffered scalar exact read whose EOF is owned by transmit.
 * @details Read-some can signal EOF by returning its first pointer even with
 * a non-throwing signature. The exact operation reports that short read in
 * the active checked ABI. It still invokes each provider with its own ABI.
 * Buffered providers retain their separate refill and exception selection.
 */
template <bool Bytes, typename Input>
inline constexpr bool transmit_exact_scalar_eof =
	!operations::decay::defines::has_ibuffer_basic_operations<Input> &&
	(transmit_exact_uses_scalar_read<Bytes, Input> || transmit_exact_typed_byte_eof<Bytes, Input>);

#endif

/** @brief Reads one staged range in the ordinary byte or element ABI. */
template <typename Input, typename Unit>
inline constexpr Unit *transmit_read_block(Input &in, Unit *first, Unit *last)
{
	if constexpr (::std::same_as<Unit, ::std::byte>)
		return operations::decay::read_some_bytes_decay_dispatch(in, first, last);
	else
		return operations::decay::read_some_decay_dispatch(in, first, last);
}

#if defined(__HERBCEPTIONS__)
/** @brief Reads one staged range using a declared decoder or scalar checked effect. */
template <typename Input, typename Unit>
inline constexpr Unit *transmit_read_checked_block(Input &in, Unit *first, Unit *last) throws
{
	if constexpr (checked_transmit_transcode_read<Input, Unit>)
	{
		return transcode_read_some_checked_define(in, first, last);
	}
	else if constexpr (transmit_scalar_read_throws<Input, Unit>)
	{
		using Leaf = transmit_scalar_read_unit<Input, Unit>;
		if constexpr (::std::same_as<Leaf, ::std::byte>)
			return read_some_bytes_underflow_define(in, first, last);
		else if constexpr (::std::same_as<Leaf, Unit>)
			return read_some_underflow_define(in, first, last);
		else
		{
			using LeafPtr
#if __has_cpp_attribute(__gnu__::__may_alias__)
				[[__gnu__::__may_alias__]]
#endif
				= Leaf *;
			return reinterpret_cast<Unit *>(read_some_underflow_define(in,
				reinterpret_cast<LeafPtr>(first), reinterpret_cast<LeafPtr>(last)));
		}
	}
	else
		return transmit_read_block(in, first, last);
}

/** @brief Reports a short exact transfer in the deterministic exception ABI. */
[[noreturn]]
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline void transmit_raise_checked_eof() throws
{
	::fast_io::details::raise_scan_error(::fast_io::parse_code::end_of_file);
}

/** @brief Fills a complete staged block through the detected checked input operation. */
template <bool Bytes, typename Input, typename Unit>
inline constexpr void transmit_read_exact_checked_block(Input &in, Unit *first, Unit *last) throws
{
	using Leaf = transmit_scalar_read_unit<Input, Unit>;
	if constexpr (transmit_exact_terminal_cursor<Bytes, Input>)
	{
		// A short prefix advances the source. The stage is published only on a complete read.
		if (!read_try_all_cursor(in, first, last)) [[unlikely]]
			transmit_raise_checked_eof();
	}
	else if constexpr (transmit_exact_typed_byte_eof<Bytes, Input> && !checked_transmit_transcode_read<Input, Unit>)
	{
		// Preserve the whole-element extent even when a read ends partway through it.
		transmit_read_exact_checked_block<true>(
			in, reinterpret_cast<::std::byte *>(first), reinterpret_cast<::std::byte *>(last));
	}
	else if constexpr (transmit_exact_scalar_eof<Bytes, Input> && ::std::same_as<Leaf, Unit> &&
		!checked_transmit_transcode_read<Input, Unit> && !transmit_scalar_read_throws<Input, Unit>)
	{
		// The ordinary scalar provider keeps its ABI. Only EOF is produced here.
		if (!read_try_all_unbuffered_some_impl(in, first, last)) [[unlikely]]
			transmit_raise_checked_eof();
	}
	else if constexpr (checked_transmit_transcode_read<Input, Unit> || transmit_exact_uses_scalar_read<Bytes, Input>)
	{
		while (first != last)
		{
			auto next{transmit_read_checked_block(in, first, last)};
			if (next == first) [[unlikely]]
				transmit_raise_checked_eof();
			first = next;
		}
	}
	else if constexpr (Bytes)
		operations::decay::read_all_bytes_decay_dispatch(in, first, last);
	else
		operations::decay::read_all_decay_dispatch(in, first, last);
}
#endif

/** @brief Delivers a staged range in the ordinary byte or element ABI. */
template <typename Output, typename Unit>
inline constexpr void transmit_write_block(Output &out, Unit const *first, Unit const *last)
{
	if constexpr (::std::same_as<Unit, ::std::byte>)
		operations::decay::write_all_bytes_decay_dispatch(out, first, last);
	else
		operations::decay::write_all_decay_dispatch(out, first, last);
}

#if defined(__HERBCEPTIONS__)
/** @brief Delivers a staged range using a declared encoder or scalar checked effect. */
template <typename Output, typename Unit>
inline constexpr void transmit_write_checked_block(Output &out, Unit const *first, Unit const *last) throws
{
	if constexpr (sync_transcode_output_checked<Output>)
	{
		auto &owner{transcode_output_owner(out)};
		if constexpr (::std::same_as<Unit, ::std::byte>)
		{
			using Char = typename Output::output_char_type;
			if constexpr (sizeof(Char) == 1u)
			{
				// Preserve a live one-byte put area before using the byte-overflow engine path.
				if (output_pointer_range_size(first, last) <= obuffer_remaining_size<Char, Output>(
					obuffer_curr(out), obuffer_end(out)))
				{
					operations::decay::write_all_bytes_decay_dispatch(out, first, last);
					return;
				}
			}
			owner.template write_all_bytes_overflow_impl<true>(first, last);
		}
		else
		{
			owner.write_checked(first, last);
		}
	}
	else if constexpr (transmit_scalar_write_throws<Output, Unit>)
	{
		using Leaf = transmit_scalar_write_unit<Output, Unit>;
		if constexpr (::std::same_as<Leaf, ::std::byte>)
			write_all_bytes_overflow_define(out, first, last);
		else if constexpr (::std::same_as<Leaf, Unit>)
			write_all_overflow_define(out, first, last);
		else
		{
			using LeafPtr
#if __has_cpp_attribute(__gnu__::__may_alias__)
				[[__gnu__::__may_alias__]]
#endif
				= Leaf const *;
			write_all_overflow_define(out, reinterpret_cast<LeafPtr>(first), reinterpret_cast<LeafPtr>(last));
		}
	}
	else
		transmit_write_block(out, first, last);
}
#endif

/** @brief Retains the existing byte dispatch unless the borrowed window certifies a typed write. */
template <bool Bytes, typename Output, typename Input>
inline constexpr bool transmit_window_writes_bytes = Bytes &&
	!(::std::same_as<typename Input::input_char_type, typename Output::output_char_type> &&
		transcode_output_window<::std::byte, Output>);

/** @brief Uses ordinary providers without imposing a Herbceptions or noexcept signature. */
struct transmit_native_io
{
	template <typename Input>
	static inline constexpr bool refill(Input &in, ::std::size_t count, bool &ready)
	{
		ready = ibuffer_underflow_reserve_define(in, count);
		return true;
	}

	template <typename Input, typename Unit>
	static inline constexpr bool read(Input &in, Unit *first, Unit *last, Unit *&next)
	{
		next = transmit_read_block(in, first, last);
		return true;
	}

	template <typename Output, typename Unit>
	static inline constexpr bool write(Output &out, Unit const *first, Unit const *last)
	{
		transmit_write_block(out, first, last);
		return true;
	}

	[[noreturn]] static inline void eof()
	{
		::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
	}

	[[noreturn]] static inline void protocol_error()
	{
		throw_transcode_stream_error(transcode_stream_errc::protocol_violation);
	}
};

#if defined(__HERBCEPTIONS__)
/** @brief Records deterministic failures until the shared transfer releases its local storage.
 * @details The native path above keeps ordinary C++ unwinding. A dependent
 * `throws(false)` would make that path noexcept and terminate legacy providers.
 * A legacy exception entering a checked callback retains its payload in the compiler's legacy error domain.
 */
struct transmit_checked_io : transcode_input_error_slot
{
	template <typename Input>
	inline bool refill(Input &in, ::std::size_t count, bool &ready)
	{
		if constexpr (checked_transmit_refill<Input>)
			return checked_step(ready, [&]() throws { return ibuffer_underflow_checked_define(in, count); });
		else
			return transmit_native_io::refill(in, count, ready);
	}

	template <typename Input, typename Unit>
	inline bool read(Input &in, Unit *first, Unit *last, Unit *&next)
	{
		return checked_step(next, [&]() throws { return transmit_read_checked_block(in, first, last); });
	}

	template <typename Output, typename Unit>
	inline bool write(Output &out, Unit const *first, Unit const *last)
	{
		bool completed;
		return checked_step(completed, [&]() throws {
			transmit_write_checked_block(out, first, last);
			return true;
		});
	}

	inline void eof()
	{
		bool unused;
		checked_step(unused, []() throws -> bool { transmit_raise_checked_eof(); });
	}

	inline void protocol_error()
	{
		bool unused;
		checked_step(unused, []() throws -> bool { throw throws ::std::errc::protocol_error; });
	}

	/** @brief Delivers the recorded failure after the transfer returns. */
	inline void propagate() throws
	{
		if (failure.domain != nullptr) [[unlikely]]
			throw throws failure;
	}
};
#endif

/** @brief Lends each stable input prefix to the output operation, including any outer encoder.
 * @details A decoder may publish a short verified prefix. Only the next logical refill may reclaim it.
 * The finite variant can leave both raw source bytes and transformed public characters for a later read.
 * An exact transfer only lends whole staging blocks unless this window completes the request.
 * The ordinary exact read consumes the rest of a short window before reporting EOF or refilling it.
 */
template <bool Bytes, bool UntilEof, bool Exact = false, typename Output, typename Input, typename Progress,
	typename Effects = transmit_native_io>
inline constexpr ::fast_io::uintfpos_t transmit_from_window(
	Output &out, Input &in, ::fast_io::uintfpos_t remaining, Progress progress, Effects &&effects = {})
{
	using Char = typename Input::input_char_type;
	constexpr auto batch{transmit_batch_size<Char, Output, Input>};
	while (UntilEof || remaining)
	{
		auto const wanted{UntilEof || remaining > batch ? batch : static_cast<::std::size_t>(remaining)};
		auto first{ibuffer_curr(in)};
		auto count{ibuffer_remaining_size(first, ibuffer_end(in))};
		if (count == 0u)
		{
			if constexpr (borrowed_terminal_input<Input>)
			{
				return remaining;
			}
			else
			{
				bool ready;
				if (!effects.refill(in, wanted, ready))
					return remaining;
				if (!ready)
				{
					if constexpr (Exact)
						effects.eof();
					return remaining;
				}
				first = ibuffer_curr(in);
				count = ibuffer_remaining_size(first, ibuffer_end(in));
				if (count == 0u) [[unlikely]]
				{
					effects.protocol_error();
					return remaining;
				}
			}
		}
		if constexpr (!UntilEof)
		{
			if (remaining <= count)
			{
				count = static_cast<::std::size_t>(remaining);
			}
			else if constexpr (Exact)
			{
				// Keep the last short block for the exact read, which may need another refill.
				if (count < batch)
					return remaining;
				count -= count % batch;
			}
			remaining -= count;
		}
		ibuffer_set_curr(in, first + count);
		// A certified one-byte put area accepts these same typed objects through either channel.
		bool written;
		if constexpr (transmit_window_writes_bytes<Bytes, Output, Input>)
		{
			auto const *bytes{reinterpret_cast<::std::byte const *>(first)};
			written = effects.write(out, bytes, bytes + count);
		}
		else
		{
			written = effects.write(out, first, first + count);
		}
		if (!written)
			return remaining;
		progress(count);
	}
	return remaining;
}

/** @brief Copies available input with the selected effects and optional finite limit.
 * @details A finite request stops at its bound even if the source can refill again.
 * Progress counts only prefixes that the selected output has accepted.
 */
template <bool Bytes, bool Bounded = false, typename Output, typename Input, typename Progress,
	typename Effects = transmit_native_io>
inline constexpr void transmit_copy_available(Output &out, Input &in, Progress &progress,
	::fast_io::uintfpos_t remaining = 0u, Effects &&effects = {})
{
	if constexpr (Bounded)
	{
		if (!remaining)
			return;
	}
	if constexpr (transmit_borrowable_window<Bytes, Output, Input>)
	{
		transmit_from_window<Bytes, !Bounded>(out, in, remaining,
			[&](::std::size_t count) { transmit_integer_add_define(progress, count); }, effects);
	}
	else
	{
		// Byte requests remain closed over the byte CPOs, even for wide or byte-only observers.
		using Unit = ::std::conditional_t<Bytes, ::std::byte, typename Input::input_char_type>;
		using Char = typename Output::output_char_type;
		constexpr auto batch{transmit_batch_size<Unit, Output, Input>};
		auto const capacity{Bounded && remaining < batch ? static_cast<::std::size_t>(remaining) : batch};
		transmit_batch_storage<Unit, batch> storage(capacity);
		conditional_secure_clear_guard<Unit, transcode_stream_sensitive<true, Input>() ||
			transcode_stream_sensitive<false, Output>()> erase{storage.ptr, storage.size};
		auto first{storage.ptr};
		for (;;)
		{
			auto const amount{Bounded && remaining < capacity ? static_cast<::std::size_t>(remaining) : capacity};
			auto const last{first + amount};
			Unit *next;
			if (!effects.read(in, first, last, next) || first == next)
				return;
			bool written;
			if constexpr (Bytes || ::std::same_as<Char, Unit>)
				written = effects.write(out, first, next);
			else
			{
				// Equal-width distinct types use the existing alias-safe output view.
				using CharPtr
#if __has_cpp_attribute(__gnu__::__may_alias__)
					[[__gnu__::__may_alias__]]
#endif
					= Char const *;
				written = effects.write(out, reinterpret_cast<CharPtr>(first), reinterpret_cast<CharPtr>(next));
			}
			if (!written)
				return;
			auto const count{static_cast<::std::size_t>(next - first)};
			transmit_integer_add_define(progress, count);
			if constexpr (Bounded)
			{
				remaining -= count;
				if (!remaining)
					return;
			}
		}
	}
}

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

namespace details
{

/** @brief Recognizes stable windows sharing an element type or a certified byte representation.
 * @details Each cursor pair denotes two positions in its one live array, or
 * two null pointers for an empty window. Positive extents fit ptrdiff_t.
 * The output can publish copied units by advancing only its put cursor.
 */
template <bool Bytes, typename Output, typename Input>
concept transmit_direct_terminal_window = ::fast_io::borrowed_terminal_input<Input> &&
	((Bytes && ::fast_io::transcode_input_window<::std::byte, Input> &&
	  ::fast_io::transcode_output_window<::std::byte, Output>) ||
	 (!Bytes && ::std::same_as<typename Input::input_char_type, typename Output::output_char_type>)) &&
	::fast_io::direct_obuffer_copy_safe<typename Output::output_char_type, Output> &&
	::fast_io::deferred_obuffer_commit_safe<typename Output::output_char_type, Output> &&
	requires(Output &out, typename Output::output_char_type *position) {
		{ obuffer_curr(out) } noexcept -> ::std::same_as<typename Output::output_char_type *>;
		{ obuffer_end(out) } noexcept -> ::std::same_as<typename Output::output_char_type *>;
		{ obuffer_set_curr(out, position) } noexcept -> ::std::same_as<void>;
	};

/** @brief Re-enters the ordinary transfer path after a terminal-window miss.
 * @details Both observers still have their original cursor state. The fallback
 * keeps the staging, progress, and errors of the ordinary operation.
 * On Clang, outlining this edge lets the small window preflight inline
 * without importing the staging loop. Generic device routes call that loop
 * directly and retain their ordinary inlining decisions.
 */
template <auto Operation, typename... Args>
#if defined(__clang__) && __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr decltype(auto) transmit_terminal_window_miss(Args &...args)
{
	return Operation(args...);
}

#if defined(__HERBCEPTIONS__)
/** @brief Outlines the same window miss when the chosen fallback uses the checked ABI. */
template <auto Operation, typename... Args>
	requires (throws((Operation(::std::declval<Args &>()...))))
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr decltype(auto) transmit_terminal_window_miss(Args &...args) throws
{
	return Operation(args...);
}
#endif

/** @brief Moves a terminal window larger than one staging block.
 * @details With rightward overlap, each write can change the next unread
 * block. Separate memmoves preserve that order without a temporary buffer.
 * All other layouts allow a single memmove. Keep the loop out of small-copy
 * callers so its registers and branches do not enter their hot path.
 */
template <typename OutputChar, typename InputChar>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline void transmit_move_large_window(
	OutputChar *destination, InputChar const *source, ::std::size_t count, ::std::size_t block) noexcept
{
	static_assert(sizeof(OutputChar) == sizeof(InputChar));
	::std::ranges::less address_less;
	if (address_less(static_cast<void const *>(source), static_cast<void const *>(destination)) &&
		address_less(static_cast<void const *>(destination), static_cast<void const *>(source + count)))
	{
		do
		{
			::fast_io::freestanding::my_memmove(destination, source, block * sizeof(InputChar));
			destination += block;
			source += block;
			count -= block;
		} while (count > block);
	}
	::fast_io::freestanding::my_memmove(destination, source, count * sizeof(InputChar));
}

/** @brief Selects completion, ordinary transfer, or a proved first-block EOF. */
enum class transmit_terminal_window_result : unsigned char
{
	complete,
	staging,
	eof
};

/** @brief Declares the block move supplied by the target-aware SIMD implementation. */
template <bool Exact, typename OutputChar, typename InputChar>
FAST_IO_GNU_ALWAYS_INLINE inline void move_small_contiguous_block(
	OutputChar *destination, InputChar const *source, ::std::size_t count) noexcept;

/** @brief Moves the requested terminal prefix into a single put area and advances both cursors.
 * @details On a staging result both cursors retain their original state.
 * For an exact read proved to run out before its first staging block is
 * full, the same prefix is consumed as in ordinary exact read and no
 * output block can have been published. Reporting EOF there avoids
 * allocating a stage that could never be written. A later EOF retains
 * ordinary staging so all prior complete blocks can be delivered.
 */
template <bool Bytes, bool UntilEof = true, bool Exact = false, typename Output, typename Input>
	requires transmit_direct_terminal_window<Bytes, Output, Input>
inline constexpr transmit_terminal_window_result transmit_try_direct_terminal_window(
	Output &out, Input &in, ::fast_io::uintfpos_t &transmitted, ::fast_io::uintfpos_t limit = 0u)
{
	using Char = typename Input::input_char_type;
	using Result = transmit_terminal_window_result;
	constexpr auto block{::fast_io::details::transmit_batch_size<Char, Output, Input>};
	auto const *first{ibuffer_curr(in)}, *end{ibuffer_end(in)};
	// Both cursors address one array, or both are null. A reversed live extent has no readable prefix.
	auto const available{end - first};
	if (available <= 0)
	{
		if constexpr (Exact)
		{
			if (limit)
			{
				return transmit_exact_terminal_cursor<Bytes, Input> ? Result::eof : Result::staging;
			}
		}
		return Result::complete;
	}
	auto count{static_cast<::std::size_t>(available)};
	if constexpr (!UntilEof)
	{
		if constexpr (Exact)
		{
			if (count < limit)
			{
				if constexpr (transmit_exact_terminal_cursor<Bytes, Input>)
				{
					if (count < block)
					{
						// The first block cannot be completed. Only the source consumes a prefix.
						ibuffer_set_curr(in, end);
						return Result::eof;
					}
				}
				return Result::staging;
			}
			count = static_cast<::std::size_t>(limit);
		}
		else if (limit < count)
		{
			count = static_cast<::std::size_t>(limit);
		}
	}

	if (!count)
	{
		return Result::complete;
	}

	auto *destination{obuffer_curr(out)}, *destination_end{obuffer_end(out)};
	// The selected count cannot exceed the nonnegative source ptrdiff_t.
	if (::std::is_constant_evaluated() ||
		destination_end - destination < static_cast<::std::ptrdiff_t>(count))
	{
		return Result::staging;
	}

	auto const *last{first + count};
	if (count > block) [[unlikely]]
	{
		transmit_move_large_window(destination, first, count, block);
	}
	else
	{
		::fast_io::details::move_small_contiguous_block<Exact>(destination, first, count);
	}
	ibuffer_set_curr(in, last);
	obuffer_set_curr(out, destination + count);
	transmit_integer_add_define(::fast_io::uintfpos_transmit_reference_wrapper{__builtin_addressof(transmitted)}, count);
	return Result::complete;
}

} // namespace details

struct transmit_result
{
	::fast_io::uintfpos_t transmitted{};
	inline constexpr bool is_overflowed() noexcept
	{
		constexpr auto mxval{::std::numeric_limits<::fast_io::uintfpos_t>::max()};
		return transmitted == mxval;
	}
};

namespace details
{

/** @brief Identifies the provider that owns a complete standard-result EOF transfer. */
template <bool Bytes, typename Output, typename Input>
inline constexpr bool has_transmit_eof_status = [] {
	if constexpr (Bytes)
		return requires(Output &out, Input &in) { requires ::std::same_as<decltype(status_transmit_bytes_until_eof_define(out, in)), transmit_result>; };
	else
		return requires(Output &out, Input &in) { requires ::std::same_as<decltype(status_transmit_until_eof_define(out, in)), transmit_result>; };
}();

#if defined(__HERBCEPTIONS__)
/** @brief Reads the effect of the existing writer in its selected representation. */
template <typename Output, typename Unit>
inline constexpr bool transmit_block_write_throws = [] {
	if constexpr (sync_transcode_output_checked<Output>)
	{
		using Owner = decltype(transcode_output_owner(::std::declval<Output &>()));
		if constexpr (::std::same_as<Unit, ::std::byte>)
			return throws((::std::declval<Owner>().template write_all_bytes_overflow_impl<true>(
				static_cast<Unit const *>(nullptr), static_cast<Unit const *>(nullptr))));
		else
			return throws((::std::declval<Owner>().write_checked(
				static_cast<Unit const *>(nullptr), static_cast<Unit const *>(nullptr))));
	}
	else return transmit_scalar_write_throws<Output, Unit>;
}();

/** @brief Selects the writer's domain after accounting for a borrowed one-byte get window. */
template <bool Bytes, typename Output, typename Input>
inline constexpr bool transmit_window_write_throws = [] {
	using Char = typename Output::output_char_type;
	using Unit = ::std::conditional_t<Bytes && (!transmit_borrowable_window<Bytes, Output, Input> ||
		transmit_window_writes_bytes<Bytes, Output, Input>), ::std::byte, Char>;
	return transmit_block_write_throws<Output, Unit>;
}();

/** @brief Resolves checked work using the same unlocked endpoint types as the ordinary operation. */
template <bool Bytes, bool PreferStatus, typename Output, typename Input>
inline constexpr bool uses_checked_transmit_eof_loop = [] {
	using Source = ::std::remove_reference_t<decltype(operations::input_stream_ref(::std::declval<Input &>()))>;
	using Sink = ::std::remove_reference_t<decltype(operations::output_stream_ref(::std::declval<Output &>()))>;
	if constexpr (!operations::decay::defines::has_complete_transmit_mutex_protocols<Sink, Source>)
		return false;
	else
	{
		using Out = transcode_endpoint_ref_t<false, Sink>;
		using In = transcode_endpoint_ref_t<true, Source>;
		using Unit = ::std::conditional_t<Bytes, ::std::byte, typename In::input_char_type>;
		if constexpr (PreferStatus && has_transmit_eof_status<Bytes, Out, In>) return false;
		else return transmit_window_write_throws<Bytes, Out, In> || checked_transmit_window<Bytes, Out, In> ||
			(!transmit_borrowable_window<Bytes, Out, In> &&
			 (checked_transmit_transcode_read<In, Unit> || transmit_scalar_read_throws<In, Unit>));
	}
}();

/** @brief A successful temporary stock encoder is eligible for native terminal validation. */
template <typename Output>
inline constexpr bool uses_checked_transmit_finish = [] {
	if constexpr (::std::is_lvalue_reference_v<Output> || !temporary_output_finish_enabled<::std::remove_cvref_t<Output>>)
		return false;
	else
	{
		using Ref = ::std::remove_cvref_t<decltype(operations::output_stream_ref(::std::declval<Output &>()))>;
		if constexpr (sync_transcode_output_checked<Ref>)
			return throws((transcode_output_owner(::std::declval<Ref &>()).template finish_impl<true>()));
		else return false;
	}
}();

template <bool Bytes, bool PreferStatus, typename Output, typename Input>
inline constexpr bool uses_checked_transmit_eof = uses_checked_transmit_eof_loop<Bytes, PreferStatus, Output, Input> ||
	uses_checked_transmit_finish<Output>;

/** @brief Resolves effects from both branches of the exact-transfer schedule.
 * @details A short borrowed window can continue through a staged read. Either path
 * can encounter a declared engine failure, and a temporary output can fail during
 * final validation after the last complete block.
 */
template <bool Bytes, typename Output, typename Input>
inline constexpr bool uses_checked_transmit_all = [] {
	using Source = ::std::remove_reference_t<decltype(operations::input_stream_ref(::std::declval<Input &>()))>;
	using Sink = ::std::remove_reference_t<decltype(operations::output_stream_ref(::std::declval<Output &>()))>;
	if constexpr (!operations::decay::defines::has_complete_transmit_mutex_protocols<Sink, Source>)
		return false;
	else
	{
		using In = transcode_endpoint_ref_t<true, Source>;
		using Out = transcode_endpoint_ref_t<false, Sink>;
		using Read = ::std::conditional_t<Bytes, ::std::byte, typename In::input_char_type>;
		using Write = ::std::conditional_t<Bytes, ::std::byte, typename Out::output_char_type>;
		return transmit_exact_terminal_cursor<Bytes, In> || checked_transmit_window<Bytes, Out, In> ||
			checked_transmit_transcode_read<In, Read> || transmit_exact_scalar_eof<Bytes, In> ||
			transmit_window_write_throws<Bytes, Out, In> || transmit_block_write_throws<Out, Write> ||
			uses_checked_transmit_finish<Output>;
	}
}();

/** @brief Keeps the operation argument and first failure across observer transport. */
template <typename Argument>
struct checked_transmit_state
{
	Argument &argument;
	transcode_input_error_slot error;
};

/** @brief Executes both selected effects while both directional locks are held. */
template <auto Copy, typename Output, typename Input, typename Argument>
inline void transmit_checked_borrowed(Output &out, Input &in, checked_transmit_state<Argument> &state)
{
	with_transcode_endpoint<false>(out, [&](auto &sink) {
		with_transcode_endpoint<true>(in, [&](auto &source) {
			bool completed;
			state.error.checked_step(completed, [&]() throws {
				Copy(sink, source, state.argument);
				return true;
			});
		});
	});
}

/** @brief Reports the checked failure after both locks are released. */
template <auto Copy, typename Output, typename Input, typename Argument>
inline constexpr void transmit_checked(Output &out, Input &in, Argument &argument) throws
{
	namespace definitions = operations::decay::defines;
	if constexpr (::fast_io::borrowed_terminal_input<Input> &&
		!definitions::has_output_or_io_stream_mutex_ref_define<Output> &&
		!definitions::has_input_or_io_stream_mutex_ref_define<Input>)
	{
		// The normalized observers already denote the target cursors. There are no locks to release.
		Copy(out, in, argument);
	}
	else
	{
		checked_transmit_state<Argument> state{argument, {}};
		operations::decay::transmit_stream_pair_accumulator_dispatch<
			&transmit_checked_borrowed<Copy, Output, Input, Argument>>(out, in, state);
		if (state.error.failure.domain != nullptr) [[unlikely]]
			throw throws state.error.failure;
	}
}

template <bool Bytes, typename Output, typename Input, typename Progress>
	requires transmit_integer_wrapper<::std::remove_cvref_t<Progress>>
inline void transmit_checked_eof(Output &out, Input &in, Progress &progress) throws
{
	transmit_checked<[](auto &sink, auto &source, auto &count) throws {
		transmit_checked_io effects;
		transmit_copy_available<Bytes>(sink, source, count, 0u, effects);
		effects.propagate();
	}>(out, in, progress);
}

/** @brief Checks a partial transfer using the same endpoint capabilities as an unbounded copy. */
template <bool Bytes, typename Output, typename Input>
inline constexpr bool uses_checked_transmit_some = uses_checked_transmit_eof<Bytes, false, Output, Input>;

/** @brief Carries the finite byte or element bound and committed progress under the endpoint locks. */
struct checked_transmit_some_progress
{
	::fast_io::uintfpos_t limit;
	::fast_io::uintfpos_t transmitted{};
};

/** @brief Runs a bounded checked transfer without draining its source past the requested count. */
template <bool Bytes, typename Output, typename Input>
	requires (Bytes || sizeof(typename Output::output_char_type) == sizeof(typename Input::input_char_type))
inline constexpr ::fast_io::uintfpos_t transmit_checked_some(Output &out, Input &in, ::fast_io::uintfpos_t count) throws
{
	checked_transmit_some_progress state{count};
	transmit_checked<[](auto &sink, auto &source, auto &request) throws {
		uintfpos_transmit_reference_wrapper progress{__builtin_addressof(request.transmitted)};
		transmit_checked_io effects;
		transmit_copy_available<Bytes, true>(sink, source, progress, request.limit, effects);
		effects.propagate();
	}>(out, in, state);
	return state.transmitted;
}

/** @brief Uses the checked engine drain only at the existing successful terminal boundary. */
template <typename Output, typename Guard>
inline void finish_checked_transmit(Guard &guard) throws
{
	if constexpr (uses_checked_transmit_finish<Output>)
		transcode_output_owner(guard.ref()).template finish_impl<true>();
	else
		guard.commit();
}
#endif

} // namespace details


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
