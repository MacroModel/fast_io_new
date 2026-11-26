#pragma once

namespace fast_io
{

namespace details::io_buffer
{

template <typename allocator_type, ::std::integral char_type, typename instmtype>
inline constexpr bool ibuffer_underflow_rl_size_impl(instmtype &insm, basic_io_buffer_pointers<char_type> &ibuffer,
												 ::std::size_t bfsz)
{
	using typed_allocator_type = ::fast_io::typed_generic_allocator_adapter<allocator_type, char_type>;
	if (ibuffer.buffer_begin == nullptr)
	{
		ibuffer.buffer_end = ibuffer.buffer_curr = ibuffer.buffer_begin = typed_allocator_type::allocate(bfsz);
	}
	auto const previous_end{ibuffer.buffer_end};
	auto const next_end{
		::fast_io::operations::decay::read_some_decay_dispatch(insm, ibuffer.buffer_begin, ibuffer.buffer_begin + bfsz)};
	if (next_end == ibuffer.buffer_begin)
	{
		// A failed refill is an observation of EOF, not a new empty chunk. Preserve the exhausted previous span so a
		// context scanner can apply its documented EOF rewind within that storage. Resetting end to begin first made a
		// subsequent rewind install curr > end. On the initial allocation previous_end equals begin, so the same rule
		// still leaves a canonical empty buffer.
		ibuffer.buffer_curr = ibuffer.buffer_end = previous_end;
		return false;
	}
	ibuffer.buffer_curr = ibuffer.buffer_begin;
	ibuffer.buffer_end = next_end;
	return true;
}

template <::std::size_t bfsz, ::std::integral char_type, typename allocator_type, typename instmtype>
inline constexpr bool ibuffer_underflow_rl_impl(instmtype &insm, basic_io_buffer_pointers<char_type> &ibuffer)
{
	return ::fast_io::details::io_buffer::ibuffer_underflow_rl_size_impl<allocator_type>(insm, ibuffer, bfsz);
}

template <typename allocator_type, ::std::integral char_type, typename instmtype>
inline constexpr void
ibuffer_minimum_size_underflow_all_prepare_rl_size_impl(instmtype &insm, basic_io_buffer_pointers<char_type> &ibuffer,
																::std::size_t bfsz)
{
	using typed_allocator_type = ::fast_io::typed_generic_allocator_adapter<allocator_type, char_type>;
	if (ibuffer.buffer_begin == nullptr)
	{
		ibuffer.buffer_end = ibuffer.buffer_curr = ibuffer.buffer_begin = typed_allocator_type::allocate(bfsz);
	}
	auto bg{ibuffer.buffer_begin};
	auto ed{bg + bfsz};
	::fast_io::operations::decay::read_all_decay_dispatch(insm, bg, ed);
	ibuffer.buffer_curr = bg;
	ibuffer.buffer_end = ed;
}

template <::std::size_t bfsz, ::std::integral char_type, typename allocator_type, typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void ibuffer_minimum_size_underflow_all_prepare_impl(instmtype &insm,
														  basic_io_buffer_pointers<char_type> &ibuffer)
{
	::fast_io::details::io_buffer::ibuffer_minimum_size_underflow_all_prepare_rl_size_impl<allocator_type>(
		insm, ibuffer, bfsz);
}

template <typename allocator_type, ::std::integral char_type, typename instmtype>
inline constexpr char_type *read_some_underflow_size_impl(instmtype &instm,
														  basic_io_buffer_pointers<char_type> &__restrict pointers,
														  char_type *first, char_type *last, ::std::size_t bfsz)
{
	using typed_allocator_type = ::fast_io::typed_generic_allocator_adapter<allocator_type, char_type>;
	if (pointers.buffer_begin == nullptr)
	{
		pointers.buffer_end = pointers.buffer_curr = pointers.buffer_begin = typed_allocator_type::allocate(bfsz);
	}
	if constexpr (::fast_io::operations::decay::defines::has_any_of_read_bytes_operations<instmtype>)
	{
		::fast_io::io_scatter_t scatters[2]{
			{first, static_cast<::std::size_t>(reinterpret_cast<::std::byte const *>(last) -
											   reinterpret_cast<::std::byte const *>(first))},
			{pointers.buffer_begin, bfsz * sizeof(char_type)}};
		auto [pos, scpos]{::fast_io::operations::decay::scatter_read_some_bytes_decay_dispatch(instm, scatters, 2)};
		if (pos == 2)
		{
			pointers.buffer_end = (pointers.buffer_curr = pointers.buffer_begin) + bfsz;
			return last;
		}
		else if (pos == 1)
		{
			pointers.buffer_end = (pointers.buffer_curr = pointers.buffer_begin) + (scpos / sizeof(char_type));
			return last;
		}
		else
		{
			pointers.buffer_end = pointers.buffer_curr = pointers.buffer_begin;
			return first + scpos / sizeof(char_type);
		}
	}
	else
	{
		basic_io_scatter_t<char_type> scatters[2]{{first, static_cast<::std::size_t>(last - first)},
												  {pointers.buffer_begin, bfsz}};
		auto [pos, scpos]{::fast_io::operations::decay::scatter_read_some_decay_dispatch(instm, scatters, 2)};
		if (pos == 2)
		{
			pointers.buffer_end = (pointers.buffer_curr = pointers.buffer_begin) + bfsz;
			return last;
		}
		else if (pos == 1)
		{
			pointers.buffer_end = (pointers.buffer_curr = pointers.buffer_begin) + scpos;
			return last;
		}
		else
		{
			pointers.buffer_end = pointers.buffer_curr = pointers.buffer_begin;
			return first + scpos;
		}
	}
}

template <::std::size_t bfsz, typename allocatortype, ::std::integral char_type, typename instmtype>
inline constexpr char_type *read_some_underflow_impl(instmtype &instm,
													 basic_io_buffer_pointers<char_type> &__restrict pointers,
													 char_type *first, char_type *last)
{
	return ::fast_io::details::io_buffer::read_some_underflow_size_impl<allocatortype>(instm, pointers, first,
																					   last, bfsz);
}

} // namespace details::io_buffer

/**
 * @brief Returns available buffered characters before refilling from the underlying device.
 * @details A duplex peer may wait for a reply once its last record is in the get area.
 * Delivering that prefix satisfies read_some even if the destination has room for more.
 * A subsequent call can flush tied output and refill once the get area is exhausted.
 */
template <typename io_buffer_type, ::std::integral char_type>
inline constexpr char_type *read_some_underflow_define(basic_io_buffer_ref<io_buffer_type> iobref,
													   char_type *first, char_type *last)
{
	if (first == last)
	{
		return first;
	}
	auto &input = iobref.iobptr->input_buffer;
	if (input.buffer_curr != input.buffer_end)
	{
		// A partial buffer hit is already progress. A blocking refill here can deadlock a duplex protocol.
		auto const available{::fast_io::details::ibuffer_remaining_size(input.buffer_curr, input.buffer_end)};
		auto const requested{::fast_io::details::input_pointer_range_size(first, last)};
		auto const count{available < requested ? available : requested};
		::fast_io::details::non_overlapped_copy_n(input.buffer_curr, count, first);
		input.buffer_curr += count;
		return first + count;
	}
	constexpr auto mode{io_buffer_type::traits_type::mode};
	if constexpr ((mode & buffer_mode::out) == buffer_mode::out && (mode & buffer_mode::tie) == buffer_mode::tie)
	{
		output_stream_buffer_flush_define(iobref);
	}
	// The handle projection is normalized once per refill operation. A prvalue obtains this local owner, while a
	// reference-returning CPO preserves the wrapped device cursor; every refill helper below borrows the same object.
	decltype(auto) inref = ::fast_io::operations::input_stream_ref(iobref.iobptr->handle);
	return ::fast_io::details::io_buffer::read_some_underflow_impl<
		io_buffer_type::traits_type::input_buffer_size, typename io_buffer_type::traits_type::allocator_type>(
		inref, iobref.iobptr->input_buffer, first, last);
}

template <typename io_buffer_type, ::std::integral char_type>
inline constexpr char_type *pread_some_underflow_define(basic_io_buffer_ref<io_buffer_type> iobref,
														char_type *first, char_type *last, ::fast_io::intfpos_t off)
{
	constexpr auto mode{io_buffer_type::traits_type::mode};
	if constexpr ((mode & buffer_mode::out) == buffer_mode::out && (mode & buffer_mode::tie) == buffer_mode::tie)
	{
		output_stream_buffer_flush_define(iobref);
	}
	decltype(auto) inref = ::fast_io::operations::input_stream_ref(iobref.iobptr->handle);
	return ::fast_io::operations::decay::pread_some_decay_dispatch(inref, first, last, off);
}

template <typename io_buffer_type, ::std::integral char_type>
inline constexpr void pread_all_underflow_define(basic_io_buffer_ref<io_buffer_type> iobref,
												 char_type *first, char_type *last, ::fast_io::intfpos_t off)
{
	constexpr auto mode{io_buffer_type::traits_type::mode};
	if constexpr ((mode & buffer_mode::out) == buffer_mode::out && (mode & buffer_mode::tie) == buffer_mode::tie)
	{
		output_stream_buffer_flush_define(iobref);
	}
	decltype(auto) inref = ::fast_io::operations::input_stream_ref(iobref.iobptr->handle);
	return ::fast_io::operations::decay::pread_all_decay_dispatch(inref, first, last, off);
}

template <typename io_buffer_type, ::std::integral char_type>
inline constexpr ::fast_io::io_scatter_status_t scatter_pread_some_underflow_define(basic_io_buffer_ref<io_buffer_type> iobref,
																					basic_io_scatter_t<char_type> const *pscatters, ::std::size_t n, ::fast_io::intfpos_t off)
{
	constexpr auto mode{io_buffer_type::traits_type::mode};
	if constexpr ((mode & buffer_mode::out) == buffer_mode::out && (mode & buffer_mode::tie) == buffer_mode::tie)
	{
		output_stream_buffer_flush_define(iobref);
	}
	decltype(auto) inref = ::fast_io::operations::input_stream_ref(iobref.iobptr->handle);
	return ::fast_io::operations::decay::scatter_pread_some_decay_dispatch(inref, pscatters, n, off);
}

template <typename io_buffer_type, ::std::integral char_type>
inline constexpr void scatter_pread_all_underflow_define(basic_io_buffer_ref<io_buffer_type> iobref,
														 basic_io_scatter_t<char_type> const *pscatters, ::std::size_t n, ::fast_io::intfpos_t off)
{
	constexpr auto mode{io_buffer_type::traits_type::mode};
	if constexpr ((mode & buffer_mode::out) == buffer_mode::out && (mode & buffer_mode::tie) == buffer_mode::tie)
	{
		output_stream_buffer_flush_define(iobref);
	}
	decltype(auto) inref = ::fast_io::operations::input_stream_ref(iobref.iobptr->handle);
	::fast_io::operations::decay::scatter_pread_all_decay_dispatch(inref, pscatters, n, off);
}

template <typename io_buffer_type>
inline constexpr bool ibuffer_underflow(basic_io_buffer_ref<io_buffer_type> iobref)
{
	constexpr auto mode{io_buffer_type::traits_type::mode};
	if constexpr ((mode & buffer_mode::out) == buffer_mode::out && (mode & buffer_mode::tie) == buffer_mode::tie)
	{
		output_stream_buffer_flush_define(iobref);
	}
	decltype(auto) inref = ::fast_io::operations::input_stream_ref(iobref.iobptr->handle);
	return ::fast_io::details::io_buffer::ibuffer_underflow_rl_impl<
		io_buffer_type::traits_type::input_buffer_size, typename io_buffer_type::traits_type::input_char_type,
		typename io_buffer_type::traits_type::allocator_type>(
		inref, iobref.iobptr->input_buffer);
}

template <typename io_buffer_type>
inline constexpr auto ibuffer_begin(basic_io_buffer_ref<io_buffer_type> iobref) noexcept
{
	return iobref.iobptr->input_buffer.buffer_begin;
}

template <typename io_buffer_type>
inline constexpr auto ibuffer_curr(basic_io_buffer_ref<io_buffer_type> iobref) noexcept
{
	return iobref.iobptr->input_buffer.buffer_curr;
}

template <typename io_buffer_type>
inline constexpr auto ibuffer_end(basic_io_buffer_ref<io_buffer_type> iobref) noexcept
{
	return iobref.iobptr->input_buffer.buffer_end;
}

template <typename io_buffer_type>
inline constexpr void ibuffer_set_curr(basic_io_buffer_ref<io_buffer_type> iobref,
									   typename basic_io_buffer_ref<io_buffer_type>::input_char_type *ptr) noexcept
{
	iobref.iobptr->input_buffer.buffer_curr = ptr;
}

template <::std::integral char_type, typename io_buffer_type>
	requires(::std::same_as<char_type, typename basic_io_buffer_ref<io_buffer_type>::input_char_type>)
inline constexpr ::std::size_t
	ibuffer_minimum_size_define(::fast_io::io_reserve_type_t<char_type, basic_io_buffer_ref<io_buffer_type>>)
{
	return io_buffer_type::traits_type::input_buffer_size;
}

template <typename io_buffer_type>
inline constexpr void ibuffer_minimum_size_underflow_all_prepare_define(basic_io_buffer_ref<io_buffer_type> iobref)
{
	decltype(auto) inref = ::fast_io::operations::input_stream_ref(iobref.iobptr->handle);
	::fast_io::details::io_buffer::ibuffer_minimum_size_underflow_all_prepare_impl<
		io_buffer_type::traits_type::input_buffer_size, typename io_buffer_type::traits_type::input_char_type,
		typename io_buffer_type::traits_type::allocator_type>(
		inref, iobref.iobptr->input_buffer);
}

namespace details::io_buffer
{
/** @brief Proves a bounded decimal field and its delimiter reside in the current read area, without consuming them. */
template <typename T, typename Char>
inline constexpr bool buffered_decimal_field(Char const *&p, Char const *end) noexcept
{
	auto space = [](Char c) {
		return c == ::fast_io::char_literal_v<u8' ', Char> || c == ::fast_io::char_literal_v<u8'\n', Char> ||
			   c == ::fast_io::char_literal_v<u8'\r', Char> || c == ::fast_io::char_literal_v<u8'\t', Char>;
	};
	// Limit speculation to one short separator run. Whitespace of any length remains supported by the scanner.
	for (unsigned skipped{}; p != end && space(*p); ++p)
	{
		if (++skipped == 8u)
		{
			return false;
		}
	}
	if (p == end)
	{
		return false;
	}
	if constexpr (::std::is_signed_v<T>)
	{
		if (*p == ::fast_io::char_literal_v<u8'-', Char>)
		{
			++p;
		}
	}
	auto begin{p};
	while (p != end && *p >= ::fast_io::char_literal_v<u8'0', Char> && *p <= ::fast_io::char_literal_v<u8'9', Char>)
	{
		if (++p - begin > ::std::numeric_limits<T>::digits10 + 1)
		{
			return false;
		}
	}
	return p != begin && p != end && space(*p);
}

} // namespace details::io_buffer

/**
 * @brief Proves a complete ordinary decimal scan can finish without a refill.
 * @details The lookahead uses a private cursor, never mutates targets and never interprets a buffer boundary as EOF.
 * Every unsupported spelling or incomplete final field goes through the original scanner and its refill protocol.
 */
template <typename Buffer, details::io_buffer::decimal_scan_integer... Targets>
	requires details::io_buffer::plain_integer_window<basic_io_buffer_ref<Buffer>, Targets...>
inline constexpr bool input_stream_scan_ready_define(basic_io_buffer_ref<Buffer> input, io_type_t<Targets>...) noexcept
{
	auto p{ibuffer_curr(input)};
	auto end{ibuffer_end(input)};
	using Char = typename Buffer::input_char_type;
	Char const *candidate{p};
	return (::fast_io::details::io_buffer::buffered_decimal_field<Targets>(candidate, static_cast<Char const *>(end)) && ...);
}

/** @brief Tries the decimal tuple in a buffered device's current area before permitting a refill. */
template <typename Buffer, details::io_buffer::decimal_scan_integer... Targets>
	requires details::io_buffer::plain_integer_window<basic_io_buffer_ref<Buffer>, Targets...>
inline constexpr bool input_stream_try_scan_define(basic_io_buffer_ref<Buffer> input, Targets &...targets) noexcept
{
	return details::io_buffer::try_scan_integer_window(input, targets...);
}

/**
 * @brief Commits a resident unsigned field before acquiring a suspending device operation.
 * @details A short logical window may still contain a complete field. The canonical resident
 * probe checks it before the caller can request a refill. Both trials leave an incomplete
 * target untouched and leave the same byte available to the refilling state machine.
 */
template <typename Buffer>
	requires (::std::numeric_limits<unsigned>::digits == 32 && details::io_buffer::builtin_decimal_window<basic_io_buffer_ref<Buffer>> &&
			  details::io_buffer::plain_integer_window<basic_io_buffer_ref<Buffer>, unsigned>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool input_stream_try_scan_define(basic_io_buffer_ref<Buffer> input,
																			 unsigned &target) noexcept
{
	if (details::io_buffer::try_scan_resident_unsigned(input, target)) [[likely]]
	{
		return true;
	}
	return details::io_buffer::try_scan_integer_window<false>(input, target);
}

/** @brief Finishes a delimited integer locally before a buffered async scan can request a refill. */
template <typename Buffer, details::io_buffer::bounded_decimal_target Target>
	requires (details::io_buffer::builtin_decimal_window<basic_io_buffer_ref<Buffer>> &&
			  details::io_buffer::plain_integer_window<basic_io_buffer_ref<Buffer>, Target>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool input_stream_try_scan_define(basic_io_buffer_ref<Buffer> input,
																			Target &target) noexcept
{
	if (details::io_buffer::try_scan_resident_integral(input, target))
	{
		return true;
	}
	// The get area can end before the maximum field width yet still contain a delimiter.
	return details::io_buffer::try_scan_integer_window<false>(input, target);
}

/**
 * @brief Parses a complete resident arithmetic field on the awaiting thread.
 * @details A floating scanner with a transactional current-chunk operation can prove its own
 * boundary, including punctuation. Its successful result commits exactly the returned prefix.
 * A miss leaves the destination and cursor unchanged. For other spellings such as infinity,
 * a nonempty field followed by C whitespace has a physical token boundary in the
 * current get area. Extending that area cannot extend a decimal number across the delimiter.
 * The ordinary contiguous scanner must accept the entire spelling. It writes a private scalar
 * so underflow, overflow and an incomplete exponent retain the original target and cursor on
 * a miss. The ordinary refilling scanner then owns the outcome, including its precise error
 * position. Only the built-in unqualified arithmetic aliases participate in this transaction.
 */
template <typename Buffer, typename Target>
	requires (::std::same_as<Target, ::std::remove_cv_t<Target>> &&
			  !details::io_buffer::decimal_scan_integer<Target> &&
			  !(details::io_buffer::builtin_decimal_window<basic_io_buffer_ref<Buffer>> && details::io_buffer::bounded_decimal_target<Target>) &&
			  ((details::non_character_integral<Target> && !::std::same_as<Target, bool>) || details::my_floating_point<Target>) &&
			  details::io_buffer::plain_integer_window<basic_io_buffer_ref<Buffer>, Target> &&
			  requires (Target &v, typename Buffer::input_char_type const *p) {
				  { scan_contiguous_define(io_reserve_type<typename Buffer::input_char_type,
					  decltype(scan_alias_define(io_alias, v))>, p, p, scan_alias_define(io_alias, v)) } noexcept
					  -> ::std::same_as<parse_result<typename Buffer::input_char_type const *>>;
			  })
inline constexpr bool input_stream_try_scan_define(basic_io_buffer_ref<Buffer> input, Target &target) noexcept
{
	using Char = typename Buffer::input_char_type;
	auto *current{ibuffer_curr(input)};
	Char const *first{current};
	Char const *last{ibuffer_end(input)};
	if (first == last)
	{
		return false;
	}
	Target value{};
	auto proxy{scan_alias_define(io_alias, value)};
	auto tag{io_reserve_type<Char, decltype(proxy)>};
	if constexpr (details::my_floating_point<Target> && current_chunk_context_scannable<Char, decltype(proxy)> &&
				  requires { { scan_context_current_chunk_define(tag, first, last, proxy) } noexcept; })
	{
		constexpr auto minimum{scan_context_current_chunk_minimum_size(io_reserve_type<Char, decltype(proxy)>)};
		if (minimum == 1u || static_cast<::std::size_t>(last - first) >= minimum)
		{
			auto const result{scan_context_current_chunk_define(tag, first, last, proxy)};
			if (result.code == parse_code::ok)
			{
				target = value;
				ibuffer_set_curr(input, current + (result.iter - first));
				return true;
			}
		}
	}
	first = details::find_space_common_impl<false, true>(first, last);
	auto const delimiter{details::find_space_common_impl<false, false>(first, last)};
	if (delimiter == last)
	{
		return false;
	}
	auto result{scan_contiguous_define(tag, first, delimiter, proxy)};
	if (result.code != parse_code::ok || result.iter != delimiter)
	{
		return false;
	}
	target = value;
	ibuffer_set_curr(input, current + (delimiter - current));
	return true;
}

/** @brief Uses the refilling integer state machine and returns the exact failure code after committed progress. */
template <typename Buffer, details::io_buffer::decimal_scan_integer... Targets>
	requires details::io_buffer::plain_integer_window<basic_io_buffer_ref<Buffer>, Targets...>
inline constexpr parse_code input_stream_scan_code_define(basic_io_buffer_ref<Buffer> in, Targets &...targets) FAST_IO_HERBCEPTIONS_THROWS
{
	return details::io_buffer::scan_integer_codes(in, targets...);
}

} // namespace fast_io
