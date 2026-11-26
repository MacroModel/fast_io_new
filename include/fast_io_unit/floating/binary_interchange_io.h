#pragma once

#include "binary_interchange_convert.h"
#include "../decimal/print.h"
#include "../decimal/scan.h"

namespace fast_io::details
{
template <typename T>
concept software_binary_value = is_software_binary<T>;

template <binary_format F>
using binary_io_native = ::std::conditional_t<F == binary_format::binary16, float16,
											  ::std::conditional_t<F == binary_format::bfloat16, bfloat16, ::std::conditional_t<F == binary_format::binary32, float32, ::std::conditional_t<F == binary_format::binary64, float64, float128>>>>;

/// @brief Tests the actual field layout of a native I/O backend, including its significand storage.
template <software_binary_value T>
inline constexpr bool binary_io_native_available = scan_decfloat_supported_floating_point<binary_io_native<T::format>> &&
												   sizeof(T) == sizeof(binary_io_native<T::format>);

template <software_binary_value T>
struct interchange_floating_traits<T>
{
	static constexpr int radix{2};
	static constexpr bool negative(T v) noexcept
	{
		return binary_interchange_parts(v).negative;
	}
	static constexpr bool finite(T v) noexcept
	{
		return binary_interchange_parts(v).category == decimal_class::finite;
	}
	static constexpr int adjusted_exponent(T value) noexcept
	{
		auto parts{binary_interchange_parts(value)};
		if (parts.category != decimal_class::finite || !binary_words_nonzero(parts.coefficient))
		{
			return 0;
		}
		auto d{binary_interchange_exact_decimal(value)};
		return d.exponent + static_cast<int>(d.size) - 1;
	}
};

template <binary_format F>
struct binary_text_components : binary_interchange_decimal_digits<F>
{
	bool negative{};
	decimal_class category{};
};

enum class binary_text_form
{
	shortest,
	exact,
	precision,
	range
};

template <typename M>
struct binary_text_policy;
template <manipulators::scalar_flags Flags, software_binary_value T>
struct binary_text_policy<manipulators::scalar_manip_t<Flags, T>>
{
	using value_type = T;
	static constexpr auto flags{Flags};
	static constexpr auto form{binary_text_form::shortest};
};
template <manipulators::scalar_flags Flags, software_binary_value T>
struct binary_text_policy<manipulators::exact_decimal_manip_t<Flags, T>> : binary_text_policy<manipulators::scalar_manip_t<Flags, T>>
{
	static constexpr auto form{binary_text_form::exact};
};
template <manipulators::scalar_flags Flags, software_binary_value T>
struct binary_text_policy<manipulators::scalar_manip_precision_t<Flags, T>> : binary_text_policy<manipulators::scalar_manip_t<Flags, T>>
{
	static constexpr auto form{binary_text_form::precision};
};
template <manipulators::scalar_flags Flags, software_binary_value T>
struct binary_text_policy<manipulators::floating_scalar_precision_range_manip_t<Flags, T>> : binary_text_policy<manipulators::scalar_manip_t<Flags, T>>
{
	static constexpr auto form{binary_text_form::range};
};
template <typename M>
concept binary_text_field = requires { typename binary_text_policy<M>::value_type; };

/// @brief Binds identical-format raw bits to the existing native presentation CPO.
template <binary_text_field M>
inline constexpr auto binary_native_field(M field) noexcept
{
	using P = binary_text_policy<M>;
	using N = binary_io_native<P::value_type::format>;
	auto bits{field.reference.to_bits()};
	if constexpr (::std::same_as<decltype(bits), binary128_words>)
	{
		bits = binary_native_word_order(bits);
	}
	else
	{
		using L = binary_interchange_layout<P::value_type::format>;
		bits = fp_translate_nan_encoding<L::mbits, L::ebits>(bits);
	}
	if constexpr (floating_scalar_requires_integer_proxy<N>)
	{
		if constexpr (P::form == binary_text_form::precision)
		{
			return manipulators::floating_scalar_field_manip_precision_t<P::flags, N>{bits, field.precision};
		}
		else if constexpr (P::form == binary_text_form::range)
		{
			return manipulators::floating_scalar_field_precision_range_manip_t<P::flags, N>{bits, field.minimum_precision, field.maximum_precision};
		}
		else if constexpr (P::form == binary_text_form::exact)
		{
			return manipulators::exact_decimal_field_manip_t<P::flags, N>{bits};
		}
		else
		{
			return manipulators::floating_scalar_field_manip_t<P::flags, N>{bits};
		}
	}
	else
	{
		auto n{::std::bit_cast<N>(bits)};
		if constexpr (P::form == binary_text_form::precision)
		{
			return manipulators::scalar_manip_precision_t<P::flags, N>{n, field.precision};
		}
		else if constexpr (P::form == binary_text_form::range)
		{
			return manipulators::floating_scalar_precision_range_manip_t<P::flags, N>{n, field.minimum_precision, field.maximum_precision};
		}
		else if constexpr (P::form == binary_text_form::exact)
		{
			return manipulators::exact_decimal_manip_t<P::flags, N>{n};
		}
		else
		{
			return manipulators::scalar_manip_t<P::flags, N>{n};
		}
	}
}

/// @brief Selects the shortest significand that converts back to the original bits.
/// @details At each decimal precision the rounding cell is an interval. Only the two grid
/// points adjacent to the exact value can be its closest representative. The first successful
/// precision is minimal. Each grid is contained in the next finer grid, so binary search
/// locates that precision. At equal distance the even decimal significand wins.
template <decimal_rounding R, software_binary_value T>
inline constexpr void binary_text_shorten(binary_text_components<T::format> &value, T source) noexcept
{
	if (value.size < 2u)
	{
		return;
	}
	constexpr unsigned limit{3u + binary_interchange_layout<T::format>::mbits * 30103u / 100000u};
	struct candidate
	{
		unsigned char digits[limit + 1u]{};
		::std::size_t size{};
		int exponent{};
		bool negative{};
	};
	candidate prefix{};
	prefix.negative = value.negative;
	prefix.size = value.size < limit + 1u ? value.size : limit + 1u;
	prefix.exponent = value.exponent + static_cast<int>(value.size - prefix.size);
	for (::std::size_t i{}; i != prefix.size; ++i)
	{
		prefix.digits[i] = value.digits[i];
	}
	bool sticky{};
	for (auto i{prefix.size}; i != value.size; ++i)
	{
		sticky |= value.digits[i] != 0;
	}
	auto target{binary_interchange_words(source)};
	candidate best{};
	auto probe = [&](unsigned count) {
		auto rounded = [&](decimal_rounding direction) {
			auto c{prefix};
			decimal_round(c, static_cast<int>(count), sticky, direction);
			exact_precision_trim(c);
			return c;
		};
		auto accept = [&](candidate const &c) {
			auto bits{binary_interchange_words(binary_interchange_from_decimal<T, R>(c, value.negative))};
			if (bits.low == target.low && bits.high == target.high)
			{
				best = c;
				return true;
			}
			return false;
		};
		auto lower{rounded(decimal_rounding::toward_zero)};
		auto nearest{rounded(decimal_rounding::nearest_to_even)};
		if (accept(nearest))
		{
			return true;
		}
		// If the nearer grid point failed, the other adjacent grid point is
		// the only remaining candidate in this interval. Rounding a carry can
		// change both the significand length and its exponent.
		bool lower_is_nearest{lower.size == nearest.size && lower.exponent == nearest.exponent};
		for (::std::size_t i{}; lower_is_nearest && i != lower.size; ++i)
		{
			lower_is_nearest = lower.digits[i] == nearest.digits[i];
		}
		return accept(lower_is_nearest ? rounded(decimal_rounding::away_from_zero) : lower);
	};
	unsigned low{1u}, high{value.size <= limit ? static_cast<unsigned>(value.size) : limit};
	while (low < high)
	{
		auto middle{low + (high - low) / 2u};
		if (probe(middle))
		{
			high = middle;
		}
		else
		{
			low = middle + 1u;
		}
	}
	if (!best.size && !probe(low))
	{
		return;
	}
	value.size = best.size;
	value.exponent = best.exponent;
	for (::std::size_t i{}; i != best.size; ++i)
	{
		value.digits[i] = best.digits[i];
	}
}

/// @brief Computes the finite decimal rendering after normalizing the software format's own precision.
template <binary_text_field M>
inline constexpr auto binary_text_plan(M field) noexcept
{
	using P = binary_text_policy<M>;
	using T = typename P::value_type;
	struct plan_type
	{
		binary_text_components<T::format> value;
		decimal_notation layout;
	};
	plan_type plan{};
	auto &v{plan.value};
	auto const parts{binary_interchange_parts(field.reference)};
	v.negative = parts.negative;
	v.category = parts.category;
	if (parts.category != decimal_class::finite)
	{
		return plan;
	}
	static_cast<binary_interchange_decimal_digits<T::format> &>(v) = binary_interchange_exact_decimal(field.reference);
	if constexpr (P::form == binary_text_form::shortest)
	{
		binary_text_shorten<P::flags.rounding>(v, field.reference);
	}
	if constexpr (P::form == binary_text_form::range)
	{
		auto short_value{v};
		binary_text_shorten<P::flags.rounding>(short_value, field.reference);
		if (make_floating_precision_range_plan(short_value.size, field.minimum_precision, field.maximum_precision).shortest)
		{
			v = short_value;
			plan.layout = decimal_notation_for<P::flags, decimal_print_mode::shortest>(v, 0, 0);
		}
		else
		{
			auto p{make_floating_precision_range_plan(short_value.size, field.minimum_precision, field.maximum_precision)};
			if (p.preserve)
			{
				constexpr auto f{floating_precision_mani_flags_cache<P::flags, manipulators::floating_precision::significant_preserve_trailing_zero>};
				plan.layout = decimal_notation_for<f, decimal_print_mode::precision>(v, p.precision, 0);
			}
			else
			{
				plan.layout = decimal_notation_for<P::flags, decimal_print_mode::precision>(v, p.precision, 0);
			}
		}
	}
	else if constexpr (P::form == binary_text_form::precision)
	{
		plan.layout = decimal_notation_for<P::flags, decimal_print_mode::precision>(v, field.precision, 0);
	}
	else
	{
		plan.layout = decimal_notation_for<P::flags, decimal_print_mode::shortest>(v, 0, 0);
	}
	return plan;
}

/// @brief Presents binary hexadecimal fields directly from their interchange words.
/// @details The leading nibble is zero for subnormals. Every remaining nibble comes from
/// the original fraction, including the bits below binary64 precision in binary128.
template <::std::integral C, binary_text_field M>
inline constexpr C *binary_hex_write(C *out, M field) noexcept
{
	using P = binary_text_policy<M>;
	using L = binary_interchange_layout<P::value_type::format>;
	constexpr auto f{P::flags};
	auto words{binary_interchange_words(field.reference)};
	auto fraction{decimal_low_bits(words, L::mbits)};
	auto exponent{decimal_field(words, L::mbits, L::ebits)};
	bool negative{decimal_field(words, L::mbits + L::ebits, 1u) != 0};
	if (exponent == (1u << L::ebits) - 1u)
	{
		// Two summary bits distinguish infinity, signaling, and quiet NaNs with/without a payload.
		auto mantissa{(decimal_field(fraction, L::mbits - 1u, 1u) << 1u) |
					  static_cast<unsigned>(binary_words_nonzero(decimal_low_bits(fraction, L::mbits - 1u)))};
		return prsv_fp_nan_impl<f.showpos, f.uppercase, f.nan_show_sign, f.nan_show_type, 2u>(out, mantissa, negative);
	}
	out = print_rsv_fp_sign_impl<f.showpos>(out, negative);
	if constexpr (f.showbase)
	{
		out = print_reserve_show_base_impl<16u, f.uppercase_showbase, false>(out);
	}
	constexpr unsigned nibbles{(L::mbits + 3u) / 4u};
	auto aligned{binary_words_shift(fraction, static_cast<int>(nibbles * 4u - L::mbits))};
	int power{static_cast<int>(exponent ? exponent : 1u) - L::bias};
	if (!exponent && !binary_words_nonzero(fraction))
	{
		power = 0;
	}
	auto digits = [&](::std::size_t index) {
		return index ? decimal_field(aligned, static_cast<unsigned>((nibbles - index) * 4u), 4u) : static_cast<unsigned>(exponent != 0);
	};
	auto round = [&](unsigned kept, unsigned guard, bool sticky) {
		return decimal_increment(f.rounding, negative, guard < 8u ? 0u : guard == 8u ? 5u
																					 : 6u,
								 sticky || (guard != 0u && guard < 8u), (kept & 1u) != 0u);
	};
	constexpr ::std::size_t count{nibbles + 1u};
	if constexpr (P::form == binary_text_form::precision)
	{
		auto total{field.precision};
		if constexpr (floating_precision_is_fractional<f.precision>)
		{
			total = intrinsics::add_or_overflow_die(total, ::std::size_t{1u});
		}
		return print_hexfloat_digits<count, 5u, f.uppercase, f.uppercase_e, f.comma, f.precision>(out, exponent != 0, power, total ? total : 1u, digits, round);
	}
	else
	{
		return print_hexfloat_digits<count, 5u, f.uppercase, f.uppercase_e, f.comma, manipulators::floating_precision::significant>(out, exponent != 0, power, count, digits, round);
	}
}

template <typename M>
struct binary_input_policy;
template <manipulators::scalar_flags Flags, software_binary_value T>
struct binary_input_policy<manipulators::scalar_manip_t<Flags, T &>>
{
	using value_type = T;
	static constexpr auto flags{Flags};
	static constexpr bool precision{};
};
template <manipulators::scalar_flags Flags, software_binary_value T>
	requires(Flags.floating != manipulators::floating_format::hexfloat)
struct binary_input_policy<manipulators::scalar_manip_precision_t<Flags, T &>> : binary_input_policy<manipulators::scalar_manip_t<Flags, T &>>
{
	static constexpr bool precision{true};
};
template <typename M>
concept binary_text_input = requires { typename binary_input_policy<M>::value_type; };

/// @brief Rebinds the destination reference while preserving lexical and precision flags.
template <binary_text_input M>
inline constexpr auto binary_native_input(M field, binary_io_native<binary_input_policy<M>::value_type::format> &value) noexcept
{
	using P = binary_input_policy<M>;
	using N = binary_io_native<P::value_type::format>;
	if constexpr (P::precision)
	{
		return manipulators::scalar_manip_precision_t<P::flags, N &>{value, field.precision};
	}
	else
	{
		return manipulators::scalar_manip_t<P::flags, N &>{value};
	}
}

/// @brief Publishes a native parser result with a bitwise copy, preserving signaling NaNs.
template <software_binary_value T>
inline constexpr void binary_native_publish(T &out, binary_io_native<T::format> const &value, parse_code code) noexcept
{
	if (code == parse_code::ok)
	{
		using U = typename T::storage_type;
		auto bits{::std::bit_cast<U>(value)};
		if constexpr (::std::same_as<U, binary128_words>)
		{
			bits = binary_native_word_order(bits);
		}
		else
		{
			using L = binary_interchange_layout<T::format>;
			bits = fp_translate_nan_encoding<L::mbits, L::ebits>(bits);
		}
		out = T::from_bits(bits);
	}
}

/// @brief Stores the leading hexadecimal digits in two significance-ordered words.
/// @details The common lexer handles leading zeroes, chunk boundaries and the sticky tail.
struct binary_hex_prefix
{
	binary128_words value{};
	constexpr void append(::std::size_t, ::std::size_t count, ::std::uint_least64_t packed) noexcept
	{
		value = binary_words_shift(value, static_cast<int>(count * 4u));
		value.low |= packed;
	}
};

/// @brief Rounds a binary significand directly from the retained hexadecimal prefix.
/// @details With at least p+1 significant bits retained, all omitted nibbles lie below the
/// guard. Combining their sticky bit with the retained low bits resolves every halfway case.
template <binary_text_input M, typename State>
inline constexpr parse_code binary_hex_finish(State const &state, M field) noexcept
{
	using P = binary_input_policy<M>;
	using T = typename P::value_type;
	using L = binary_interchange_layout<T::format>;
	auto const &digits{state.significand_state};
	if (!digits.has_digit || !state.has_exponent_marker || !state.exponent_has_digit)
	{
		return parse_code::invalid;
	}
	auto magnitude{digits.stored.value};
	if (!digits.has_nonzero_digit)
	{
		field.reference = binary_interchange_from_words<T>(binary_words_pack(L::mbits, L::ebits, state.negative, {}, 0));
		return parse_code::ok;
	}
	auto excess{digits.significant_hex_digits - static_cast<::std::int_least64_t>(digits.stored_hex_digits) - digits.fractional_hex_digits};
	auto twice{scan_decfloat_saturating_add(excess, excess)};
	auto shift{scan_decfloat_saturating_add(scan_hexfloat_context_exponent(state), scan_decfloat_saturating_add(twice, twice))};
	// Clamping after combining the exponents preserves exact cancellation in long input.
	int power{shift < -32768 ? -32768 : shift > 32768 ? 32768
													  : static_cast<int>(shift)};
	int top{power + binary_words_width(magnitude) - 1};
	if (top > L::bias)
	{
		return parse_code::overflow;
	}
	constexpr int minimum{1 - L::bias};
	int cut{(top < minimum ? minimum : top) - static_cast<int>(L::mbits) - power};
	auto kept{binary_words_shift(magnitude, -cut)};
	if (cut > 0)
	{
		bool guard{cut <= 128 && decimal_field(magnitude, static_cast<unsigned>(cut - 1), 1u) != 0u};
		bool sticky{digits.truncated_nonzero || cut > 128 || binary_words_nonzero(decimal_low_bits(magnitude, static_cast<unsigned>(cut - 1)))};
		if (decimal_increment(P::flags.rounding, state.negative, guard ? 5u : 0u, sticky, (kept.low & 1u) != 0u))
		{
			binary_words_increment(kept);
		}
	}
	if (!binary_words_nonzero(kept) || (top == L::bias && binary_words_width(kept) > static_cast<int>(L::mbits) + 1))
	{
		return parse_code::overflow;
	}
	field.reference = binary_interchange_from_words<T>(binary_words_pack(L::mbits, L::ebits, state.negative, kept, top));
	return parse_code::ok;
}

/// @brief Transfers the special-token lexer's infinity, sign and signaling class without a numeric cast.
template <software_binary_value T>
inline constexpr void binary_hex_special(T &dest, double const &value) noexcept
{
	using L = binary_interchange_layout<T::format>;
	auto parts{cast_source_binary_parts(value)};
	auto result{binary_cast_words(parts, L::mbits, L::ebits)};
	if (parts.category == decimal_class::signaling_nan)
	{
		if (!binary_words_nonzero(decimal_low_bits(result, L::mbits - 1u)))
		{
			result.low |= 1u;
		}
		auto quiet{binary_words_shift({1u, 0u}, L::mbits - 1u)};
		result.low &= ~quiet.low;
		result.high &= ~quiet.high;
	}
	dest = binary_interchange_from_words<T>(result);
}

/// @brief Completes a finite decimal token using the original digits and sticky tail.
template <binary_text_input M>
inline constexpr parse_code binary_text_finish(decimal_token_state<binary_text_components<binary_input_policy<M>::value_type::format>> &s, M field) noexcept
{
	using P = binary_input_policy<M>;
	using T = typename P::value_type;
	using L = binary_interchange_layout<T::format>;
	auto &v{s.value};
	using phase = decimal_scan_phase;
	if (s.phase == phase::start)
	{
		return parse_code::end_of_file;
	}
	if (v.category != decimal_class::finite)
	{
		bool inf{v.category == decimal_class::infinity};
		if (s.phase == phase::payload || (inf ? s.word_size != 3 && s.word_size != 8 : (s.word_size != (v.category == decimal_class::signaling_nan ? 4u : 3u) && s.phase != phase::closed_payload)))
		{
			return parse_code::invalid;
		}
		binary128_words payload{};
		for (::std::size_t i{}; i != v.size; ++i)
		{
			decimal_multiply_add(payload, 10u, v.digits[i]);
		}
		auto w{binary_words_special(L::mbits, L::ebits, v.negative && (inf || P::flags.nan_parse_sign), inf, payload)};
		if (v.category == decimal_class::signaling_nan)
		{
			// The quiet bit and payload do not overlap the exponent.
			if constexpr (L::mbits > 64)
			{
				w.high &= ~(UINT64_C(1) << (L::mbits - 65));
			}
			else
			{
				w.low &= ~(UINT64_C(1) << (L::mbits - 1));
			}
			if (!binary_words_nonzero(decimal_low_bits(w, L::mbits)))
			{
				w.low |= 1u;
			}
		}
		field.reference = binary_interchange_from_words<T>(w);
		return parse_code::ok;
	}
	if (!s.any_digit || ((s.phase == phase::exponent_sign || s.phase == phase::exponent ||
						  P::flags.floating == manipulators::floating_format::scientific) &&
						 !s.exponent_digit))
	{
		return parse_code::invalid;
	}
	auto q{scan_decfloat_saturating_add(s.point_shift, s.negative_exponent ? -s.exponent : s.exponent)};
	v.exponent = q < -32000 ? -32000 : q > 32000 ? 32000
												 : static_cast<int>(q);
	if constexpr (P::precision)
	{
		auto n{field.precision > 64000u ? 64000 : static_cast<int>(field.precision)};
		int keep{floating_precision_is_fractional<P::flags.precision> ? static_cast<int>(v.size) + v.exponent + n : (n ? n : 1)};
		if (keep < static_cast<int>(v.size))
		{
			decimal_round(v, keep, s.sticky, P::flags.rounding);
			s.sticky = false;
		}
	}
	auto result{binary_interchange_from_decimal<T, P::flags.rounding>(v, v.negative, s.sticky)};
	auto parts{binary_interchange_parts(result)};
	if (parts.category != decimal_class::finite || (v.digits[0] && !binary_words_nonzero(parts.coefficient)))
	{
		return parse_code::overflow;
	}
	field.reference = result;
	return parse_code::ok;
}
} // namespace fast_io::details

