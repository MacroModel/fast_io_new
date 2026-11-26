#pragma once

namespace fast_io::transcoders
{
/** @brief Distinguishes completed ranges, incomplete scalars, malformed data and insufficient output room. */
using utf_status = ::fast_io::details::codecvt::unicode_status;
using utf_byte_result = ::fast_io::details::codecvt::unicode_progress<::std::byte, ::std::byte>;

/** @brief Selects how a UTF wire engine handles malformed and truncated input. */
enum class utf_error_policy : unsigned char
{
	/** Stop at the start of the first ill-formed scalar. */
	strict,
	/** Emit one U+FFFD per maximal subpart and resume at the first byte outside it. */
	replace
};

/**
 * @brief Converts a byte stream between two Unicode encodings using exact-width, alignment-independent accesses.
 * @details From and To specify the code-unit widths. The two encoding schemes describe object representations
 * on the wire. The default is native, including native PDP-11 word order. Each scalar is committed atomically.
 * try_process is usable without exceptions and retains at most three bytes across input fragments.
 * Strict mode reports the first invalid scalar. Subtract the previously reported pending count from
 * the result's absolute input position if that scalar started in an earlier range. Discard a failed message.
 * Replacement mode consumes one maximal subpart for each U+FFFD and emits one U+FFFD for a nonempty
 * unfinished suffix at terminal finish. No allocator is needed.
 */
template <::std::integral From, ::std::integral To, encoding_scheme FromOrder = encoding_scheme::utf,
		  encoding_scheme ToOrder = encoding_scheme::utf, utf_error_policy Policy = utf_error_policy::strict>
	requires((sizeof(From) == 1 || sizeof(From) == 2 || sizeof(From) == 4) &&
			 (sizeof(To) == 1 || sizeof(To) == 2 || sizeof(To) == 4) &&
			 !::std::same_as<From, bool> && !::std::same_as<To, bool> &&
			 ::fast_io::details::codecvt::encoding_is_utf(FromOrder) && ::fast_io::details::codecvt::encoding_is_utf(ToOrder) &&
			 (Policy == utf_error_policy::strict || Policy == utf_error_policy::replace))
class utf_wire
{
	/** @brief Permits the serial async driver to suspend between bounded steps while this engine retains its state. */
	template <bool Input>
	friend constexpr ::std::true_type async_transcode_step_define(::fast_io::transcode_reserve_t<utf_wire>, ::std::bool_constant<Input>) noexcept
	{
		return {};
	}

	/**
	 * @brief Allows UTF-8 replacement decoding to publish a larger batch of UTF-16 bytes.
	 * @details Malformed input yields one U+FFFD per maximal subpart. Each completed scalar commits
	 * all its code units at once. The carry retains the same unfinished suffix across window sizes.
	 */
	friend constexpr ::std::true_type transcode_input_batch_define(::fast_io::transcode_reserve_t<utf_wire>) noexcept
		requires (Policy == utf_error_policy::replace && sizeof(From) == 1 && sizeof(To) == 2)
	{
		return {};
	}

	/** @brief Allows larger transfers within the output of one successful strict step. */
	friend constexpr ::std::true_type transcode_verified_window_define(::fast_io::transcode_reserve_t<utf_wire>) noexcept
		requires (Policy == utf_error_policy::strict && sizeof(From) == 1 && sizeof(To) == 2)
	{
		return {};
	}

	/** @brief An empty nonterminal flush preserves incomplete UTF input until more data or finish. */
	friend constexpr ::std::true_type transcode_inert_sync_flush_define(::fast_io::transcode_reserve_t<utf_wire>) noexcept
	{
		return {};
	}

	::std::byte tail[4]{};
	unsigned char count{};
	static constexpr bool replaces_errors{Policy == utf_error_policy::replace};

