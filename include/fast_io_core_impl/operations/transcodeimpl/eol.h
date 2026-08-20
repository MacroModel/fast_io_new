#pragma once

/**
 * @file
 * @brief Implements stateful bounded end-of-line conversion.
 *
 * CRLF expansion/contraction may split at either source or destination
 * boundaries. pending_character records exactly that one unresolved code unit,
 * and sync-flush/finish drain it without accessing an unbounded destination.
 */

namespace fast_io
{

namespace transcoders
{

/** @brief Identifies supported logical and platform-native newline encodings. */
enum class eol_scheme
{
	lf,
	crlf,
	cr,
	nl, /* EBCDIC */
#if 0
	lfcr,
	newline,
#endif
#if (defined(_WIN32) && !defined(__WINE__)) || defined(__CYGWIN__) || defined(__MSDOS__)
	// Windows-family native text convention.
	native = crlf
#else
	// POSIX and other platforms use LF as the native convention.
	native = lf
#endif
};

/** @brief Stateful bounded engine converting between newline conventions. */
template <::std::integral char_type, eol_scheme from_scheme, eol_scheme to_scheme>
struct basic_eol
{
	using from_value_type = char_type;
	using to_value_type = char_type;

	bool pending_character{};

private:
	/** @brief Derives the process status from the remaining bounded source range. */
	inline static constexpr ::fast_io::basic_transcode_process_result<char_type, char_type>
	process_result(char_type const *from_next, char_type const *from_last,
				   char_type *to_next) noexcept
	{
		return {from_next, to_next,
				from_next == from_last
					? ::fast_io::transcode_step_status::need_input
					: ::fast_io::transcode_step_status::need_output};
	}

