#pragma once

#include "../../fast_io_core.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

namespace fast_io::json
{

namespace details
{

enum class json_stage1_errc : ::std::uint_least8_t
{
	none,
	invalid_unicode,
	unescaped_control_character,
	unterminated_string
};

struct json_stage1_result
{
	::std::vector<::std::size_t> structurals{};
	::std::size_t error_offset{};
	json_stage1_errc error{json_stage1_errc::none};

	[[nodiscard]] constexpr explicit operator bool() const noexcept
	{
		return error == json_stage1_errc::none;
	}
};

/*
The structural pass records every JSON punctuation character and every
pseudo-structural character (the first non-space character after either JSON
whitespace or punctuation).  A quote beginning a string is a pseudo-structural.
Its matching quote is deliberately not stored.  Stage two consequently walks
tokens without rescanning whitespace; its SIMD string decoder finds a closing
quote or the first escape while copying the decoded value.

The only sequential state in an ASCII block is (1) whether the current byte is
inside a string and (2) the parity of the immediately preceding run of
backslashes.  Toggling `backslash_odd` for each backslash and clearing it on any
other non-quote byte proves that a quote is escaped exactly when its preceding
maximal backslash run has odd length.  Carrying these two bits between blocks is
therefore sufficient even when a run ends at a SIMD boundary.
*/
struct json_stage1_state
{
	bool in_string{};
	bool backslash_odd{};
	bool may_begin_pseudo{true};

	::std::uint_least32_t utf8_code_point{};
	::std::uint_least32_t utf8_minimum{};
	::std::uint_least8_t utf8_remaining{};
	::std::size_t utf8_sequence_offset{};