	/** @brief Converts as many complete physical code units as fit, including an unaligned first or last unit. */
	static utf_byte_result bulk(::std::byte const *p, ::std::byte const *last, ::std::byte *out, ::std::byte *end) noexcept
	{
		namespace cv = ::fast_io::details::codecvt;
		if (p == last) { return {p, out, utf_status::complete}; }
		auto const available{(last - p) / static_cast<::std::ptrdiff_t>(sizeof(From))};
		auto const room{out == end ? ::std::size_t{} : static_cast<::std::size_t>(end - out) / sizeof(To)};
		auto first{cv::unicode_byte_cursor<From>(p)};
		auto dest{cv::unicode_byte_cursor<To>(out)};
		auto r{cv::unicode_convert_range<FromOrder, ToOrder, replaces_errors>(first, first + available, dest, room)};
		auto next{reinterpret_cast<::std::byte const *>(cv::unicode_address(r.input))};
		auto status{r.status == utf_status::complete && next != last ? utf_status::incomplete : r.status};
		return {next, reinterpret_cast<::std::byte *>(cv::unicode_address(r.output)), status};
	}

public:
	using from_value_type = ::std::byte;
	using to_value_type = ::std::byte;

#if defined(__HERBCEPTIONS__)
	/**
	 * @brief Reports a strict decoding failure directly to an effect-aware consumer.
	 * @details The observer borrows the same carry as the legacy bounded step. A failure
	 * returns before the consumer publishes the speculative destination window.
	 */
	friend basic_transcode_process_result<::std::byte, ::std::byte>
	transcode_process_checked_define(transcoder_ref<utf_wire> ref, ::std::byte const *first,
		::std::byte const *last, ::std::byte *out, ::std::byte *end) throws
		requires (!replaces_errors)
	{
		auto result{ref.ptr->try_process(first, last, out, end)};
		if (result.status == utf_status::invalid) [[unlikely]]
		{
			::fast_io::details::raise_scan_error(parse_code::invalid);
		}
		return {result.input, result.output, result.status == utf_status::output_full
			? transcode_step_status::need_output : transcode_step_status::need_input};
	}

	/** @brief Delivers an incomplete terminal scalar through the caller's deterministic error channel. */
	friend basic_transcode_drain_result<::std::byte>
	transcode_finish_checked_define(transcoder_ref<utf_wire> ref, ::std::byte *out, ::std::byte *) throws
		requires (!replaces_errors)
	{
		if (ref.ptr->pending_bytes()) [[unlikely]]
		{
			::fast_io::details::raise_scan_error(parse_code::invalid);
		}
		return {out, transcode_drain_status::complete};
	}
#endif