	/** @brief Emits the single code unit retained across a bounded call boundary. */
	inline constexpr ::fast_io::basic_transcode_drain_result<char_type>
	drain(char_type *to_first, char_type *to_last) noexcept
	{
		// A pending LF completes an earlier CR insertion; a pending CR from CRLF
		// contraction becomes literal when no following LF is available.
		if (!pending_character)
		{
			// No expansion or contraction state remains to be emitted.
			return {to_first, ::fast_io::transcode_drain_status::complete};
		}
		if (to_first == to_last)
		{
			// Preserve pending state until the caller supplies writable capacity.
			return {to_first, ::fast_io::transcode_drain_status::need_output};
		}
		if constexpr (from_scheme == eol_scheme::lf &&
					  to_scheme == eol_scheme::crlf)
		{
			// Complete the LF half of an earlier CRLF expansion.
			*to_first = ::fast_io::char_literal_v<u8'\n', char_type>;
		}
		else
		{
			// Emit an unmatched CR retained by CRLF contraction.
			static_assert(from_scheme == eol_scheme::crlf &&
						  to_scheme == eol_scheme::lf);
			*to_first = ::fast_io::char_literal_v<u8'\r', char_type>;
		}
		pending_character = false;
		return {to_first + 1, ::fast_io::transcode_drain_status::complete};
	}

public:
	/** @brief Converts as much source as bounded destination capacity permits. */
	inline constexpr ::fast_io::basic_transcode_process_result<char_type, char_type>
	process(char_type const *from_first, char_type const *from_last,
			char_type *to_first, char_type *to_last) noexcept
	{
		if constexpr (from_scheme == eol_scheme::lf &&
					  to_scheme == eol_scheme::crlf)
		{
			// Expand each LF to CRLF while retaining a split trailing LF if needed.
			if (pending_character)
			{
				// Complete an expansion split at the preceding destination boundary.
				if (to_first == to_last)
				{
					// Request capacity without consuming any new source.
					return process_result(from_first, from_last, to_first);
				}
				*to_first = ::fast_io::char_literal_v<u8'\n', char_type>;
				++to_first;
				pending_character = false;
			}
			while (from_first != from_last && to_first != to_last)
			{
				// Expand bounded source units until source or destination is exhausted.
				auto const ch{*from_first};
				++from_first;
				if (ch == ::fast_io::char_literal_v<u8'\n', char_type>)
				{
					// Emit CR first so a full destination can retain only the LF half.
					*to_first = ::fast_io::char_literal_v<u8'\r', char_type>;
					++to_first;
					if (to_first == to_last)
					{
						// Remember that the LF half must lead the next process/drain call.
						pending_character = true;
						break;
					}
				}
				*to_first = ch;
				++to_first;
			}
		}
		else if constexpr (from_scheme == eol_scheme::crlf &&
						   to_scheme == eol_scheme::lf)
		{
			// Contract CRLF pairs while preserving unmatched CR code units.
			if (pending_character)
			{
				// Resolve a CR that ended the preceding bounded source range.
				if (to_first == to_last || from_first == from_last)
				{
					// Resolution requires both one output slot and one lookahead unit.
					return process_result(from_first, from_last, to_first);
				}
				if (*from_first == ::fast_io::char_literal_v<u8'\n', char_type>)
				{
					// Consume the LF and publish the contracted newline.
					*to_first = ::fast_io::char_literal_v<u8'\n', char_type>;
					++from_first;
				}
				else
				{
					// Publish the retained CR when lookahead is not LF.
					*to_first = ::fast_io::char_literal_v<u8'\r', char_type>;
				}
				++to_first;
				pending_character = false;
			}
			while (from_first != from_last && to_first != to_last)
			{
				// Contract pairs while retaining any CR that needs future lookahead.
				auto ch{*from_first};
				++from_first;
				if (ch == ::fast_io::char_literal_v<u8'\r', char_type>)
				{
					// A CR requires bounded lookahead to determine pair contraction.
					if (from_first == from_last)
					{
						// Retain a trailing CR until more input or terminal drain.
						pending_character = true;
						break;
					}
					if (*from_first == ::fast_io::char_literal_v<u8'\n', char_type>)
					{
						// Replace the CRLF pair with one LF and consume lookahead.
						ch = ::fast_io::char_literal_v<u8'\n', char_type>;
						++from_first;
					}
				}
				*to_first = ch;
				++to_first;
			}
		}
		else if constexpr ((from_scheme == eol_scheme::lf &&
							to_scheme == eol_scheme::cr) ||
						   (from_scheme == eol_scheme::cr &&
							to_scheme == eol_scheme::lf))
		{
			// Substitute single-unit LF and CR conventions without expansion.
			constexpr bool from_cr{from_scheme == eol_scheme::cr};
			constexpr char_type from_character{
				from_cr ? ::fast_io::char_literal_v<u8'\r', char_type>
						: ::fast_io::char_literal_v<u8'\n', char_type>};
			constexpr char_type to_character{
				from_cr ? ::fast_io::char_literal_v<u8'\n', char_type>
						: ::fast_io::char_literal_v<u8'\r', char_type>};
			while (from_first != from_last && to_first != to_last)
			{
				// Substitute each bounded single-unit newline occurrence.
				auto ch{*from_first};
				if (ch == from_character)
				{
					// Replace only the selected source newline code unit.
					ch = to_character;
				}
				*to_first = ch;
				++from_first;
				++to_first;
			}
		}
		else if constexpr ((from_scheme == eol_scheme::lf &&
							to_scheme == eol_scheme::nl) ||
						   (from_scheme == eol_scheme::nl &&
							to_scheme == eol_scheme::lf))
		{
			// Substitute between ASCII LF and the execution-set NL character.
			constexpr bool from_nl{from_scheme == eol_scheme::nl};
			constexpr char_type from_character{
				from_nl
					? ::fast_io::details::execution_newline_literal<char_type>()
					: ::fast_io::char_literal_v<u8'\n', char_type>};
			constexpr char_type to_character{
				from_nl
					? ::fast_io::char_literal_v<u8'\n', char_type>
					: ::fast_io::details::execution_newline_literal<char_type>()};
			while (from_first != from_last && to_first != to_last)
			{
				*to_first = *from_first == from_character
								? to_character
								: *from_first;
				++from_first;
				++to_first;
			}
		}
		else
		{
			// Identical or otherwise no-op schemes copy bounded units verbatim.
			while (from_first != from_last && to_first != to_last)
			{
				*to_first = *from_first;
				++from_first;
				++to_first;
			}
		}
		return process_result(from_first, from_last, to_first);
	}