namespace fast_io
{
/// @brief Publishes a scanned value through the caller's lvalue.
template <details::software_binary_value T>
constexpr auto scan_alias_define(io_alias_t, T &value) noexcept
{
	return manipulators::scalar_manip_t<manipulators::floating_point_default_scalar_flags, T &>{value};
}

/// @brief Reserves the destination-format presentation, including explicit decimal precision.
template <::std::integral C, details::binary_text_field M>
constexpr ::std::size_t print_reserve_size(io_reserve_type_t<C, M>, M field) noexcept
{
	using P = details::binary_text_policy<M>;
	if constexpr (details::binary_io_native_available<typename P::value_type>)
	{
		auto n{details::binary_native_field(field)};
		if constexpr (requires { print_reserve_size(io_reserve_type<C, decltype(n)>, n); })
		{
			return print_reserve_size(io_reserve_type<C, decltype(n)>, n);
		}
		else
		{
			return print_reserve_size(io_reserve_type<C, decltype(n)>);
		}
	}
	else if constexpr (P::flags.floating == manipulators::floating_format::hexfloat)
	{
		// Sign, prefix, radix, leading digit, exponent, and the exact fraction all fit this bound.
		constexpr auto exact{static_cast<::std::size_t>((details::binary_interchange_layout<P::value_type::format>::mbits + 3u) / 4u + 12u)};
		if constexpr (P::form == details::binary_text_form::precision && details::floating_precision_preserves_trailing_zero<P::flags.precision>)
		{
			return details::intrinsics::add_or_overflow_die(exact, field.precision);
		}
		return exact;
	}
	else
	{
		auto plan{details::binary_text_plan(field)};
		if (plan.value.category != details::decimal_class::finite)
		{
			return 12;
		}
		return details::intrinsics::add_or_overflow_die(details::decimal_spelling_size(plan.value, plan.layout), static_cast<::std::size_t>(P::flags.showpos || plan.value.negative));
	}
}

/// @brief Emits the original bit pattern using its declared binary format and rounding policy.
template <::std::integral C, details::binary_text_field M>
constexpr C *print_reserve_define(io_reserve_type_t<C, M>, C *out, M field) noexcept
{
	using P = details::binary_text_policy<M>;
	if constexpr (details::binary_io_native_available<typename P::value_type>)
	{
		auto n{details::binary_native_field(field)};
		return print_reserve_define(io_reserve_type<C, decltype(n)>, out, n);
	}
	else if constexpr (P::flags.floating == manipulators::floating_format::hexfloat)
	{
		return details::binary_hex_write(out, field);
	}
	else
	{
		auto plan{details::binary_text_plan(field)};
		return details::decimal_write<P::flags>(out, plan.value, plan.layout);
	}
}

/// @brief Supplies bounded lexical state sufficient for the longest binary rounding midpoint.
template <::std::integral C, details::binary_text_input M>
constexpr auto scan_context_type(io_reserve_type_t<C, M>) noexcept
{
	using P = details::binary_input_policy<M>;
	using T = typename P::value_type;
	if constexpr (details::binary_io_native_available<T>)
	{
		using N = details::binary_io_native<T::format>;
		using Bridge = decltype(details::binary_native_input(::std::declval<M>(), ::std::declval<N &>()));
		return scan_context_type(io_reserve_type<C, Bridge>);
	}
	else if constexpr (P::flags.floating == manipulators::floating_format::hexfloat)
	{
		return io_type_t<details::scan_hexfloat_context<C, details::binary_hex_prefix>>{};
	}
	else
	{
		return io_type_t<details::decimal_token_state<details::binary_text_components<T::format>>>{};
	}
}
/// @brief Accepts a fragment and leaves the destination untouched while the token is partial.
template <::std::integral C, details::binary_text_input M>
constexpr parse_result<C const *> scan_context_define(io_reserve_type_t<C, M>, typename decltype(scan_context_type(io_reserve_type<C, M>))::type &state, C const *first, C const *last, M field) noexcept
{
	using P = details::binary_input_policy<M>;
	using T = typename P::value_type;
	if constexpr (details::binary_io_native_available<T>)
	{
		details::binary_io_native<T::format> value{};
		auto bridge{details::binary_native_input(field, value)};
		auto result{scan_context_define(io_reserve_type<C, decltype(bridge)>, state, first, last, bridge)};
		details::binary_native_publish(field.reference, value, result.code);
		return result;
	}
	else if constexpr (P::flags.floating == manipulators::floating_format::hexfloat)
	{
		constexpr unsigned capacity{(details::binary_interchange_layout<T::format>::mbits + 8u) / 4u};
		return details::scan_hexfloat_context_feed<capacity, P::flags>(state, first, last, [&] { return details::binary_hex_finish(state, field); }, [&](C const *begin, C const *end) {
				double value{};
				auto result{details::scan_hexfloat_context_special_define<C, P::flags, double>(state, begin, end, value)};
				if (result.code == parse_code::ok)
				{
					details::binary_hex_special(field.reference, value);
				}
				return result; });
	}
	else
	{
		return details::decimal_scan_feed<P::flags, 38u>(state, first, last, [&] { return details::binary_text_finish(state, field); });
	}
}

/// @brief Commits a completed token at EOF under the same rounding rule as delimiter completion.
template <::std::integral C, details::binary_text_input M>
constexpr parse_code scan_context_eof_define(io_reserve_type_t<C, M>, typename decltype(scan_context_type(io_reserve_type<C, M>))::type &state, M field) noexcept
{
	using P = details::binary_input_policy<M>;
	using T = typename P::value_type;
	if constexpr (details::binary_io_native_available<T>)
	{
		details::binary_io_native<T::format> value{};
		auto bridge{details::binary_native_input(field, value)};
		auto code{scan_context_eof_define(io_reserve_type<C, decltype(bridge)>, state, bridge)};
		details::binary_native_publish(field.reference, value, code);
		return code;
	}
	else if constexpr (P::flags.floating == manipulators::floating_format::hexfloat)
	{
		if (state.phase == details::scan_hexfloat_context_phase::special)
		{
			auto first{state.special_buffer.data()};
			double value{};
			auto r{details::scan_hexfloat_contiguous_define_impl<C, P::flags>(first, first + state.special_buffer.size, value)};
			if (r.code == parse_code::ok)
			{
				details::binary_hex_special(field.reference, value);
			}
			return r.code;
		}
		return state.phase == details::scan_hexfloat_context_phase::start ? parse_code::end_of_file : details::binary_hex_finish(state, field);
	}
	else
	{
		return details::binary_text_finish(state, field);
	}
}

/// @brief Uses the matching native parser's contiguous path and commits only a complete token.
template <::std::integral C, details::binary_text_input M>
	requires details::binary_io_native_available<typename details::binary_input_policy<M>::value_type>
constexpr parse_result<C const *> scan_contiguous_define(io_reserve_type_t<C, M>, C const *first, C const *last, M field) noexcept
{
	using T = typename details::binary_input_policy<M>::value_type;
	details::binary_io_native<T::format> value{};
	auto bridge{details::binary_native_input(field, value)};
	auto result{scan_contiguous_define(io_reserve_type<C, decltype(bridge)>, first, last, bridge)};
	details::binary_native_publish(field.reference, value, result.code);
	return result;
}

/// @brief Relays the native decimal lexer's rewind count for a tentative special-token suffix.
template <::std::integral C, details::binary_text_input M>
	requires(details::binary_io_native_available<typename details::binary_input_policy<M>::value_type> &&
			 details::binary_input_policy<M>::flags.floating != manipulators::floating_format::hexfloat)
constexpr ::std::size_t scan_context_eof_rewind_size(io_reserve_type_t<C, M>, typename decltype(scan_context_type(io_reserve_type<C, M>))::type &state, M field) noexcept
{
	using T = typename details::binary_input_policy<M>::value_type;
	details::binary_io_native<T::format> value{};
	auto bridge{details::binary_native_input(field, value)};
	return scan_context_eof_rewind_size(io_reserve_type<C, decltype(bridge)>, state, bridge);
}
} // namespace fast_io