	/** @brief Returns the number of accepted bytes belonging to the single unfinished scalar. */
	constexpr unsigned pending_bytes() const noexcept
	{
		return count;
	}

private:
	utf_byte_result try_process_regular(::std::byte const *p, ::std::byte const *last, ::std::byte *out, ::std::byte *end) noexcept
	{
		namespace cv = ::fast_io::details::codecvt;
		while (count)
		{
			if (p == last)
			{
				return {p, out, utf_status::incomplete};
			}
			auto const remaining{static_cast<::std::size_t>(last - p)};
			auto const n{remaining < 4u - count ? remaining : 4u - count};
			::fast_io::freestanding::my_memcpy(tail + count, p, n);
			auto const bytes{count + n};
			From input[4 / sizeof(From)]{};
			::fast_io::freestanding::my_memcpy(input, tail, bytes);
			auto scalar{bytes < sizeof(From) ? cv::unicode_scalar{0, 0, utf_status::incomplete} : cv::unicode_decode<FromOrder>(input, bytes / sizeof(From))};
			if (scalar.status == utf_status::invalid) [[unlikely]]
			{
				if constexpr (replaces_errors)
				{
					scalar.value = 0xfffdu;
				}
				else
				{
					return {p, out, utf_status::invalid};
				}
			}
			if (scalar.status == utf_status::incomplete)
			{
				count = static_cast<unsigned char>(bytes);
				return {last, out, utf_status::incomplete};
			}
			auto width{cv::unicode_width<To>(scalar.value)};
			auto written{width * sizeof(To)};
			if (out == end || static_cast<::std::size_t>(end - out) < written)
			{
				return {p, out, utf_status::output_full};
			}
			To units[4 / sizeof(To)]{};
			cv::unicode_emit<ToOrder>(scalar.value, width, units);
			::fast_io::freestanding::my_memcpy(out, units, written);
			out += written;
			auto const consumed{scalar.units * sizeof(From)};
			if constexpr (replaces_errors && sizeof(From) == 2u)
			{
				if (consumed < count) [[unlikely]]
				{
					// A rejected high surrogate can leave the first byte of the next unit in carry.
					tail[0] = tail[consumed];
					count = 1;
					continue;
				}
			}
			p += consumed - count;
			count = 0;
		}
		auto result{bulk(p, last, out, end)};
		if (result.status == utf_status::incomplete)
		{
			return remember_tail(result, last);
		}
		return result;
	}

#if __has_cpp_attribute(__gnu__::__noinline__)
	[[__gnu__::__noinline__]]
#endif
	utf_byte_result resume_pending(::std::byte const *p, ::std::byte const *last, ::std::byte *out, ::std::byte *end) noexcept
	{
		return try_process_regular(p, last, out, end);
	}

#if __has_cpp_attribute(__gnu__::__noinline__)
	[[__gnu__::__noinline__]]
#endif
	utf_byte_result remember_tail(utf_byte_result result, ::std::byte const *last) noexcept
	{
		count = static_cast<unsigned char>(last - result.input);
		::fast_io::freestanding::my_memcpy(tail, result.input, count);
		result.input = last;
		return result;
	}

public:
	/** @brief Returns exact byte cursors and a recoverable status for a bounded, disjoint input/output pair. */
	utf_byte_result try_process(::std::byte const *p, ::std::byte const *last, ::std::byte *out, ::std::byte *end) noexcept
	{
		namespace cv = ::fast_io::details::codecvt;
		if constexpr (cv::unicode_has_bmp_packet && sizeof(From) == 2u && sizeof(To) == 4u)
		{
			if (count) [[unlikely]]
			{
				return resume_pending(p, last, out, end);
			}
			constexpr ::std::ptrdiff_t packet_input{64u * sizeof(From)}, packet_output{64u * sizeof(To)};
			if (last - p == packet_input && end - out >= packet_output &&
				cv::utf_widen_bmp_packet<64, FromOrder, ToOrder>(cv::unicode_byte_cursor<From>(p), cv::unicode_byte_cursor<To>(out)))
			{
				return {last, out + packet_output, utf_status::complete};
			}
			auto result{bulk(p, last, out, end)};
			if (result.status == utf_status::incomplete) [[unlikely]]
			{
				return remember_tail(result, last);
			}
			return result;
		}
		return try_process_regular(p, last, out, end);
	}