	bool pending_high_surrogate{};
	::std::size_t high_surrogate_offset{};
};

[[nodiscard]] inline constexpr bool json_ascii_space(::std::uint_least32_t value) noexcept
{
	return value == 0x20u || value == 0x09u || value == 0x0Au || value == 0x0Du;
}

[[nodiscard]] inline constexpr bool json_ascii_structural(::std::uint_least32_t value) noexcept
{
	return value == 0x7Bu || value == 0x7Du || value == 0x5Bu || value == 0x5Du ||
		   value == 0x2Cu || value == 0x3Au;
}

inline constexpr bool json_stage1_fail(json_stage1_result &result,
									   json_stage1_errc error, ::std::size_t offset) noexcept
{
	result.error = error;
	result.error_offset = offset;
	return false;
}

inline constexpr bool json_stage1_validate_utf8_byte(json_stage1_state &state,
													 json_stage1_result &result,
													 ::std::uint_least8_t byte,
													 ::std::size_t offset) noexcept
{
	if (state.utf8_remaining == 0u)
	{
		if (byte < 0x80u)
		{
			return true;
		}

		state.utf8_sequence_offset = offset;
		if (byte >= 0xC2u && byte <= 0xDFu)
		{
			state.utf8_code_point = static_cast<::std::uint_least32_t>(byte & 0x1Fu);
			state.utf8_minimum = 0x80u;
			state.utf8_remaining = 1u;
			return true;
		}
		if (byte >= 0xE0u && byte <= 0xEFu)
		{
			state.utf8_code_point = static_cast<::std::uint_least32_t>(byte & 0x0Fu);
			state.utf8_minimum = 0x800u;
			state.utf8_remaining = 2u;
			return true;
		}
		if (byte >= 0xF0u && byte <= 0xF4u)
		{
			state.utf8_code_point = static_cast<::std::uint_least32_t>(byte & 0x07u);
			state.utf8_minimum = 0x10000u;
			state.utf8_remaining = 3u;
			return true;
		}
		return json_stage1_fail(result, json_stage1_errc::invalid_unicode, offset);
	}

	if (byte < 0x80u || byte > 0xBFu)
	{
		return json_stage1_fail(result, json_stage1_errc::invalid_unicode, offset);
	}
	state.utf8_code_point = static_cast<::std::uint_least32_t>(
		(state.utf8_code_point << 6u) | static_cast<::std::uint_least32_t>(byte & 0x3Fu));
	--state.utf8_remaining;
	if (state.utf8_remaining != 0u)
	{
		return true;
	}

	auto const code_point{state.utf8_code_point};
	if (code_point < state.utf8_minimum ||
		(code_point >= 0xD800u && code_point <= 0xDFFFu) || code_point > 0x10FFFFu)
	{
		return json_stage1_fail(result, json_stage1_errc::invalid_unicode,
								state.utf8_sequence_offset);
	}
	return true;
}

inline constexpr bool json_stage1_record_ascii(json_stage1_state &state,
											   json_stage1_result &result,
											   ::std::uint_least8_t byte,
											   ::std::size_t offset)
{
	if (!json_stage1_validate_utf8_byte(state, result, byte, offset))
	{
		return false;
	}

	if (state.in_string)
	{
		if (byte < 0x20u)
		{
			return json_stage1_fail(result, json_stage1_errc::unescaped_control_character, offset);
		}
		if (byte == 0x22u && !state.backslash_odd)
		{
			state.in_string = false;
			state.backslash_odd = false;
			state.may_begin_pseudo = false;
			return true;
		}
		if (byte == 0x5Cu)
		{
			state.backslash_odd = !state.backslash_odd;
		}
		else
		{
			state.backslash_odd = false;
		}
		return true;
	}

	if (byte == 0x22u)
	{
		result.structurals.push_back(offset);
		state.in_string = true;
		state.backslash_odd = false;
		state.may_begin_pseudo = false;
	}
	else if (json_ascii_space(byte))
	{
		state.may_begin_pseudo = true;
	}
	else if (json_ascii_structural(byte))
	{
		result.structurals.push_back(offset);
		state.may_begin_pseudo = true;
	}
	else if (state.may_begin_pseudo)
	{
		result.structurals.push_back(offset);
		state.may_begin_pseudo = false;
	}
	return true;
}

/* A SIMD block with no candidate byte contains only printable, non-space ASCII
   bytes.  Such a block cannot change string state or UTF-8 state. */
inline constexpr bool json_stage1_record_plain_ascii(json_stage1_state &state,
													 json_stage1_result &result,
													 ::std::size_t first,
													 ::std::size_t last)
{
	if (first == last)
	{
		return true;
	}
	if (state.utf8_remaining != 0u)
	{
		return json_stage1_fail(result, json_stage1_errc::invalid_unicode, first);
	}
	if (state.in_string)
	{
		state.backslash_odd = false;
	}
	else if (state.may_begin_pseudo)
	{
		result.structurals.push_back(first);
		state.may_begin_pseudo = false;
	}
	return true;
}

template <::std::size_t n>
[[nodiscard]] inline ::fast_io::intrinsics::simd_vector<::std::uint_least8_t, n>
json_simd_splat(::std::uint_least8_t value) noexcept
{
	::std::uint_least8_t lanes[n]{};
	for (::std::size_t i{}; i != n; ++i)
	{
		lanes[i] = value;
	}
	::fast_io::intrinsics::simd_vector<::std::uint_least8_t, n> result{};
	result.load(lanes);
	return result;
}

template <::std::size_t n>
[[nodiscard]] inline constexpr ::std::uint_least64_t
json_stage1_lane_mask() noexcept
{
	static_assert(n != 0u && n <= 64u);
	constexpr auto digits{(::std::numeric_limits<::std::uint_least64_t>::digits)};
	static_assert(digits >= 64u);
	if constexpr (n == digits)
	{
		return (::std::numeric_limits<::std::uint_least64_t>::max)();
	}
	else
	{
		return (static_cast<::std::uint_least64_t>(1u) << n) - 1u;
	}
}

/*
For a bit vector q, the successive assignments

  q ^= q << 1; q ^= q << 2; q ^= q << 4; ...

form the inclusive prefix xor.  After the shift 2^k, bit i is the xor of
the last min(2^(k+1),i+1) original bits; induction on k proves that the six
steps below cover every prefix of a 64-lane block.  Complementing the result
inside the live lane mask composes the carried "already in string" bit.
*/
template <::std::size_t n>
[[nodiscard]] inline constexpr ::std::uint_least64_t
json_stage1_inclusive_prefix_xor(::std::uint_least64_t bits,
	bool carried) noexcept
{
	bits ^= bits << 1u;
	bits ^= bits << 2u;
	bits ^= bits << 4u;
	bits ^= bits << 8u;
	bits ^= bits << 16u;
	bits ^= bits << 32u;
	bits &= json_stage1_lane_mask<n>();
	if (carried)
	{
		bits ^= json_stage1_lane_mask<n>();
	}
	return bits;
}

/* Return the length of the maximal reverse-solidus run immediately before
   position.  Left alignment turns that suffix into leading one bits, so
   countl_one is exactly the run length.  position may equal n (the block end)
   but never exceeds 64. */
[[nodiscard]] inline constexpr ::std::size_t
json_stage1_reverse_solidus_run_before(::std::uint_least64_t reverse_solidi,
	::std::size_t position) noexcept
{
	constexpr auto digits{static_cast<::std::size_t>(
		(::std::numeric_limits<::std::uint_least64_t>::digits))};
	if (position == 0u)
	{
		return 0u;
	}
	auto const lower_mask{position == digits
		? (::std::numeric_limits<::std::uint_least64_t>::max)()
		: (static_cast<::std::uint_least64_t>(1u) << position) - 1u};
	auto const aligned{static_cast<::std::uint_least64_t>(
		(reverse_solidi & lower_mask) << (digits - position))};
	return static_cast<::std::size_t>(::std::countl_one(aligned));
}

/*
Compute the byte positions preceded by an odd reverse-solidus run without a
lane loop.  Let B contain every reverse solidus and S = B & ~(B << 1) every
run start.  Adding one selected start bit to B carries through precisely that
run and stops at its first zero, hence `(B + S_selected) & ~B` marks its end.
A run has odd length exactly when its start and end have opposite index
parity, so starts on even and odd lanes are added separately and their ends
are intersected with the opposite parity mask.  A carried odd run makes the
lane-zero run behave as though it started at odd lane -1; moving that start
from the even to the odd set composes the carry.  If lane zero is not a
reverse solidus, the carried run ends at lane zero directly.

The additions for distinct runs cannot interfere: each carry terminates at a
zero separating it from the next run.  Bits beyond the live lane mask are
discarded, leaving a run which reaches the block end for the explicit state
carry below.
*/
template <::std::size_t n>
[[nodiscard]] inline constexpr ::std::uint_least64_t
json_stage1_odd_reverse_solidus_end_bits(
	::std::uint_least64_t reverse_solidi, bool carried_odd) noexcept
{
	auto const lane_mask{json_stage1_lane_mask<n>()};
	reverse_solidi &= lane_mask;
	constexpr auto even_lanes{
		(::std::numeric_limits<::std::uint_least64_t>::max)() / 3u};
	constexpr auto odd_lanes{static_cast<::std::uint_least64_t>(~even_lanes)};
	auto const starts{reverse_solidi & ~(reverse_solidi << 1u) & lane_mask};
	auto even_starts{starts & even_lanes};
	auto odd_starts{starts & odd_lanes};
	if (carried_odd && (reverse_solidi & 1u) != 0u)
	{
		even_starts &= ~static_cast<::std::uint_least64_t>(1u);
		odd_starts |= 1u;
	}
	auto const ends_from_even_starts{
		(reverse_solidi + even_starts) & ~reverse_solidi & lane_mask};
	auto const ends_from_odd_starts{
		(reverse_solidi + odd_starts) & ~reverse_solidi & lane_mask};
	auto result{(ends_from_even_starts & odd_lanes) |
		(ends_from_odd_starts & even_lanes)};
	if (carried_odd && (reverse_solidi & 1u) == 0u)
	{
		result |= 1u;
	}
	return result;
}

/*
A quote is escaped iff it is inside a string and the maximal immediately
preceding reverse-solidus run has odd length.  If that run reaches lane zero,
its parity is xor-composed with the carried run parity; otherwise the local
run is complete.  Processing only quote bits in increasing order is enough:
an escaped quote leaves the string state unchanged and every other quote
toggles it.  This removes whitespace and punctuation from the sequential
part of stage one without changing cross-block escape semantics.
*/
template <::std::size_t n>
[[nodiscard]] inline constexpr ::std::uint_least64_t
json_stage1_unescaped_quote_bits(::std::uint_least64_t quotes,
	::std::uint_least64_t reverse_solidi, bool initially_in_string,
	bool carried_reverse_solidus_odd) noexcept
{
	bool in_string{initially_in_string};
	::std::uint_least64_t unescaped{};
	while (quotes != 0u)
	{
		auto const position{static_cast<::std::size_t>(::std::countr_zero(quotes))};
		auto const run{json_stage1_reverse_solidus_run_before(
			reverse_solidi, position)};
		bool odd{(run & 1u) != 0u};
		if (run == position)
		{
			odd = odd != carried_reverse_solidus_odd;
		}
		if (!(in_string && odd))
		{
			auto const quote_bit{static_cast<::std::uint_least64_t>(1u) << position};
			unescaped |= quote_bit;
			in_string = !in_string;
		}
		quotes &= quotes - 1u;
	}
	return unescaped & json_stage1_lane_mask<n>();
}

template <::std::size_t n>
inline void json_stage1_append_structural_bits(json_stage1_result &result,
	::std::uint_least64_t bits, ::std::size_t offset)
{
	while (bits != 0u)
	{
		auto const position{static_cast<::std::size_t>(::std::countr_zero(bits))};
		result.structurals.push_back(offset + position);
		bits &= bits - 1u;
	}
}

template <::std::size_t n, typename char_type>
inline bool json_stage1_scan_simd_blocks(char_type const *first, ::std::size_t count,
										 json_stage1_state &state,
										 json_stage1_result &result,
										 ::std::size_t &offset)
{
	using simd_type = ::fast_io::intrinsics::simd_vector<::std::uint_least8_t, n>;
	static_assert(n != 0u && n <= 64u);

	auto const quote{json_simd_splat<n>(0x22u)};
	auto const reverse_solidus{json_simd_splat<n>(0x5Cu)};
	auto const space{json_simd_splat<n>(0x20u)};
	auto const high_bit{json_simd_splat<n>(0x80u)};
	auto const left_brace{json_simd_splat<n>(0x7Bu)};
	auto const right_brace{json_simd_splat<n>(0x7Du)};
	auto const left_bracket{json_simd_splat<n>(0x5Bu)};
	auto const right_bracket{json_simd_splat<n>(0x5Du)};
	auto const comma{json_simd_splat<n>(0x2Cu)};
	auto const colon{json_simd_splat<n>(0x3Au)};
	auto const horizontal_tab{json_simd_splat<n>(0x09u)};
	auto const line_feed{json_simd_splat<n>(0x0Au)};
	auto const carriage_return{json_simd_splat<n>(0x0Du)};

	for (; count - offset >= n; offset += n)
	{
		simd_type bytes{};
		bytes.load(static_cast<void const *>(first + offset));

		auto const quote_mask{bytes == quote};
		auto const reverse_solidus_mask{bytes == reverse_solidus};
		auto const quote_or_slash{quote_mask | reverse_solidus_mask};
		auto const structural{(bytes == left_brace) | (bytes == right_brace) |
							  (bytes == left_bracket) | (bytes == right_bracket) | (bytes == comma) | (bytes == colon)};
		auto const whitespace{(bytes == space) | (bytes == horizontal_tab) |
							  (bytes == line_feed) | (bytes == carriage_return)};
		auto const control{bytes < space};
		auto const non_ascii{bytes >= high_bit};
		auto const exceptional{control | non_ascii};
		auto const candidates{quote_or_slash | structural | whitespace | exceptional};

		if (::fast_io::intrinsics::is_all_zeros(candidates))
		{
			if (!json_stage1_record_plain_ascii(state, result, offset, offset + n))
			{
				return false;
			}
			continue;
		}

		/* Invalid UTF-8 and controls need byte-order error precedence.  They are
		   rare in valid JSON, so retain the scalar-event path for only those
		   blocks instead of burdening every ASCII block with an error merge. */
		if (!::fast_io::intrinsics::is_all_zeros(exceptional))
		{
			auto candidate_bits{
				::fast_io::intrinsics::vector_mask_to_bitset(candidates)};
			::std::size_t local{};
			while (candidate_bits != 0u)
			{
				auto const event{static_cast<::std::size_t>(
					::std::countr_zero(candidate_bits))};
				if (!json_stage1_record_plain_ascii(
						state, result, offset + local, offset + event))
				{
					return false;
				}
				auto const byte{static_cast<::std::uint_least8_t>(
					first[offset + event])};
				if (!json_stage1_record_ascii(state, result, byte, offset + event))
				{
					return false;
				}
				local = event + 1u;
				candidate_bits &= candidate_bits - 1u;
			}
			if (!json_stage1_record_plain_ascii(
					state, result, offset + local, offset + n))
			{
				return false;
			}
			continue;
		}

		if (state.utf8_remaining != 0u)
		{
			return json_stage1_fail(result,
				json_stage1_errc::invalid_unicode, offset);
		}

		auto const quote_bits{
			::fast_io::intrinsics::vector_mask_to_bitset(quote_mask)};
		auto const reverse_solidus_bits{
			::fast_io::intrinsics::vector_mask_to_bitset(reverse_solidus_mask)};
		auto const structural_bits{
			::fast_io::intrinsics::vector_mask_to_bitset(structural)};
		auto const whitespace_bits{
			::fast_io::intrinsics::vector_mask_to_bitset(whitespace)};

		auto const initially_in_string{state.in_string};
		auto const initially_reverse_solidus_odd{state.backslash_odd};
		auto const initially_may_begin_pseudo{state.may_begin_pseudo};
		auto const odd_reverse_solidus_ends{
			json_stage1_odd_reverse_solidus_end_bits<n>(
				reverse_solidus_bits, initially_reverse_solidus_odd)};
		auto const tentatively_escaped_quotes{
			quote_bits & odd_reverse_solidus_ends};
		auto unescaped_quotes{quote_bits & ~tentatively_escaped_quotes};
		auto inside_after{json_stage1_inclusive_prefix_xor<n>(
			unescaped_quotes, initially_in_string)};
		/* Reverse solidi outside strings do not escape an opening quote.  The
		   tentative mask is exact whenever every removed quote is inside its
		   tentative string mask.  Otherwise the first disagreement is exactly
		   such an outside quote, so the quote-only state machine supplies the
		   uncommon malformed-input fallback. */
		if ((tentatively_escaped_quotes & ~inside_after) != 0u) [[unlikely]]
		{
			unescaped_quotes = json_stage1_unescaped_quote_bits<n>(
				quote_bits, reverse_solidus_bits, initially_in_string,
				initially_reverse_solidus_odd);
			inside_after = json_stage1_inclusive_prefix_xor<n>(
				unescaped_quotes, initially_in_string);
		}
		auto const string_bytes{inside_after & ~unescaped_quotes};
		auto const outside_delimiters{
			(structural_bits | whitespace_bits) & ~string_bytes};
		auto const outside_structurals{structural_bits & ~string_bytes};
		auto const atom_bits{json_stage1_lane_mask<n>() &
			~(string_bytes | unescaped_quotes | structural_bits | whitespace_bits)};
		auto pseudo_predecessors{outside_delimiters << 1u};
		if (initially_may_begin_pseudo)
		{
			pseudo_predecessors |= 1u;
		}
		auto const pseudo_bits{atom_bits & pseudo_predecessors};
		auto const opening_quotes{unescaped_quotes & inside_after};
		json_stage1_append_structural_bits<n>(result,
			outside_structurals | opening_quotes | pseudo_bits, offset);
		state.in_string = ((inside_after >> (n - 1u)) & 1u) != 0u;
		if (state.in_string)
		{
			auto const run{json_stage1_reverse_solidus_run_before(
				reverse_solidus_bits, n)};
			state.backslash_odd = ((run & 1u) != 0u) !=
				(run == n && initially_reverse_solidus_odd);
			state.may_begin_pseudo = false;
		}
		else
		{
			state.backslash_odd = false;
			state.may_begin_pseudo =
				((outside_delimiters >> (n - 1u)) & 1u) != 0u;
		}
	}
	return true;
}

template <typename char_type>
inline constexpr bool json_stage1_scan_one_byte_scalar(char_type const *first, ::std::size_t count,
													   json_stage1_state &state,
													   json_stage1_result &result,
													   ::std::size_t offset = 0u)
{
	for (; offset != count; ++offset)
	{
		auto const byte{static_cast<::std::uint_least8_t>(first[offset])};
		if (!json_stage1_record_ascii(state, result, byte, offset))
		{
			return false;
		}
	}
	return true;
}

template <typename char_type>
[[nodiscard]] inline constexpr ::std::uint_least32_t json_code_unit_value(char_type value) noexcept
{
	using unsigned_type = ::std::make_unsigned_t<char_type>;
	return static_cast<::std::uint_least32_t>(static_cast<unsigned_type>(value));
}

template <typename char_type>
inline constexpr bool json_stage1_validate_wide_code_unit(char_type const *first,
														  ::std::size_t offset,
														  json_stage1_state &state,
														  json_stage1_result &result) noexcept
{
	auto const value{json_code_unit_value(first[offset])};
	if constexpr (sizeof(char_type) == 2u)
	{
		if (state.pending_high_surrogate)
		{
			if (value < 0xDC00u || value > 0xDFFFu)
			{
				return json_stage1_fail(result, json_stage1_errc::invalid_unicode, offset);
			}
			state.pending_high_surrogate = false;
			return true;
		}
		if (value >= 0xD800u && value <= 0xDBFFu)
		{
			state.pending_high_surrogate = true;
			state.high_surrogate_offset = offset;
			return true;
		}
		if (value >= 0xDC00u && value <= 0xDFFFu)
		{
			return json_stage1_fail(result, json_stage1_errc::invalid_unicode, offset);
		}
		return true;
	}
	else
	{
		if (value > 0x10FFFFu || (value >= 0xD800u && value <= 0xDFFFu))
		{
			return json_stage1_fail(result, json_stage1_errc::invalid_unicode, offset);
		}
		return true;
	}
}

template <typename char_type>
inline constexpr bool json_stage1_scan_wide_scalar(char_type const *first, ::std::size_t count,
												   json_stage1_state &state,
												   json_stage1_result &result)
{
	for (::std::size_t offset{}; offset != count; ++offset)
	{
		if (!json_stage1_validate_wide_code_unit(first, offset, state, result))
		{
			return false;
		}
		auto const value{json_code_unit_value(first[offset])};
		if (state.in_string)
		{
			if (value < 0x20u)
			{
				return json_stage1_fail(result, json_stage1_errc::unescaped_control_character, offset);
			}
			if (value == 0x22u && !state.backslash_odd)
			{
				state.in_string = false;
				state.backslash_odd = false;
				state.may_begin_pseudo = false;
			}
			else if (value == 0x5Cu)
			{
				state.backslash_odd = !state.backslash_odd;
			}
			else
			{
				state.backslash_odd = false;
			}
		}
		else if (value == 0x22u)
		{
			result.structurals.push_back(offset);
			state.in_string = true;
			state.backslash_odd = false;
			state.may_begin_pseudo = false;
		}
		else if (json_ascii_space(value))
		{
			state.may_begin_pseudo = true;
		}
		else if (json_ascii_structural(value))
		{
			result.structurals.push_back(offset);
			state.may_begin_pseudo = true;
		}
		else if (state.may_begin_pseudo)
		{
			result.structurals.push_back(offset);
			state.may_begin_pseudo = false;
		}
	}
	return true;
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr json_stage1_result
json_build_structural_index(char_type const *first, char_type const *last)
{
	json_stage1_result result{};
	if (first == last)
	{
		return result;
	}
	auto const count{static_cast<::std::size_t>(last - first)};
	json_stage1_state state{};

	if constexpr (sizeof(char_type) == 1u)
	{
		::std::size_t offset{};
		if (!::std::is_constant_evaluated())
		{
			constexpr auto native_mask_width{
				::fast_io::intrinsics::optimal_simd_vector_run_with_cpu_instruction_size_with_mask_to_bitset};
			constexpr auto width{native_mask_width != 0u
					? native_mask_width
					: ::fast_io::intrinsics::optimal_simd_vector_run_with_cpu_instruction_size};
			if constexpr (width != 0u)
			{
				/* Learn the actual structural density while scanning the first 4 KiB.
				   A fixed count/4 reservation is fast for ordinary object JSON but
				   wastes twice the input size on one huge string; count/8, conversely,
				   forces a reallocating copy for the common ~24% density.  The sample
				   is part of the real scan (never a second pass), and its extrapolation
				   is capped at count/4, so it removes that copy without increasing the
				   former worst-case reservation.  A 1/16 margin absorbs normal local
				   density variation; unusual later density still uses vector's normal
				   correctness-preserving growth. */
				constexpr ::std::size_t sample_bytes{4096u};
				auto const sample_limit{count > sample_bytes
					? sample_bytes - sample_bytes % width
					: count};
				if (count <= (::std::numeric_limits<::std::size_t>::max)() - 8u)
				{
					result.structurals.reserve(sample_limit / 4u + 8u);
				}
				if (!json_stage1_scan_simd_blocks<width>(
						first, sample_limit, state, result, offset))
				{
					return result;
				}
				if (offset != 0u && offset != count)
				{
					auto const observed{result.structurals.size()};
					auto const quotient{count / offset};
					auto const remainder{count % offset};
					auto const estimate{observed * quotient +
						observed * remainder / offset};
					auto const maximum_hint{count / 4u + 8u};
					auto hint{(::std::min)(estimate, maximum_hint)};
					auto const room{maximum_hint - hint};
					auto const margin{(::std::min)(room, hint / 16u + 16u)};
					hint += margin;
					if (hint > result.structurals.capacity())
					{
						result.structurals.reserve(hint);
					}
					if (!json_stage1_scan_simd_blocks<width>(
							first, count, state, result, offset))
					{
						return result;
					}
				}
			}
		}
		if (result.structurals.capacity() == 0u &&
			count <= (::std::numeric_limits<::std::size_t>::max)() - 8u)
		{
			result.structurals.reserve(count / 8u + 8u);
		}
		if (!json_stage1_scan_one_byte_scalar(
				first, count, state, result, offset))
		{
			return result;
		}
		if (state.utf8_remaining != 0u)
		{
			json_stage1_fail(result, json_stage1_errc::invalid_unicode, state.utf8_sequence_offset);
			return result;
		}
	}
	else
	{
		if (count <= (::std::numeric_limits<::std::size_t>::max)() - 8u)
		{
			result.structurals.reserve(count / 8u + 8u);
		}
		if (!json_stage1_scan_wide_scalar(
				first, count, state, result))
		{
			return result;
		}
		if (state.pending_high_surrogate)
		{
			json_stage1_fail(result, json_stage1_errc::invalid_unicode, state.high_surrogate_offset);
			return result;
		}
	}

	if (state.in_string)
	{
		json_stage1_fail(result, json_stage1_errc::unterminated_string, count);
	}
	return result;
}

} // namespace details

} // namespace fast_io::json
