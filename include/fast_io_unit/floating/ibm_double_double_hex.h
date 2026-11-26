#pragma once

#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
namespace fast_io::details
{

// One leading digit and the nibbles between binary64's extreme exponents.
inline constexpr ::std::size_t ibm_double_double_hex_digits{1u + (1023u + 1074u + 3u) / 4u};
inline constexpr ::std::size_t ibm_double_double_hex_reserve{ibm_double_double_hex_digits + 10u};

struct ibm_double_double_hex_plan
{
	unsigned char digits[ibm_double_double_hex_digits]{};
	::std::size_t retained{};
	::std::size_t length{};
	::std::int_least32_t exponent{};
	bool negative{};
};

template <::fast_io::manipulators::scalar_flags flags, bool exact, typename flt>
	requires(::fast_io::details::fp_floating_point_is_ibm_double_double<flt>)
[[nodiscard]] inline constexpr ibm_double_double_hex_plan
make_ibm_double_double_hex_plan(flt value, ::std::size_t precision) noexcept
{
	auto const storage{::fast_io::bit_cast<::fast_io::details::ibm_double_double_storage>(value)};
	auto const high{::fast_io::details::ibm_double_double_decode_component(storage.high)};
	auto const low{::fast_io::details::ibm_double_double_decode_component(storage.low)};

	// Sparse pairs have the lowest bit in the second component. Express both
	// components in that unit before extracting hexadecimal digits.
	constexpr auto capacity{::fast_io::details::scan_hexfloat_ibm_storage::word_count + 1u};
	::fast_io::details::scan_decfloat_bigint<capacity> magnitude;
	::fast_io::details::scan_decfloat_bigint_set_u64(magnitude, high.significand);
	auto const shift{static_cast<::std::size_t>(high.exponent - low.exponent)};
	(void)::fast_io::details::scan_decfloat_bigint_shift_left_inplace(magnitude, shift);
	if (high.negative == low.negative)
	{
		(void)::fast_io::details::scan_decfloat_bigint_add_u64(magnitude, low.significand);
	}
	else
	{
		::fast_io::details::scan_decfloat_bigint<capacity> subtrahend;
		::fast_io::details::scan_decfloat_bigint_set_u64(subtrahend, low.significand);
		::fast_io::details::scan_decfloat_bigint_sub_assign(magnitude, subtrahend);
	}
	auto const fraction_bits{::fast_io::details::scan_decfloat_bigint_bit_width(magnitude) - 1u};
	auto const available{1u + (fraction_bits + 3u) / 4u};
	auto digit_at = [&](::std::size_t digit) constexpr noexcept {
		auto const position{static_cast<::std::int_least32_t>(fraction_bits) -
							 static_cast<::std::int_least32_t>(digit * 4u)};
		if (position < 0)
		{
			return static_cast<unsigned>((magnitude.limb[0] << -position) & 15u);
		}
		auto const word{static_cast<::std::size_t>(position) / 64u};
		auto const offset{static_cast<unsigned>(position) % 64u};
		auto bits{magnitude.limb[word] >> offset};
		if (60u < offset && word + 1u < magnitude.size)
		{
			bits |= magnitude.limb[word + 1u] << (64u - offset);
		}
		return static_cast<unsigned>(bits & 15u);
	};
	auto total_precision{available};
	if constexpr (!exact)
	{
		if constexpr (::fast_io::details::floating_precision_is_fractional<flags.precision>)
		{
			total_precision = ::fast_io::details::compiler_constant_hex_size_add(precision, 1u);
		}
		else
		{
			total_precision = precision ? precision : 1u;
		}
	}
	ibm_double_double_hex_plan plan{};
	plan.negative = high.negative;
	plan.exponent = low.exponent + static_cast<::std::int_least32_t>(fraction_bits);
	plan.retained = total_precision < available ? total_precision : available;
	for (::std::size_t digit{}; digit != plan.retained; ++digit)
	{
		plan.digits[digit] = static_cast<unsigned char>(digit_at(digit));
	}
	if constexpr (!exact)
	{
		if (total_precision < available)
		{
			auto const guard{digit_at(total_precision)};
			auto const guard_position{total_precision * 4u};
			auto const sticky{guard_position < fraction_bits &&
				::fast_io::details::scan_decfloat_bigint_any_bits_below(magnitude, fraction_bits - guard_position)};
			using mode = ::fast_io::manipulators::floating_rounding;
			auto rounds_up = [&]<mode rounding>() constexpr noexcept {
				return ::fast_io::details::print_rsvhexfloat_round_up<rounding>(
					plan.negative, plan.digits[plan.retained - 1u], guard, sticky);
			};
			bool increment{};
			if constexpr (flags.rounding == mode::current_environment)
			{
				switch (::fast_io::details::current_floating_rounding())
				{
				case mode::toward_plus_infinity:
					increment = rounds_up.template operator()<mode::toward_plus_infinity>();
					break;
				case mode::toward_minus_infinity:
					increment = rounds_up.template operator()<mode::toward_minus_infinity>();
					break;
				case mode::toward_zero:
					break;
				default:
					increment = rounds_up.template operator()<mode::nearest_to_even>();
					break;
				}
			}
			else
			{
				increment = rounds_up.template operator()<flags.rounding>();
			}
			for (auto digit{plan.retained}; increment && digit;)
			{
				auto &current{plan.digits[--digit]};
				increment = ++current == 16u;
				if (increment)
				{
					current = 0u;
				}
			}
			if constexpr (flags.precision != ::fast_io::manipulators::floating_precision::charconv_hex_fractional)
			{
				if (plan.digits[0] == 2u)
				{
					plan.digits[0] = 1u;
					++plan.exponent;
				}
			}
		}
	}
	if constexpr (!exact && ::fast_io::details::floating_precision_preserves_trailing_zero<flags.precision>)
	{
		plan.length = total_precision;
	}
	else
	{
		plan.length = plan.retained;
		while (1u < plan.length && !plan.digits[plan.length - 1u])
		{
			--plan.length;
		}
	}
	return plan;
}

template <::fast_io::manipulators::scalar_flags flags, bool exact, typename flt>
[[nodiscard]] inline constexpr ::std::size_t
ibm_double_double_hex_size(flt value, ::std::size_t precision = 0u) noexcept
{
	auto const plan{::fast_io::details::make_ibm_double_double_hex_plan<flags, exact>(value, precision)};
	auto const punctuation{static_cast<::std::size_t>(flags.showpos || plan.negative) +
		static_cast<::std::size_t>(flags.showbase) * 2u + static_cast<::std::size_t>(1u < plan.length) +
		::fast_io::details::compiler_constant_hex_exponent_size(plan.exponent)};
	return ::fast_io::details::compiler_constant_hex_size_add(plan.length, punctuation);
}

template <::fast_io::manipulators::scalar_flags flags, bool exact, ::std::integral char_type, typename flt>
inline constexpr char_type *print_ibm_double_double_hex(char_type *iter, flt value,
														::std::size_t precision = 0u) noexcept
{
	auto const plan{::fast_io::details::make_ibm_double_double_hex_plan<flags, exact>(value, precision)};
	iter = ::fast_io::details::print_rsv_fp_sign_impl<flags.showpos>(iter, plan.negative);
	if constexpr (flags.showbase)
	{
		iter = ::fast_io::details::print_reserve_show_base_impl<16u, flags.uppercase_showbase, false>(iter);
	}
	iter = ::fast_io::details::print_rsvhexfloat_digit_impl<flags.uppercase>(iter, plan.digits[0]);
	if (1u < plan.length)
	{
		*iter++ = ::fast_io::char_literal_v<(flags.comma ? u8',' : u8'.'), char_type>;
		auto const count{plan.length < plan.retained ? plan.length : plan.retained};
		for (::std::size_t digit{1u}; digit != count; ++digit)
		{
			iter = ::fast_io::details::print_rsvhexfloat_digit_impl<flags.uppercase>(iter, plan.digits[digit]);
		}
		iter = ::fast_io::details::my_fill_n(iter, plan.length - count, ::fast_io::char_literal_v<u8'0', char_type>);
	}
	*iter++ = ::fast_io::char_literal_v<(flags.uppercase_e ? u8'P' : u8'p'), char_type>;
	return ::fast_io::details::compiler_constant_hex_write_exponent(iter, plan.exponent);
}

} // namespace fast_io::details
#endif
