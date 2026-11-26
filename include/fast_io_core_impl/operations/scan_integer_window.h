#pragma once

namespace fast_io
{
template <::std::integral Char>
struct basic_ibuffer_view_ref;
template <typename Buffer>
class basic_io_buffer_ref;

namespace details::io_buffer
{
/** @brief Identifies ordinary decimal integer scan targets whose separator rules are known by the buffer. */
template <typename T>
concept decimal_scan_integer =
	::std::same_as<T, short> || ::std::same_as<T, unsigned short> ||
	::std::same_as<T, int> || ::std::same_as<T, unsigned> ||
	::std::same_as<T, long> || ::std::same_as<T, unsigned long> ||
	::std::same_as<T, long long> || ::std::same_as<T, unsigned long long>;

// The resident strategy is declared before the STO implementation is included.
#if defined(__SSE4_1__) && (defined(__x86_64__) || defined(_M_X64)) && !defined(_M_ARM64EC) && !defined(__arm64ec__)
#if defined(_MSC_VER) && !defined(__clang__)
#define FAST_IO_STO_X86_SSE41
#elif FAST_IO_HAS_BUILTIN(__builtin_ia32_pmovmskb128) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pshufb128) && \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_pmaddubsw128) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pmaddwd128) && \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_packusdw128) && FAST_IO_HAS_BUILTIN(__builtin_ia32_packuswb128)
#define FAST_IO_STO_X86_SSE41
#endif
#endif

#if defined(FAST_IO_STO_X86_SSE41) && (!defined(_MSC_VER) || defined(__clang__)) && ('0' == 0x30)
inline constexpr bool has_packed_signed_decimal_window{true};
#else
inline constexpr bool has_packed_signed_decimal_window{false};
#endif

/**
 * @brief Selects the decimal digit-window implementation for small and wide integer magnitudes.
 * @details Character targets, bool and cv-qualified destinations retain their separate scan protocols.
 * A 31-bit signed magnitude additionally has a packed implementation when the compiler supplies its instructions.
 */
template <typename T>
concept bounded_decimal_target =
	::std::same_as<T, ::std::remove_cv_t<T>> && ::fast_io::details::non_character_integral<T> &&
	!::std::same_as<T, bool> && ::std::numeric_limits<T>::is_specialized &&
	(::std::numeric_limits<T>::digits <= 16 || ::std::numeric_limits<T>::digits >= 63 ||
	 (has_packed_signed_decimal_window && ::std::numeric_limits<T>::is_signed && ::std::numeric_limits<T>::digits == 31));

/** @brief Recognizes one complete decimal field in the given get area using the integer conversion primitives. */
template <bounded_decimal_target T>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char const *
scan_bounded_decimal(char const *first, char const *last, T &target) noexcept;

/**
 * @brief Accumulates a representable magnitude and publishes the target only at its delimiter.
 * @details The first digit is already known and nonzero. Limiting the magnitude to digits10
 * permits either sign without overflow. A single narrow target can use one further digit
 * after checking its magnitude against the signed or unsigned limit. The ten-unit window
 * covers the optional separator, sign, five digits and lookahead without another refill.
 */
template <bool Negative, bool CompleteNarrow, typename Char, typename T>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool finish_short_decimal(Char const *&cursor, Char const *p,
                                                                     ::std::uint_least32_t value, T &target) noexcept
{
	using U = ::std::make_unsigned_t<Char>;
	using accumulator = ::std::uint_least32_t;
	constexpr auto zero{static_cast<U>(::fast_io::char_literal_v<u8'0', Char>)};
	constexpr unsigned room{Negative ? 7u : 8u};
	constexpr unsigned bound{::std::numeric_limits<T>::digits10 < room ? ::std::numeric_limits<T>::digits10 : room};
	constexpr bool extra{CompleteNarrow && bound == 4u};
	for (unsigned i{1}; i <= bound + extra; ++i)
	{
		auto const digit{static_cast<accumulator>(static_cast<U>(*++p) - zero)};
		if (digit > 9u)
		{
			if constexpr (extra)
			{
				constexpr auto maximum{static_cast<accumulator>((::std::numeric_limits<T>::max)()) + Negative};
				if (i > bound && value > maximum) [[unlikely]]
				{
					return false;
				}
			}
			cursor = p;
			if constexpr (Negative && extra) target = static_cast<T>(-static_cast<::std::int_least32_t>(value));
			else if constexpr (Negative) target = static_cast<T>(-static_cast<T>(value));
			else target = static_cast<T>(value);
			return true;
		}
		value = value * 10u + digit;
	}
	return false;
}

/**
 * @brief Decodes a complete short decimal field in a window of ten readable code units.
 * @details One separator, a sign, seven digits and a delimiter fit in the same span as an
 * unsigned eight-digit field. Redundant leading zeroes and longer magnitudes use the canonical
 * grammar. Failed speculation preserves the target and the original cursor.
 */
template <bool CompleteNarrow = false, typename Char, typename T>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool short_decimal(Char const *&cursor, T &target) noexcept
{
	using U = ::std::make_unsigned_t<Char>;
	using accumulator = ::std::uint_least32_t;
	constexpr auto zero{static_cast<U>(::fast_io::char_literal_v<u8'0', Char>)};
	auto *p{cursor};
	if (*p == ::fast_io::char_literal_v<u8' ', Char> || *p == ::fast_io::char_literal_v<u8'\n', Char>) ++p;
	auto value{static_cast<accumulator>(static_cast<U>(*p) - zero)};
	if (value - 1u < 9u)
	{
		return finish_short_decimal<false, CompleteNarrow>(cursor, p, value, target);
	}
	if constexpr (::std::is_signed_v<T>)
	{
		if (*p == ::fast_io::char_literal_v<u8'-', Char>)
		{
			value = static_cast<accumulator>(static_cast<U>(*++p) - zero);
			if (value - 1u < 9u) return finish_short_decimal<true, CompleteNarrow>(cursor, p, value, target);
		}
	}
	// Zero must stand alone. A second digit may be a grammar error.
	if (value != 0u || static_cast<accumulator>(static_cast<U>(p[1]) - zero) <= 9u) return false;
	cursor = p + 1;
	target = 0;
	return true;
}
} // namespace details::io_buffer

namespace details::io_buffer
{
/**
 * @brief Parses a resident integer tuple and publishes its targets after every field reaches a delimiter.
 * @details The parser speculates on local values. A short window or an incomplete field leaves the public
 * cursor and targets available for the ordinary scanner. Publication follows argument order even if targets
 * alias one another. Inlining the tuple body is an implementation choice independent of these guarantees.
 *
 * @par Equivalence to sequential decimal scanning
 * Let p[0] be the public cursor. At field i, p[i] is the end returned by the preceding scanner.
 * A successful trial proves that the field at p[i] ends at p[i+1] inside the resident window
 * and gives the same value as the ordinary decimal grammar. By induction, at group success all
 * temporary values and the final cursor agree with sequential scanning. Built-in assignments
 * cannot throw. Committing them in argument order preserves repeated references. The disjointness
 * check prevents an earlier assignment from changing characters still needed by a later field.
 *
 * @par Refill and observable progress
 * A chunk boundary cannot terminate a decimal token. For example, the windows "12 3" and "4 "
 * spell 12 and 34. Every unsuccessful trial discards its private cursor and values. The ordinary
 * scanner then commits 12 before refilling and retains the second field's parser state across the
 * refill. An error in that field exposes the same committed prefix and error cursor. A count of
 * completed targets alone cannot resume an already consumed partial token. This probe always
 * returns at a whole-group boundary and never owns a continuation or publishes an async point.
 */
template <bool ProbeShort = true, typename Input, typename... Targets>
	requires(sizeof...(Targets) != 0 && ((decimal_scan_integer<Targets> || (!ProbeShort && bounded_decimal_target<Targets>)) && ...))
inline constexpr bool try_scan_integer_window(Input &input, Targets &...targets) noexcept
{
	using Char = typename Input::input_char_type;
	auto *begin{ibuffer_curr(input)};
	auto *end{ibuffer_end(input)};
	if (begin == end)
	{
		return false;
	}
	if constexpr (sizeof...(Targets) > 1u)
	{
		if (::std::is_constant_evaluated())
		{
			// Unrelated object addresses have no portable ordering during constant evaluation.
			return false;
		}
		// Committing an earlier target can change the spelling of a later field in the same storage.
		auto disjoint = [begin, end](auto &target) {
			auto *p{::fast_io::freestanding::addressof(target)};
			::std::ranges::less less{};
			return !less(static_cast<void const *>(begin), static_cast<void const *>(p + 1)) ||
				   !less(static_cast<void const *>(p), static_cast<void const *>(end));
		};
		if (!(disjoint(targets) && ...))
		{
			return false;
		}
	}
	Char const *cursor{begin};
	auto parse = [&](auto &value) {
		auto target{scan_alias_define(::fast_io::io_alias, value)};
		auto const field{scan_contiguous_define(io_reserve_type<Char, decltype(target)>, cursor, static_cast<Char const *>(end), target)};
		if (field.code != ::fast_io::parse_code::ok || field.iter == end)
		{
			return false;
		}
		cursor = field.iter;
		return true;
	};
#if defined(__linux__) && defined(__x86_64__) && defined(__clang__) && (__clang_major__ == 21 || __clang_major__ == 24) && \
	!defined(_MSC_VER)
	// Naming the closure allows a call-site inline hint for the measured small unsigned tuples.
	auto parse_tuple =
#else
	return
#endif
	[&](Targets... values) {
		bool done{};
		if constexpr (ProbeShort && sizeof(Char) <= sizeof(::std::uint_least32_t))
		{
			// One range proof covers the entire tuple and keeps per-digit bounds checks out of its loop.
			if (static_cast<::std::size_t>(end - begin) / 10u >= sizeof...(Targets))
			{
				done = (::fast_io::details::io_buffer::short_decimal(cursor, values) && ...);
			}
		}
		if (!done)
		{
			// Any failed speculation restarts the canonical grammar from the original public cursor.
			cursor = begin;
			if (!(parse(values) && ...))
			{
				return false;
			}
		}
		// Commit aliased targets in the same left-to-right order as the ordinary scanner.
		((targets = values), ...);
		ibuffer_set_curr(input, begin + (cursor - begin));
		return true;
	}
#if defined(__linux__) && defined(__x86_64__) && defined(__clang__) && (__clang_major__ == 21 || __clang_major__ == 24) && \
	!defined(_MSC_VER)
	;
#if __clang_major__ == 21
	constexpr bool inline_tuple{2u <= sizeof...(Targets) && sizeof...(Targets) <= 4u};
#else
	constexpr bool inline_tuple{sizeof...(Targets) == 2u && ::std::same_as<Char, char>};
#endif
	if constexpr (inline_tuple && (::std::same_as<Targets, unsigned> && ...))
	{
		// The small-pack specialization keeps the resident cursor in registers across the field commits.
		[[clang::always_inline]] return parse_tuple(Targets{}...);
	}
	return parse_tuple(Targets{}...);
#else
	(Targets{}...);
#endif
}
} // namespace details::io_buffer

namespace details::io_buffer
{
/** @brief Requires public integer semantics and excludes mutex and whole-pack status interception. */
template <typename Input, typename... Targets>
concept plain_integer_window =
	(sizeof...(Targets) != 0 &&
	 !::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<Input> &&
	 !::fast_io::operations::decay::defines::has_status_scan_define<Input,
																	::std::remove_reference_t<decltype(::fast_io::io_scan_forward<typename Input::input_char_type>(
																		::fast_io::io_scan_alias(::std::declval<Targets &>())))> &...>);

/** @brief Admits transparent byte views and the library's refillable input buffer to the single-field transaction. */
template <typename Input>
inline constexpr bool builtin_decimal_window =
	::std::same_as<Input, ::fast_io::basic_ibuffer_view_ref<char>>;

template <typename Buffer>
inline constexpr bool builtin_decimal_window<::fast_io::basic_io_buffer_ref<Buffer>> =
	::std::same_as<typename Buffer::input_char_type, char>;

/** @brief Publishes the get pointer only after the bounded parser has read the complete field. */
template <typename Input, bounded_decimal_target T>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool try_scan_resident_integral(Input &input, T &target) noexcept
{
	auto *first{ibuffer_curr(input)};
	auto const *next{scan_bounded_decimal(first, ibuffer_end(input), target)};
	if (next == nullptr)
	{
		return false;
	}
	ibuffer_set_curr(input, first + (next - first));
	return true;
}

/** @brief Parses a short terminal unsigned field within its readable extent. */
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char const *
scan_small_terminal_unsigned(char const *first, char const *last, unsigned &target) noexcept;

/** @brief Selects an optional eight-digit parser for a proven readable decimal window. */
template <typename Char>
inline constexpr auto resident_decimal_eight_parser{nullptr};

/**
 * @brief Commits one delimited 32-bit unsigned decimal in the current input window.
 * @details Whitespace and digits are inspected through a private cursor. Eleven readable units at the
 * first digit cover ten digits and a delimiter. After nine digits the partial value is at most
 * 999999999. The tenth is admitted only when 10 * value + digit fits the destination and is final.
 * A leading zero is complete only at a delimiter. These checks are exactly the ordinary unsigned
 * decimal grammar on a successful transaction. On a miss the original cursor and destination
 * remain available to the state machine, including when the input ends immediately after a digit.
 */
template <typename Input>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool try_scan_resident_unsigned(Input &in, unsigned &target) noexcept
{
	auto *p{ibuffer_curr(in)};
	auto const last{ibuffer_end(in)};
	if (p == last || last - p < 11)
	{
		return false;
	}
	constexpr unsigned zero{static_cast<unsigned char>(::fast_io::char_literal_v<u8'0', char>)};
	auto digit = [](char ch) { return static_cast<unsigned>(static_cast<unsigned char>(ch)) - zero; };
	auto value{digit(*p)};
	if (value > 9u)
	{
		// Once fewer than eleven bytes remain, the untouched window goes to the ordinary scanner.
		auto const first_short{last - 10};
		while (::fast_io::char_category::is_c_space(*p))
		{
			if (++p == first_short)
			{
				return false;
			}
		}
		value = digit(*p);
		if (value > 9u)
		{
			return false;
		}
	}
	if (value == 0u)
	{
		// The ordinary grammar rejects a redundant leading zero.
		if (digit(p[1]) <= 9u)
		{
			return false;
		}
		++p;
	}
	else
	{
		constexpr auto parse_eight{resident_decimal_eight_parser<typename Input::input_char_type>};
		if constexpr (!::std::same_as<::std::remove_cv_t<decltype(parse_eight)>, ::std::nullptr_t>)
		{
			FAST_IO_IF_NOT_CONSTEVAL
			{
				::std::uint_least64_t magnitude{};
				if (digit(p[7]) <= 9u && parse_eight(p, magnitude))
				{
					// The packed leaf proves eight leading digits. The original window
					// proof covers another two digits and the required delimiter.
					auto const *last_possible{p + 10u};
					p += 8u;
					for (; p != last_possible && digit(*p) <= 9u; ++p)
					{
						magnitude = magnitude * 10u + digit(*p);
					}
					if (digit(*p) <= 9u || magnitude > ::std::numeric_limits<unsigned>::max())
					{
						return false;
					}
					target = static_cast<unsigned>(magnitude);
					ibuffer_set_curr(in, p);
					return true;
				}
			}
		}
		for (unsigned count{1}; count != 9u; ++count)
		{
			auto const next{digit(*++p)};
			if (next > 9u)
			{
				target = value;
				ibuffer_set_curr(in, p);
				return true;
			}
			value = value * 10u + next;
		}
		auto const next{digit(*++p)};
		if (next <= 9u)
		{
			constexpr unsigned maximum{::std::numeric_limits<unsigned>::max()};
			if (value > maximum / 10u || (value == maximum / 10u && next > maximum % 10u) || digit(p[1]) <= 9u)
			{
				return false;
			}
			value = value * 10u + next;
			++p;
		}
	}
	target = value;
	ibuffer_set_curr(in, p);
	return true;
}
} // namespace details::io_buffer

/** @brief Parses ordinary integer lvalues in a borrowed terminal span, independently of padding or character width. */
template <borrowed_terminal_input Input, details::io_buffer::decimal_scan_integer... Targets>
	requires details::io_buffer::plain_integer_window<Input, Targets...>
inline constexpr bool input_stream_try_scan_define(Input &input, Targets &...targets) noexcept
{
	return details::io_buffer::try_scan_integer_window(input, targets...);
}

} // namespace fast_io
