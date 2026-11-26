#pragma once

namespace fast_io::details
{

enum class decimal_scan_phase : unsigned char
{
	start,
	number,
	exponent_sign,
	exponent,
	word,
	payload,
	closed_payload
};

template <typename Components>
struct decimal_token_state
{
	Components value{};
	::std::int_least64_t point_shift{};
	::std::int_least64_t exponent{};
	decimal_scan_phase phase{};
	unsigned word_size{};
	unsigned payload_length{};
	bool negative_exponent{};
	bool fraction{};
	bool any_digit{};
	bool nonzero{};
	bool sticky{};
	bool exponent_digit{};
	bool numeric_payload{true};
	bool snan_payload{true};
};

template <unsigned width>
using decimal_scan_state = decimal_token_state<decimal_components<width>>;

template <decimal_interchange_value T, ::fast_io::manipulators::scalar_flags flags, bool explicit_precision>
inline constexpr parse_code decimal_scan_finish(decimal_scan_state<decimal_interchange_binding<T>::bits> &state,
												T &out, ::std::size_t precision) noexcept
{
	using layout = decimal_interchange_binding<T>;
	if (state.phase == decimal_scan_phase::start)
	{
		return parse_code::end_of_file;
	}
	auto &value{state.value};
	if (value.category != decimal_class::finite)
	{
		if (state.phase == decimal_scan_phase::payload)
		{
			return parse_code::invalid;
		}
		auto const complete{value.category == decimal_class::infinity ? state.word_size == 3u || state.word_size == 8u : value.category == decimal_class::quiet_nan ? state.word_size == 3u
																																									: state.word_size == 4u || state.phase == decimal_scan_phase::closed_payload};
		if (!complete)
		{
			return parse_code::invalid;
		}
		if constexpr (!flags.nan_parse_sign)
		{
			if (value.category != decimal_class::infinity)
			{
				value.negative = false;
			}
		}
		out = decimal_encode<T>(value);
		return parse_code::ok;
	}
	if (!state.any_digit)
	{
		return parse_code::invalid;
	}
	if constexpr (flags.floating == ::fast_io::manipulators::floating_format::scientific)
	{
		if (!state.exponent_digit)
		{
			return parse_code::invalid;
		}
	}
	if ((state.phase == decimal_scan_phase::exponent_sign || state.phase == decimal_scan_phase::exponent) && !state.exponent_digit)
	{
		return parse_code::invalid;
	}
	auto quantum{scan_decfloat_saturating_add(state.point_shift, state.negative_exponent ? -state.exponent : state.exponent)};
	if (!state.nonzero)
	{
		value.exponent = quantum < layout::minimum_quantum ? layout::minimum_quantum : layout::maximum_quantum < quantum ? layout::maximum_quantum
																														 : static_cast<int>(quantum);
		out = decimal_encode<T>(value);
		return parse_code::ok;
	}

	// All representable quantum exponents lie within +/-6176. Saturating after
	// combining the lexical decimal point with its exponent permits very long
	// mantissas to cancel a large exponent exactly.
	value.exponent = quantum < -32000 ? -32000 : 32000 < quantum ? 32000
																 : static_cast<int>(quantum);
	auto const size{static_cast<int>(value.size)};
	auto keep{static_cast<int>(layout::precision)};
	auto const subnormal_keep{size + value.exponent - layout::minimum_quantum};
	if (subnormal_keep < keep)
	{
		keep = subnormal_keep;
	}
	bool precision_zero{};
	bool keep_quantum_on_carry{keep == subnormal_keep && keep < static_cast<int>(layout::precision)};
	if constexpr (explicit_precision)
	{
		auto const limit{precision < 64000u ? static_cast<int>(precision) : 64000};
		auto const requested{floating_precision_is_fractional<flags.precision>
								 ? size + value.exponent + limit
								 : (limit ? limit : 1)};
		precision_zero = requested <= 0 && requested <= subnormal_keep;
		keep_quantum_on_carry &= keep < requested;
		if (requested < keep)
		{
			keep = requested;
		}
	}
	// Precision, subnormal and destination-width restrictions are all decimal
	// grids. Rounding directly at the coarsest grid avoids two midpoint decisions.
	// A carry within the subnormal grid can retain its minimum quantum if the
	// resulting coefficient still fits.
	decimal_round(value, keep, state.sticky, flags.rounding, keep_quantum_on_carry);
	if (!value.digits[0])
	{
		if (!precision_zero)
		{
			return parse_code::overflow;
		}
		out = decimal_encode<T>(value);
		return parse_code::ok;
	}
	if (layout::maximum_quantum < value.exponent)
	{
		auto const extra{static_cast<unsigned>(value.exponent - layout::maximum_quantum)};
		if (extra <= layout::precision - value.size)
		{
			value.exponent = layout::maximum_quantum;
			for (unsigned i{}; i != extra; ++i)
			{
				value.digits[value.size++] = 0u;
			}
		}
		else
		{
			auto const direction{decimal_effective_rounding(flags.rounding)};
			bool const to_finite{direction == decimal_rounding::toward_zero ||
								 (direction == decimal_rounding::toward_minus_infinity && !value.negative) ||
								 (direction == decimal_rounding::toward_plus_infinity && value.negative)};
			if (!to_finite)
			{
				return parse_code::overflow;
			}
			value.size = layout::precision;
			value.exponent = layout::maximum_quantum;
			for (unsigned i{}; i != layout::precision; ++i)
			{
				value.digits[i] = 9u;
			}
		}
	}
	out = decimal_encode<T>(value);
	return parse_code::ok;
}

template <typename Components>
inline constexpr void decimal_scan_digit(decimal_token_state<Components> &state, unsigned digit) noexcept
{
	// After p+1 significant digits only the guard, sticky bit and radix
	// displacement matter. The integral and fractional tails update the
	// displacement in opposite ways and need no input-sized allocation.
	state.any_digit = true;
	if (state.value.size == sizeof(state.value.digits) / sizeof(state.value.digits[0]))
	{
		if (!state.fraction)
		{
			state.point_shift = scan_decfloat_saturating_add(state.point_shift, 1);
		}
		state.sticky |= digit != 0u;
		return;
	}
	if (state.fraction)
	{
		state.point_shift = scan_decfloat_saturating_add(state.point_shift, -1);
	}
	if (state.nonzero)
	{
		// GCC on i686 folds an unsigned subscript into a one-element bound
		// in constant evaluation. The cursor keeps the full array provenance.
		auto *out{state.value.digits + state.value.size++};
		*out = static_cast<unsigned char>(digit);
	}
	else if (digit)
	{
		state.nonzero = true;
		state.value.digits[0] = static_cast<unsigned char>(digit);
	}
}

/// @brief Advances the decimal token grammar and calls the destination's rounding operation at a delimiter.
template <::fast_io::manipulators::scalar_flags flags, unsigned payload_digits, ::std::integral Char, typename Components, typename Finish>
inline constexpr parse_result<Char const *> decimal_scan_feed(decimal_token_state<Components> &state,
	Char const *first, Char const *last, Finish finish) noexcept
{
	using phase = decimal_scan_phase;
	constexpr auto plus{::fast_io::char_literal_v<u8'+', Char>};
	constexpr auto minus{::fast_io::char_literal_v<u8'-', Char>};
	auto &value{state.value};
	while (first != last)
	{
		auto ch{*first};
		char8_t digit{};
		switch (state.phase)
		{
		case phase::start:
			first = scan_floating_context_skip_space<Char, flags.noskipws>(first, last);
			if (first == last)
			{
				return {last, parse_code::partial};
			}
			value.negative = *first == minus;
			if (value.negative || (flags.allow_leading_plus && *first == plus))
			{
				++first;
			}
			state.phase = phase::number;
			continue;
		case phase::number:
			if (scan_decfloat_decimal_digit(ch, digit))
			{
				do
				{
					decimal_scan_digit(state, digit);
				} while (++first != last && scan_decfloat_decimal_digit(*first, digit));
				continue;
			}
			if (!state.fraction && ch == ::fast_io::char_literal_v < flags.comma ? u8',' : u8'.', Char >)
			{
				state.fraction = true;
				break;
			}
			if (!state.fraction && !state.any_digit)
			{
				if (scan_hexfloat_caseless_equal<u8'i', u8'I'>(ch))
				{
					value.category = decimal_class::infinity;
				}
				else if (scan_hexfloat_caseless_equal<u8'n', u8'N'>(ch))
				{
					value.category = decimal_class::quiet_nan;
				}
				else if (scan_hexfloat_caseless_equal<u8's', u8'S'>(ch))
				{
					value.category = decimal_class::signaling_nan;
				}
				else
				{
					return {first, parse_code::invalid};
				}
				state.phase = phase::word;
				continue;
			}
			if constexpr (flags.floating != ::fast_io::manipulators::floating_format::fixed)
			{
				if (state.any_digit && scan_hexfloat_caseless_equal<u8'e', u8'E'>(ch))
				{
					state.phase = phase::exponent_sign;
					break;
				}
			}
			return {first, finish()};
		case phase::exponent_sign:
			state.phase = phase::exponent;
			if (ch == plus || ch == minus)
			{
				state.negative_exponent = ch == minus;
				break;
			}
			continue;
		case phase::exponent:
			if (!scan_decfloat_decimal_digit(ch, digit))
			{
				return {first, finish()};
			}
			state.exponent_digit = true;
			{
				constexpr auto limit{(::std::numeric_limits<::std::int_least64_t>::max)()};
				state.exponent = state.exponent <= (limit - digit) / 10 ? state.exponent * 10 + digit : limit;
			}
			break;
		case phase::word:
		{
			auto const *word{value.category == decimal_class::infinity ? u8"infinity" : value.category == decimal_class::signaling_nan ? u8"snan"
																																	   : u8"nan"};
			if (word[state.word_size] && scan_hexfloat_caseless_equal_ascii(ch, word[state.word_size]))
			{
				++state.word_size;
				break;
			}
			if constexpr (flags.nan_payload_scan != ::fast_io::manipulators::floating_nan_payload_scan::none)
			{
				if (value.category != decimal_class::infinity && !word[state.word_size] && ch == ::fast_io::char_literal_v<u8'(', Char>)
				{
					state.phase = phase::payload;
					break;
				}
			}
			if (scan_hexfloat_nan_sequence_char(ch))
			{
				return {first, parse_code::invalid};
			}
			return {first, finish()};
		}
		case phase::payload:
			if (ch == ::fast_io::char_literal_v<u8')', Char>)
			{
				if constexpr (flags.nan_payload_scan == ::fast_io::manipulators::floating_nan_payload_scan::preserve)
				{
					if (!state.numeric_payload)
					{
						value.size = 1u;
						value.digits[0] = 0u;
					}
					if (state.snan_payload && state.payload_length == 4u)
					{
						value.category = decimal_class::signaling_nan;
					}
				}
				state.phase = phase::closed_payload;
				break;
			}
			if (!scan_hexfloat_nan_sequence_char(ch))
			{
				return {first, parse_code::invalid};
			}
			if constexpr (flags.nan_payload_scan == ::fast_io::manipulators::floating_nan_payload_scan::preserve)
			{
				state.snan_payload &= state.payload_length < 4u && scan_hexfloat_caseless_equal_ascii(ch, u8"snan"[state.payload_length]);
				if (state.payload_length < 5u)
				{
					++state.payload_length;
				}
				if (state.numeric_payload && scan_decfloat_decimal_digit(ch, digit))
				{
					if (!value.digits[0])
					{
						value.digits[0] = digit;
					}
					else if (value.size < payload_digits)
					{
						value.digits[value.size++] = digit;
					}
					else
					{
						state.numeric_payload = false;
					}
				}
				else
				{
					state.numeric_payload = false;
				}
			}
			break;
		case phase::closed_payload:
			if (scan_hexfloat_nan_sequence_char(ch))
			{
				return {first, parse_code::invalid};
			}
			return {first, finish()};
		}
		++first;
	}
	return {last, parse_code::partial};
}

template <::std::integral Char, decimal_interchange_value T, ::fast_io::manipulators::scalar_flags flags,
		  bool explicit_precision>
inline constexpr parse_result<Char const *> decimal_scan_chunk(decimal_scan_state<decimal_interchange_binding<T>::bits> &state,
															   Char const *first, Char const *last, T &out, ::std::size_t precision) noexcept
{
	return decimal_scan_feed<flags,decimal_interchange_binding<T>::precision - 1u>(state,first,last,[&]() noexcept {
		return decimal_scan_finish<T,flags,explicit_precision>(state,out,precision);
	});
}

template <typename M>
struct decimal_scan_traits;

template <::fast_io::manipulators::scalar_flags options, decimal_interchange_value T>
struct decimal_scan_traits<::fast_io::manipulators::scalar_manip_t<options, T &>>
{
	using value_type = T;
	inline static constexpr auto flags{options};
	inline static constexpr bool precision{};
};
template <::fast_io::manipulators::scalar_flags options, decimal_interchange_value T>
struct decimal_scan_traits<::fast_io::manipulators::scalar_manip_precision_t<options, T &>>
{
	using value_type = T;
	inline static constexpr auto flags{options};
	inline static constexpr bool precision{true};
};

template <typename M>
concept decimal_scanner = requires { typename decimal_scan_traits<M>::value_type; };

template <decimal_scanner M>
using decimal_scanner_state = decimal_scan_state<decimal_interchange_binding<typename decimal_scan_traits<M>::value_type>::bits>;

template <decimal_scanner M>
inline constexpr ::std::size_t decimal_scan_precision(M value) noexcept
{
	if constexpr (decimal_scan_traits<M>::precision)
	{
		return value.precision;
	}
	else
	{
		return 0u;
	}
}

} // namespace fast_io::details

