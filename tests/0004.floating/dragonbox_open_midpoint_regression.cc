#include <bit>
#include <charconv>
#include <cstdint>
#include <string_view>
#include <system_error>

#include <fast_io_freestanding.h>

namespace
{

using rounding = ::fast_io::manipulators::floating_rounding;

template <typename result_type>
inline constexpr void normalize_decimal(result_type &result) noexcept
{
	while (result.m10 && result.m10 % 10u == 0u)
	{
		result.m10 /= 10u;
		++result.e10;
	}
}

template <typename floating_type, rounding policy>
[[nodiscard]] bool check_narrow_policy_powers() noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	constexpr auto exponent_mask{
		(static_cast<::std::uint_least32_t>(1u) << trait::ebits) - 1u};
	for (bool negative : {false, true})
	{
		for (::std::uint_least32_t exponent{1u};
			 exponent != exponent_mask; ++exponent)
		{
			auto current{
				::fast_io::details::dragonbox_impl_narrow_hybrid<
					floating_type, policy>(
						0u, static_cast<::std::int_least32_t>(exponent),
						negative)};
			auto reference{
				::fast_io::details::wide_shortest_from_binary<
					floating_type, policy>(
						0u, exponent, negative)};
			normalize_decimal(current);
			normalize_decimal(reference);
			if (!reference.success ||
				current.m10 != reference.m10 ||
				current.e10 != reference.e10)
			{
				return false;
			}
		}
	}
	return true;
}

template <typename floating_type>
[[nodiscard]] bool check_all_narrow_policy_powers() noexcept
{
	return check_narrow_policy_powers<
			   floating_type, rounding::nearest_to_even>() &&
		   check_narrow_policy_powers<
			   floating_type, rounding::nearest_to_odd>() &&
		   check_narrow_policy_powers<
			   floating_type, rounding::nearest_toward_plus_infinity>() &&
		   check_narrow_policy_powers<
			   floating_type, rounding::nearest_toward_minus_infinity>() &&
		   check_narrow_policy_powers<
			   floating_type, rounding::nearest_toward_zero>() &&
		   check_narrow_policy_powers<
			   floating_type, rounding::nearest_away_from_zero>() &&
		   check_narrow_policy_powers<
			   floating_type, rounding::toward_plus_infinity>() &&
		   check_narrow_policy_powers<
			   floating_type, rounding::toward_minus_infinity>() &&
		   check_narrow_policy_powers<
			   floating_type, rounding::toward_zero>() &&
		   check_narrow_policy_powers<
			   floating_type, rounding::away_from_zero>();
}

template <typename floating_type, ::std::size_t extent>
[[nodiscard]] bool check_narrow_public_boundary(
	::std::uint_least16_t representation,
	char const (&expected)[extent]) noexcept
{
	volatile ::std::uint_least16_t runtime_representation{representation};
	auto const value{
		::std::bit_cast<floating_type>(
			static_cast<::std::uint_least16_t>(runtime_representation))};
	char exact[extent - 1u];
	auto const formatted{
		::fast_io::to_chars<rounding::toward_plus_infinity>(
			exact, exact + sizeof(exact), value,
			::std::chars_format::scientific)};
	if (formatted.ec != ::std::errc{} ||
		formatted.ptr != exact + sizeof(exact) ||
		::std::string_view(exact, sizeof(exact)) !=
			::std::string_view(expected, extent - 1u))
	{
		return false;
	}
	char short_output[extent]{};
	for (auto &element : short_output)
	{
		element = static_cast<char>(0x5a);
	}
	auto const rejected{
		::fast_io::to_chars<rounding::toward_plus_infinity>(
			short_output, short_output + sizeof(exact) - 1u, value,
			::std::chars_format::scientific)};
	if (rejected.ec != ::std::errc::value_too_large ||
		rejected.ptr != short_output + sizeof(exact) - 1u)
	{
		return false;
	}
	for (auto element : short_output)
	{
		if (element != static_cast<char>(0x5a))
		{
			return false;
		}
	}
	return true;
}

[[nodiscard]] inline constexpr bool same_fields(
	double left, double right) noexcept
{
	auto const lhs{::fast_io::details::get_punned_result(left)};
	auto const rhs{::fast_io::details::get_punned_result(right)};
	return lhs.mantissa == rhs.mantissa &&
		   lhs.exponent == rhs.exponent &&
		   lhs.sign == rhs.sign;
}

consteval bool constant_carrier_is_correct() noexcept
{
	constexpr auto source{
		::std::bit_cast<double>(UINT64_C(0xc3d5819119d3b7a8))};
	constexpr auto fields{
		::fast_io::details::get_punned_result(source)};
	constexpr auto result{
		::fast_io::details::dragonbox_impl<
			double, rounding::nearest_to_odd>(
			fields.mantissa,
			static_cast<::std::int_least32_t>(fields.exponent),
			fields.sign)};
	return result.m10 == UINT64_C(6198717147617599) &&
		   result.e10 == 3;
}

static_assert(constant_carrier_is_correct());

/*
The rejected carrier 61987171476176*10^5 is exactly the open midpoint above
this negative binary64 under nearest-to-odd.  The exact endpoint divisibility
test must continue to the finer grid and emit 6198717147617599*10^3.  Checking
the constexpr integer kernel, the public formatter spelling, and an
independent public parse pins the same theorem at all three interfaces,
including MSVC targets without a native uint128 type.
*/
[[nodiscard]] bool check_public_roundtrip() noexcept
{
	constexpr auto source{
		::std::bit_cast<double>(UINT64_C(0xc3d5819119d3b7a8))};
	char buffer[64u]{};
	auto const formatted{
		::fast_io::to_chars<rounding::nearest_to_odd>(
			buffer, buffer + sizeof(buffer), source,
			::std::chars_format::scientific)};
	constexpr ::std::string_view expected{
		"-6.198717147617599e+18"};
	if (formatted.ec != ::std::errc{} ||
		::std::string_view(
			buffer, static_cast<::std::size_t>(formatted.ptr - buffer)) !=
			expected)
	{
		return false;
	}

	double parsed{};
	auto const converted{
		::fast_io::from_chars<rounding::nearest_to_odd>(
			buffer, formatted.ptr, parsed,
			::std::chars_format::scientific)};
	return converted.ec == ::std::errc{} &&
		   converted.ptr == formatted.ptr &&
		   same_fields(parsed, source);
}

} // namespace

int main()
{
	bool result{check_public_roundtrip()};
#if defined(__FLT16_MANT_DIG__) && __FLT16_MANT_DIG__ == 11
	result = result && check_all_narrow_policy_powers<_Float16>() &&
		check_narrow_public_boundary<_Float16>(
			0x0400u, "6.1e-05");
#endif
#if (defined(__GNUC__) && !defined(__clang__) && \
	 defined(__BFLT16_MANT_DIG__)) ||            \
	defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE)
	result = result && check_all_narrow_policy_powers<__bf16>() &&
		check_narrow_public_boundary<__bf16>(
			0x0080u, "1.17e-38");
#endif
	return !result;
}