	/** @brief Emits one replacement for the unfinished suffix when there is enough room for it. */
	friend inline basic_transcode_drain_result<::std::byte>
	transcode_finish_define(utf_wire &state, ::std::byte *out, ::std::byte *end) noexcept
		requires(replaces_errors)
	{
		namespace cv = ::fast_io::details::codecvt;
		if (!state.count)
		{
			return {out, transcode_drain_status::complete};
		}
		constexpr auto width{cv::unicode_width<To>(0xfffdu)};
		constexpr auto size{width * sizeof(To)};
		if (out == end || static_cast<::std::size_t>(end - out) < size)
		{
			return {out, transcode_drain_status::need_output};
		}
		cv::unicode_emit<ToOrder>(0xfffdu, width, cv::unicode_byte_cursor<To>(out));
		state.count = 0;
		return {out + size, transcode_drain_status::complete};
	}
};

/** @brief Gives a short batch enough source and destination lanes for the active Unicode kernel. */
template <typename F, typename T, encoding_scheme A, encoding_scheme B, utf_error_policy P>
inline constexpr transcode_buffer_sizes transcode_preferred_buffer_sizes_define(transcode_reserve_t<utf_wire<F, T, A, B, P>>) noexcept
{
	constexpr auto width{::fast_io::intrinsics::optimal_simd_vector_run_with_cpu_instruction_size};
	constexpr auto units{width ? 2u * width / sizeof(F) : 16u};
	return {units * sizeof(F), units * sizeof(T)};
}

/** @brief Reserves one longest scalar for processing and one replacement for a replacing finish. */
template <typename F, typename T, encoding_scheme A, encoding_scheme B, utf_error_policy P>
inline constexpr ::std::size_t transcode_min_output_size_define(transcode_reserve_t<utf_wire<F, T, A, B, P>>, transcode_phase phase) noexcept
{
	if constexpr (P == utf_error_policy::replace)
	{
		if (phase == transcode_phase::finish)
		{
			return ::fast_io::details::codecvt::unicode_width<T>(0xfffdu) * sizeof(T);
		}
	}
	return phase == transcode_phase::process ? 4u : 0u;
}

/**
 * @brief Bounds emitted bytes even when a previous input fragment left an incomplete scalar.
 * @details Every emitted scalar encodes in at most four bytes. A replacing UTF-8 or UTF-16
 * engine may additionally replace a buffered prefix before consuming the first new byte.
 * Saturation preserves the bound on addressable output. Flush retains the unfinished suffix.
 */
template <typename F, typename T, encoding_scheme A, encoding_scheme B, utf_error_policy P>
inline constexpr ::std::size_t transcode_max_output_size_define(transcode_reserve_t<utf_wire<F, T, A, B, P>> tag,
	::std::size_t bytes, transcode_phase phase) noexcept
{
	if (phase != transcode_phase::process)
	{
		return transcode_min_output_size_define(tag, phase);
	}
	constexpr auto maximum{::std::numeric_limits<::std::size_t>::max()};
	if constexpr (sizeof(F) == 1u && sizeof(T) == 2u)
	{
		// Each new UTF-8 byte pays for at most one UTF-16 unit. A carried
		// four-byte scalar can finish with one new byte and emit two units.
		// Replacement can instead emit a unit for the old prefix before
		// consuming any new byte. Both bounds are attained with an ASCII suffix.
		if (!bytes) { return 0u; }
		return bytes >= maximum / 2u ? maximum : (bytes + 1u) * 2u;
	}
	if constexpr (P == utf_error_policy::replace && sizeof(F) < 4u)
	{
		if (!bytes) { return 0u; }
		constexpr auto extra{::fast_io::details::codecvt::unicode_width<T>(0xfffdu) * sizeof(T)};
		return bytes > (maximum - extra) / 4u ? maximum : bytes * 4u + extra;
	}
	return bytes > maximum / 4u ? maximum : bytes * 4u;
}

/** @brief Maps the wire result into the bounded stream protocol and reports malformed strict input. */
template <typename F, typename T, encoding_scheme A, encoding_scheme B, utf_error_policy P>
inline basic_transcode_process_result<::std::byte, ::std::byte>
transcode_process_define(utf_wire<F, T, A, B, P> &state, ::std::byte const *p, ::std::byte const *last,
						 ::std::byte *out, ::std::byte *end) noexcept(P == utf_error_policy::replace)
{
	auto result{state.try_process(p, last, out, end)};
	if constexpr (P == utf_error_policy::strict)
	{
		if (result.status == utf_status::invalid)
		{
			::fast_io::throw_parse_code(::fast_io::parse_code::invalid);
		}
	}
	return {result.input, result.output, result.status == utf_status::output_full ? transcode_step_status::need_output : transcode_step_status::need_input};
}

/** @brief Rejects an incomplete trailing code unit or scalar and completes an empty or complete message. */
template <typename F, typename T, encoding_scheme A, encoding_scheme B>
inline basic_transcode_drain_result<::std::byte> transcode_finish_define(utf_wire<F, T, A, B> &state,
																		 ::std::byte *out, ::std::byte *)
{
	if (state.pending_bytes())
	{
		::fast_io::throw_parse_code(::fast_io::parse_code::invalid);
	}
	return {out, transcode_drain_status::complete};
}
} // namespace fast_io::transcoders