namespace fast_io
{

template <details::decimal_interchange_value T>
inline constexpr auto scan_alias_define(io_alias_t, T &out) noexcept
{
	return manipulators::scalar_manip_t<manipulators::floating_point_default_scalar_flags, T &>{out};
}

template <::std::integral Char, details::decimal_scanner M>
inline constexpr auto scan_context_type(io_reserve_type_t<Char, M>) noexcept
{
	return io_type_t<details::decimal_scanner_state<M>>{};
}

template <::std::integral Char, details::decimal_scanner M>
inline constexpr parse_result<Char const *> scan_context_define(io_reserve_type_t<Char, M>, details::decimal_scanner_state<M> &state,
																Char const *first, Char const *last, M value) noexcept
{
	using trait = details::decimal_scan_traits<M>;
	return details::decimal_scan_chunk<Char, typename trait::value_type, trait::flags, trait::precision>(state, first, last, value.reference, details::decimal_scan_precision(value));
}

template <::std::integral Char, details::decimal_scanner M>
inline constexpr parse_code scan_context_eof_define(io_reserve_type_t<Char, M>, details::decimal_scanner_state<M> &state, M value) noexcept
{
	using trait = details::decimal_scan_traits<M>;
	return details::decimal_scan_finish<typename trait::value_type, trait::flags, trait::precision>(state, value.reference, details::decimal_scan_precision(value));
}

template <::std::integral Char, details::decimal_scanner M>
inline constexpr parse_result<Char const *> scan_contiguous_define(io_reserve_type_t<Char, M> tag, Char const *first,
																   Char const *last, M value) noexcept
{
	details::decimal_scanner_state<M> state{};
	auto result{scan_context_define(tag, state, first, last, value)};
	if (result.code == parse_code::partial)
	{
		result.code = scan_context_eof_define(tag, state, value);
	}
	return result;
}

} // namespace fast_io