	/** @brief Nonterminally emits any pending split newline code unit. */
	inline constexpr ::fast_io::basic_transcode_drain_result<char_type>
	sync_flush(char_type *to_first, char_type *to_last) noexcept
	{
		return drain(to_first, to_last);
	}

	/** @brief Terminally emits any pending expansion or unmatched CR. */
	inline constexpr ::fast_io::basic_transcode_drain_result<char_type>
	finish(char_type *to_first, char_type *to_last) noexcept
	{
		return drain(to_first, to_last);
	}
};

/** @brief Exposes bounded newline processing through the transcode process CPO. */
template <::std::integral char_type, eol_scheme from_scheme, eol_scheme to_scheme>
inline constexpr ::fast_io::basic_transcode_process_result<char_type, char_type>
transcode_process_define(
	basic_eol<char_type, from_scheme, to_scheme> &engine,
	char_type const *from_first, char_type const *from_last,
	char_type *to_first, char_type *to_last) noexcept
{
	return engine.process(from_first, from_last, to_first, to_last);
}

/** @brief Exposes nonterminal newline drain through the sync-flush CPO. */
template <::std::integral char_type, eol_scheme from_scheme, eol_scheme to_scheme>
inline constexpr ::fast_io::basic_transcode_drain_result<char_type>
transcode_sync_flush_define(
	basic_eol<char_type, from_scheme, to_scheme> &engine,
	char_type *to_first, char_type *to_last) noexcept
{
	return engine.sync_flush(to_first, to_last);
}

/** @brief Exposes terminal newline drain through the finish CPO. */
template <::std::integral char_type, eol_scheme from_scheme, eol_scheme to_scheme>
inline constexpr ::fast_io::basic_transcode_drain_result<char_type>
transcode_finish_define(
	basic_eol<char_type, from_scheme, to_scheme> &engine,
	char_type *to_first, char_type *to_last) noexcept
{
	return engine.finish(to_first, to_last);
}

/** @brief Guarantees that every newline phase can progress with one output unit. */
template <::std::integral char_type, eol_scheme from_scheme, eol_scheme to_scheme>
inline constexpr ::std::size_t transcode_min_output_size_define(
	::fast_io::transcode_reserve_t<basic_eol<char_type, from_scheme, to_scheme>>,
	::fast_io::transcode_phase) noexcept
{
	return 1u;
}

using lf_to_crlf = basic_eol<char, eol_scheme::lf, eol_scheme::crlf>;
using crlf_to_lf = basic_eol<char, eol_scheme::crlf, eol_scheme::lf>;

using wlf_to_crlf = basic_eol<wchar_t, eol_scheme::lf, eol_scheme::crlf>;
using wcrlf_to_lf = basic_eol<wchar_t, eol_scheme::crlf, eol_scheme::lf>;

using u8lf_to_crlf = basic_eol<char8_t, eol_scheme::lf, eol_scheme::crlf>;
using u8crlf_to_lf = basic_eol<char8_t, eol_scheme::crlf, eol_scheme::lf>;

using u16lf_to_crlf = basic_eol<char16_t, eol_scheme::lf, eol_scheme::crlf>;
using u16crlf_to_lf = basic_eol<char16_t, eol_scheme::crlf, eol_scheme::lf>;

using u32lf_to_crlf = basic_eol<char32_t, eol_scheme::lf, eol_scheme::crlf>;
using u32crlf_to_lf = basic_eol<char32_t, eol_scheme::crlf, eol_scheme::lf>;

} // namespace transcoders

} // namespace fast_io
