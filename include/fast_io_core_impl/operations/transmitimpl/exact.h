#pragma once

namespace fast_io
{
class c_malloc_allocator;
}

namespace fast_io::details
{

/** @brief Recognizes exact transfers that can preflight one terminal window. */
template <bool Bytes, typename Output, typename Input>
concept transmit_exact_direct_window =
	transmit_direct_terminal_window<Bytes, Output, Input> && !transmit_borrowable_window<Bytes, Output, Input>;

/** @brief Completes one staged read and publishes it in the selected output representation.
 * @details The callback controls whether the staged read and write completed.
 * For typed transfers between different code-unit types, the entry point proves equal width.
 */
template <bool Bytes, bool Compact = false, typename Output, typename Input, typename Unit, typename Transfer>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool transmit_exact_stage_block(
	Output &out, Input &in, Unit *first, ::std::size_t amount, Transfer &transfer)
{
	using Char = typename Output::output_char_type;
	auto *last{first + amount};
	if constexpr (Bytes || ::std::same_as<Char, Unit>)
		return transfer(::std::bool_constant<Compact>{}, out, in, first, last, first, last);
	else
	{
		using OutputPointer
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= Char const *;
		return transfer(::std::bool_constant<Compact>{}, out, in, first, last,
			reinterpret_cast<OutputPointer>(first), reinterpret_cast<OutputPointer>(last));
	}
}

/** @brief Uses a fixed automatic stage for an exact request known to fit in one small block.
 * @details Keep the frame on this path so longer requests and terminal-window hits
 * do not enter it. The read still fills a private array before the write. A checked
 * failure returns through the callback, and sensitive bytes are erased on either exit.
 */
template <bool Bytes, typename Unit, ::std::size_t Capacity, typename Output, typename Input, typename Transfer>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr void transmit_exact_stack_block(
	Output &out, Input &in, ::std::size_t amount, Transfer &transfer)
{
	// Align the larger stage for both bulk copies. The short frame keeps its native alignment.
	constexpr auto alignment{Capacity * sizeof(Unit) > 256u && alignof(Unit) < 32u ? 32u : alignof(Unit)};
	alignas(alignment) Unit scratch[Capacity];
	conditional_secure_clear_guard<Unit, transcode_stream_sensitive<true, Input>() ||
		transcode_stream_sensitive<false, Output>()> erase{scratch, amount};
	(void)transmit_exact_stage_block<Bytes, Capacity * sizeof(Unit) <= 256u>(out, in, scratch, amount, transfer);
}

/** @brief Executes the bounded staging schedule for an exact transfer.
 * @details The request and allocation are in the selected domain, bytes or
 * input elements. A block callback first completes the read into the stage
 * and only then publishes it. A checked failure stops the schedule while
 * the stage's lifetime and sensitive-storage cleanup are still in scope.
 * A request that fits one stage can use automatic storage within the stack
 * policy and a 4 KiB ceiling. A separate short frame keeps small copies
 * compact. Terminal sources already preflight their resident window.
 * Their misses and explicit custom allocator calls use the ordinary schedule.
 */
template <bool Bytes, typename Output, typename Input, typename Transfer,
	typename StackPolicy = ::fast_io::details::default_print_stack_policy>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void transmit_exact_staged_loop(
	Output &out, Input &in, ::fast_io::uintfpos_t count, Transfer transfer)
{
	using Unit = ::std::conditional_t<Bytes, ::std::byte, typename Input::input_char_type>;
	constexpr auto maximum{transmit_batch_size<Unit, Output, Input>};
	constexpr auto budget{print_stack_buffer_max_bytes<StackPolicy>()};
	constexpr auto units{(budget < 4096u ? budget : 4096u) / sizeof(Unit)};
	constexpr auto stack_capacity{units < maximum ? units : maximum};
	constexpr auto short_units{256u / sizeof(Unit)};
	constexpr auto short_capacity{short_units < stack_capacity ? short_units : stack_capacity};
	using DefaultCAllocator = ::fast_io::generic_allocator_adapter<
		::fast_io::generic_allocator_adapter<::fast_io::c_malloc_allocator>>;
	if constexpr (!borrowed_terminal_input<Input> && stack_capacity != 0u &&
		::std::same_as<::fast_io::native_thread_local_allocator, DefaultCAllocator>)
	{
		if (count <= short_capacity)
		{
			return transmit_exact_stack_block<Bytes, Unit, short_capacity>(
				out, in, static_cast<::std::size_t>(count), transfer);
		}
		if constexpr (short_capacity < stack_capacity)
		{
			if (count <= stack_capacity)
			{
				return transmit_exact_stack_block<Bytes, Unit, stack_capacity>(
					out, in, static_cast<::std::size_t>(count), transfer);
			}
		}
	}
	// The comparison proves the request fits size_t before it is narrowed.
	auto const capacity{count < maximum ? static_cast<::std::size_t>(count) : maximum};
	transmit_batch_storage<Unit, maximum> storage(capacity);
	conditional_secure_clear_guard<Unit, transcode_stream_sensitive<true, Input>() ||
		transcode_stream_sensitive<false, Output>()> erase{storage.ptr, storage.size};
	auto *first{storage.ptr};
	while (count != 0u)
	{
		auto const amount{count < capacity ? static_cast<::std::size_t>(count) : capacity};
		if (!transmit_exact_stage_block<Bytes>(out, in, first, amount, transfer)) [[unlikely]]
			return;
		count -= amount;
	}
}

/** @brief Selects the existing scalar exact-read leaf for an unbuffered stage.
 * @details A native all or scatter-all CPO takes precedence. A get area or
 * mutex requires the regular dispatch, and the target's direct byte ABI
 * remains in use for observers that satisfy it.
 */
template <bool Bytes, typename Input>
inline constexpr bool transmit_exact_inline_read_some = [] {
	namespace definitions = operations::decay::defines;
	if constexpr (definitions::has_ibuffer_basic_operations<Input> ||
		definitions::has_input_or_io_stream_mutex_ref_define<Input>)
		return false;
	else if constexpr (Bytes)
		return definitions::has_read_some_bytes_underflow_define<Input> &&
			!definitions::has_read_all_bytes_underflow_define<Input> &&
			!definitions::has_scatter_read_all_bytes_underflow_define<Input> &&
			!::fast_io::details::abi_value_direct_read_some_bytes<Input>;
	else
		return definitions::has_read_some_underflow_define<Input> &&
			!definitions::has_read_all_underflow_define<Input> &&
			!definitions::has_scatter_read_all_underflow_define<Input>;
}();

/** @brief Finishes the unread suffix of a compact stage after a partial scalar read. */
template <typename Input, typename Unit>
#if __has_cpp_attribute(__gnu__::__noinline__) && __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__noinline__, __gnu__::__cold__]]
#endif
inline constexpr bool transmit_exact_read_remainder(Input &in, Unit *next, Unit *last)
{
	return ::fast_io::details::read_try_all_unbuffered_some_impl(in, next, last);
}

/** @brief Completes an unbuffered stage using its selected scalar CPO.
 * @details In a compact frame, the first read can complete without
 * importing the retry loop. A partial read resumes at its returned
 * pointer and an unchanged pointer reports EOF to the caller.
 */
template <bool Compact, typename Input, typename Unit>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool transmit_try_read_scalar_stage(
	Input &in, Unit *first, Unit *last)
{
	if constexpr (Compact)
	{
		auto *next{::fast_io::details::read_unbuffered_some_impl(in, first, last)};
		return next == last || (next != first && transmit_exact_read_remainder(in, next, last));
	}
	else
	{
		return ::fast_io::details::read_try_all_unbuffered_some_impl(in, first, last);
	}
}

/** @brief Publishes a complete stage through the selected native writer.
 * @details A stable put area can use the existing primitive directly.
 * The normalized, unlocked observer retains all capacity and overflow checks.
 */
template <bool Bytes, typename Output, typename Write>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void transmit_exact_write_native(
	Output &out, Write const *first, Write const *last)
{
	using Char = typename Output::output_char_type;
	constexpr bool fixed_put_area{::fast_io::direct_obuffer_copy_safe<Char, Output> &&
		::fast_io::deferred_obuffer_commit_safe<Char, Output> &&
		operations::decay::defines::has_obuffer_basic_operations<Output> &&
		(!Bytes || sizeof(Char) == 1u)};
	if constexpr (Bytes)
	{
		if constexpr (fixed_put_area)
		{
#if __has_cpp_attribute(clang::always_inline)
			[[clang::always_inline]]
#endif
			::fast_io::details::write_all_bytes_impl(out, first, last);
		}
		else
			operations::decay::write_all_bytes_decay_dispatch(out, first, last);
	}
	else
	{
		if constexpr (fixed_put_area)
		{
#if __has_cpp_attribute(clang::always_inline)
			[[clang::always_inline]]
#endif
			::fast_io::details::write_all_impl(out, first, last);
		}
		else
			operations::decay::write_all_decay_dispatch(out, first, last);
	}
}

/** @brief Reads and publishes one block using the ordinary endpoint ABIs. */
template <bool Bytes>
struct transmit_exact_native_block
{
	template <bool Compact, typename Output, typename Input, typename Read, typename Write>
	FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool operator()(
		::std::bool_constant<Compact>, Output &out, Input &in, Read *first, Read *last,
		Write const *write_first, Write const *write_last) const
	{
		if constexpr (transmit_exact_inline_read_some<Bytes, Input>)
		{
			if (!transmit_try_read_scalar_stage<Compact>(in, first, last)) [[unlikely]]
				::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
		}
		else if constexpr (Bytes)
			operations::decay::read_all_bytes_decay_dispatch(in, first, last);
		else
			operations::decay::read_all_decay_dispatch(in, first, last);
		::fast_io::details::transmit_exact_write_native<Bytes>(out, write_first, write_last);
		return true;
	}
};

/** @brief Uses the ordinary sequential read and write ABIs for one exact staged transfer. */
template <bool Bytes, typename Output, typename Input>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void transmit_exact_staged(Output &out, Input &in, ::fast_io::uintfpos_t count)
{
	transmit_exact_staged_loop<Bytes>(out, in, count, transmit_exact_native_block<Bytes>{});
}

/** @brief Preflights an exact request after the observers are normalized and unlocked. */
template <bool Bytes, typename Output, typename Input>
inline constexpr transmit_terminal_window_result transmit_exact_preflight(
	Output &out, Input &in, ::fast_io::uintfpos_t count)
{
	if (count == 0u)
		return transmit_terminal_window_result::complete;
	if constexpr (transmit_exact_direct_window<Bytes, Output, Input>)
	{
		::fast_io::uintfpos_t transferred{};
		return transmit_try_direct_terminal_window<Bytes, false, true>(out, in, transferred, count);
	}
	return transmit_terminal_window_result::staging;
}

/**
 * @brief Continues an exact window miss with the existing ABI transport for each observer.
 * @details The public dispatch proves value transport separately in each direction. A copied
 * proxy still publishes to the same underlying cursor. Identity-sensitive observers remain
 * references. The requested count is a value and the continuation never updates the caller's copy.
 */
template <auto Operation, typename Output, typename Input>
#if defined(__clang__) && __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void transmit_exact_window_miss(Output out, Input in, ::fast_io::uintfpos_t count)
{
	Operation(out, in, count);
}

/**
 * @brief Completes the block schedule of an exact transfer after terminal preflight misses.
 * @details Let B be the ordinary staging capacity. If the source is short, exactly
 * floor(readable/B) full blocks can reach the writer. Otherwise the complete prefix
 * is the original request, including a possible final partial block. A destination
 * shorter than that prefix can publish only floor(writable/B) full blocks before
 * overflow terminates. The overlapping move preserves the same read-before-write
 * ordering at every block boundary. Its scalar ABI does not borrow either observer.
 */
template <typename Out, typename In>
#if __has_cpp_attribute(__gnu__::__noinline__) && __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__noinline__, __gnu__::__cold__]]
#endif
inline ::std::size_t transmit_fixed_window_miss(Out *destination, ::std::ptrdiff_t writable,
	In const *source, ::std::size_t readable, ::fast_io::uintfpos_t exact, ::std::size_t block) noexcept
{
	auto const complete{readable < exact ? readable - readable % block : static_cast<::std::size_t>(exact)};
	auto const capacity{writable < 0 ? 0u : static_cast<::std::size_t>(writable)};
	if (complete > capacity) [[unlikely]]
	{
		auto const prefix{capacity - capacity % block};
		if (prefix != 0u)
			transmit_move_large_window(destination, source, prefix, block);
		::fast_io::fast_terminate();
	}
	if (complete != 0u)
		transmit_move_large_window(destination, source, complete, block);
	return complete;
}

/**
 * @brief Publishes the source and destination positions after a terminal-window miss.
 * @details The terminal input has no refill effects. Its final incomplete block is
 * consumed, and the put cursor reflects only the complete blocks. The caller raises
 * EOF through its own exception ABI once both positions are set.
 */
template <bool Bytes, typename Output, typename Input>
	requires (::fast_io::terminal_put_area<Output> && transmit_exact_terminal_cursor<Bytes, Input>)
FAST_IO_GNU_ALWAYS_INLINE inline bool transmit_finish_fixed_window_miss(
	Output &out, Input &in, ::fast_io::uintfpos_t count) noexcept
{
	auto const *source{ibuffer_curr(in)}, *end{ibuffer_end(in)};
	auto const reading{end - source};
	auto *destination{obuffer_curr(out)};
	auto const writable{obuffer_end(out) - destination};
	auto const readable{reading <= 0 ? 0u : static_cast<::std::size_t>(reading)};
	constexpr auto block{transmit_batch_size<typename Input::input_char_type, Output, Input>};
	auto const copied{transmit_fixed_window_miss(destination, writable, source, readable, count, block)};
	if (readable < count)
	{
		ibuffer_set_curr(in, end);
		if (copied != 0u)
			obuffer_set_curr(out, destination + copied);
		return false;
	}
	ibuffer_set_curr(in, source + copied);
	obuffer_set_curr(out, destination + copied);
	return true;
}

/** @brief Executes an exact transfer using the ordinary exception ABI.
 * @details In dual-enabled builds, a legacy endpoint's ordinary C++ exception
 * can still unwind through this function and its staging storage.
 */
template <bool Bytes, typename Output, typename Input>
	requires (Bytes || sizeof(typename Output::output_char_type) == sizeof(typename Input::input_char_type))
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void transmit_exact_main_impl(Output &out, Input &in, ::fast_io::uintfpos_t count)
{
	auto const status{transmit_exact_preflight<Bytes>(out, in, count)};
	if (status == transmit_terminal_window_result::complete) [[likely]]
		return;
	if constexpr (transmit_exact_direct_window<Bytes, Output, Input>)
	{
		if constexpr (transmit_exact_terminal_cursor<Bytes, Input>)
		{
			if (status == transmit_terminal_window_result::eof) [[unlikely]]
				::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
			if constexpr (::fast_io::terminal_put_area<Output>)
			{
				if (!::std::is_constant_evaluated())
				{
					if (!transmit_finish_fixed_window_miss<Bytes>(out, in, count))
						::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
					return;
				}
			}
		}
		using Out = ::fast_io::operations::decay::defines::transmit_stream_transport_parameter<
			::fast_io::operations::defines::abi_value_output_stream_ref_result<Output &>, Output>;
		using In = ::fast_io::operations::decay::defines::transmit_stream_transport_parameter<
			::fast_io::operations::defines::abi_value_input_stream_ref_result<Input &>, Input>;
		return transmit_exact_window_miss<transmit_exact_staged<Bytes, Output, Input>, Out, In>(out, in, count);
	}
	else
	{
		if constexpr (transmit_borrowable_window<Bytes, Output, Input>)
		{
			count = transmit_from_window<Bytes, false, true>(out, in, count, [](::std::size_t) {});
			if (count == 0u)
				return;
		}
		transmit_exact_staged<Bytes>(out, in, count);
	}
}

#if defined(__HERBCEPTIONS__)
/** @brief Gives an ordinary scalar transfer a checked EOF result.
 * @details Selection proves which scalar reader fills the private stage
 * and that neither endpoint requires a checked-provider call. Ordinary
 * C++ unwinding still follows the endpoint signatures. A short read
 * stops before publishing this block, then the checked EOF is produced
 * after the stage lifetime and sensitive-storage cleanup have ended.
 */
template <bool Bytes, typename Leaf, typename Output, typename Input>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void transmit_exact_checked_native_staged(
	Output &out, Input &in, ::fast_io::uintfpos_t count) throws
{
	bool complete{true};
	transmit_exact_staged_loop<Bytes>(out, in, count,
		[&](auto compact, auto &sink, auto &source, auto *first, auto *last,
			auto const *write_first, auto const *write_last) {
			constexpr bool short_stage{decltype(compact)::value};
			if constexpr (::std::same_as<Leaf, ::std::remove_pointer_t<decltype(first)>>)
			{
				complete = transmit_try_read_scalar_stage<short_stage>(source, first, last);
			}
			else
			{
				// The selected byte/character bridge views the private stage in the provider's representation.
				using Pointer
#if __has_cpp_attribute(__gnu__::__may_alias__)
					[[__gnu__::__may_alias__]]
#endif
					= Leaf *;
				complete = transmit_try_read_scalar_stage<short_stage>(
					source, reinterpret_cast<Pointer>(first), reinterpret_cast<Pointer>(last));
			}
			if (complete) [[likely]]
				::fast_io::details::transmit_exact_write_native<Bytes>(sink, write_first, write_last);
			return complete;
		});
	if (!complete) [[unlikely]]
		transmit_raise_checked_eof();
}

/** @brief Keeps a checked staged failure until the stage has been cleaned up.
 * @details The scheduling decisions stay visible to the caller. The stack
 * array itself remains in the outlined stage so only that path owns its frame.
 */
template <bool Bytes, typename Output, typename Input>
FAST_IO_GNU_ALWAYS_INLINE inline void transmit_exact_checked_staged(
	Output &out, Input &in, ::fast_io::uintfpos_t count) throws
{
	using Read = ::std::conditional_t<Bytes, ::std::byte, typename Input::input_char_type>;
	using Write = ::std::conditional_t<Bytes, ::std::byte, typename Output::output_char_type>;
	using Leaf = ::std::conditional_t<transmit_exact_typed_byte_eof<Bytes, Input>,
		::std::byte, transmit_scalar_read_unit<Input, Read>>;
	if constexpr (transmit_exact_scalar_eof<Bytes, Input> &&
		!checked_transmit_transcode_read<Input, Read> && !checked_transmit_transcode_read<Input, Leaf> &&
		!transmit_scalar_read_throws<Input, Leaf> &&
		!sync_transcode_output_checked<Output> && !transmit_block_write_throws<Output, Write>)
	{
		::fast_io::details::transmit_exact_checked_native_staged<Bytes, Leaf>(out, in, count);
	}
	else
	{
		transcode_input_error_slot result;
		transmit_exact_staged_loop<Bytes>(out, in, count,
			[&](auto, auto &sink, auto &source, auto *first, auto *last,
				auto const *write_first, auto const *write_last) {
				bool complete;
				return result.checked_step(complete, [&]() throws {
					transmit_read_exact_checked_block<Bytes>(source, first, last);
					transmit_write_checked_block(sink, write_first, write_last);
					return true;
				});
			});
		if (result.failure.domain != nullptr) [[unlikely]]
			throw throws result.failure;
	}
}

/** @brief Executes the same exact-transfer schedule with native checked failures. */
template <bool Bytes, typename Output, typename Input>
	requires (Bytes || sizeof(typename Output::output_char_type) == sizeof(typename Input::input_char_type))
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void transmit_exact_checked_impl(
	Output &out, Input &in, ::fast_io::uintfpos_t count) throws
{
	auto const status{transmit_exact_preflight<Bytes>(out, in, count)};
	if (status == transmit_terminal_window_result::complete) [[likely]]
		return;
	if constexpr (transmit_exact_direct_window<Bytes, Output, Input>)
	{
		if constexpr (transmit_exact_terminal_cursor<Bytes, Input>)
		{
			if (status == transmit_terminal_window_result::eof) [[unlikely]]
				transmit_raise_checked_eof();
			if constexpr (::fast_io::terminal_put_area<Output>)
			{
				if (!::std::is_constant_evaluated())
				{
					if (!transmit_finish_fixed_window_miss<Bytes>(out, in, count))
						transmit_raise_checked_eof();
					return;
				}
			}
		}
		return transmit_terminal_window_miss<transmit_exact_checked_staged<Bytes, Output, Input>>(out, in, count);
	}
	else
	{
		if constexpr (transmit_borrowable_window<Bytes, Output, Input>)
		{
			transmit_checked_io effects;
			count = transmit_from_window<Bytes, false, true>(out, in, count, [](::std::size_t) {}, effects);
			effects.propagate();
			if (count == 0u)
				return;
		}
		transmit_exact_checked_staged<Bytes>(out, in, count);
	}
}

/** @brief Enters the exact checked schedule with the required synchronization.
 * @details Normalized endpoints without directional mutexes already expose
 * their final observer types. A direct checked call can report its failure
 * immediately. The locked path keeps the scoped error slot so the locks are
 * released before propagating an error.
 */
template <bool Bytes, typename Output, typename Input>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void transmit_exact_checked_dispatch(
	Output &out, Input &in, ::fast_io::uintfpos_t count) throws
{
	namespace definitions = operations::decay::defines;
	if constexpr (definitions::has_output_or_io_stream_mutex_ref_define<Output> ||
		definitions::has_input_or_io_stream_mutex_ref_define<Input>)
	{
		::fast_io::details::transmit_checked<[](auto &sink, auto &source, auto &limit) throws {
			::fast_io::details::transmit_exact_checked_impl<Bytes>(sink, source, limit);
		}>(out, in, count);
	}
	else
	{
		::fast_io::details::transmit_exact_checked_impl<Bytes>(out, in, count);
	}
}
#endif

} // namespace fast_io::details
