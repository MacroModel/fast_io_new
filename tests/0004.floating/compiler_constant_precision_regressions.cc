#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <fast_io_freestanding.h>

namespace
{
using format = ::fast_io::manipulators::floating_format;
using precision_mode = ::fast_io::manipulators::floating_precision;
using rounding = ::fast_io::manipulators::floating_rounding;

template <format presentation, precision_mode precision, rounding policy,
		  bool uppercase = false, bool comma = false>
consteval auto make_flags() noexcept
{
	auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.floating = presentation;
	flags.precision = precision;
	flags.rounding = policy;
	flags.showpos = true;
	flags.uppercase = uppercase;
	flags.uppercase_e = uppercase;
	flags.nan_show_sign = true;
	flags.nan_show_type = true;
	flags.comma = comma;
	flags.showbase = presentation == format::hexfloat;
	flags.uppercase_showbase = uppercase && presentation == format::hexfloat;
	return flags;
}

template <::std::size_t size>
struct expected_literal
{
	char value[size];

	consteval expected_literal(char const (&source)[size]) noexcept
	{
		for (::std::size_t index{}; index != size; ++index)
		{
			value[index] = source[index];
		}
	}
};

template <::std::integral char_type>
[[nodiscard]] constexpr char_type oracle_code_unit(char value) noexcept
{
	// Expected spellings are written once in the source execution character
	// set, then translated through fast_io's character-domain literals.  This
	// keeps the regression valid for EBCDIC char/wchar_t targets as well as the
	// UTF code-unit types instead of assuming that an ASCII byte can be widened.
	switch (value)
	{
	case '+':
		return ::fast_io::char_literal_v<u8'+', char_type>;
	case '-':
		return ::fast_io::char_literal_v<u8'-', char_type>;
	case '.':
		return ::fast_io::char_literal_v<u8'.', char_type>;
	case ',':
		return ::fast_io::char_literal_v<u8',', char_type>;
	case '0':
		return ::fast_io::char_literal_v<u8'0', char_type>;
	case '1':
		return ::fast_io::char_literal_v<u8'1', char_type>;
	case '2':
		return ::fast_io::char_literal_v<u8'2', char_type>;
	case '3':
		return ::fast_io::char_literal_v<u8'3', char_type>;
	case '4':
		return ::fast_io::char_literal_v<u8'4', char_type>;
	case '5':
		return ::fast_io::char_literal_v<u8'5', char_type>;
	case '6':
		return ::fast_io::char_literal_v<u8'6', char_type>;
	case '7':
		return ::fast_io::char_literal_v<u8'7', char_type>;
	case '8':
		return ::fast_io::char_literal_v<u8'8', char_type>;
	case '9':
		return ::fast_io::char_literal_v<u8'9', char_type>;
	case 'e':
		return ::fast_io::char_literal_v<u8'e', char_type>;
	case 'E':
		return ::fast_io::char_literal_v<u8'E', char_type>;
	case 'p':
		return ::fast_io::char_literal_v<u8'p', char_type>;
	case 'P':
		return ::fast_io::char_literal_v<u8'P', char_type>;
	case 'x':
		return ::fast_io::char_literal_v<u8'x', char_type>;
	case 'X':
		return ::fast_io::char_literal_v<u8'X', char_type>;
	default:
		return char_type{};
	}
}

template <::std::integral char_type, typename floating_type, auto flags,
		  ::std::size_t expected_size>
[[nodiscard]] bool check(
	::fast_io::details::punning_result<floating_type> fields,
	::std::size_t requested_precision,
	char const (&expected)[expected_size]) noexcept
{
	using source_type = ::fast_io::manipulators::scalar_manip_precision_t<
		flags, floating_type>;
	using source_tag = ::fast_io::io_reserve_type_t<char_type, source_type>;
	auto const value{
		::fast_io::details::compiler_constant_floating_value_from_fields<
			floating_type>(fields)};
	source_type const source{value, requested_precision};
	auto const proxy{print_compiler_constant_materialize(source_tag{}, source)};
	using proxy_type = ::std::remove_cv_t<decltype(proxy)>;
	using proxy_tag = ::fast_io::io_reserve_type_t<char_type, proxy_type>;

	constexpr ::std::size_t prefix_guard{13u};
	constexpr ::std::size_t payload_capacity{128u};
	constexpr ::std::size_t suffix_guard{17u};
	constexpr auto guard{static_cast<char_type>(0x5au)};
	char_type ordinary[prefix_guard + payload_capacity + suffix_guard];
	char_type replacement[prefix_guard + payload_capacity + suffix_guard];
	for (auto &element : ordinary)
	{
		element = guard;
	}
	for (auto &element : replacement)
	{
		element = guard;
	}
	auto *const ordinary_output{ordinary + prefix_guard};
	auto *const replacement_output{replacement + prefix_guard};
	auto const ordinary_length{
		print_reserve_precise_size(source_tag{}, source)};
	auto const replacement_length{
		print_reserve_precise_size(proxy_tag{}, proxy)};
	if (payload_capacity < ordinary_length ||
		ordinary_length != expected_size - 1u ||
		replacement_length != ordinary_length)
	{
		return false;
	}
	auto const ordinary_end{print_reserve_precise_define(
		source_tag{}, ordinary_output, ordinary_length, source)};
	auto const replacement_end{print_reserve_precise_define(
		proxy_tag{}, replacement_output, replacement_length, proxy)};
	if (ordinary_end != ordinary_output + ordinary_length ||
		replacement_end != replacement_output + replacement_length)
	{
		return false;
	}
	for (::std::size_t index{}; index != prefix_guard; ++index)
	{
		if (ordinary[index] != guard || replacement[index] != guard)
		{
			return false;
		}
	}
	for (::std::size_t index{prefix_guard + ordinary_length};
		 index != prefix_guard + payload_capacity + suffix_guard; ++index)
	{
		if (ordinary[index] != guard || replacement[index] != guard)
		{
			return false;
		}
	}
	for (::std::size_t index{}; index != ordinary_length; ++index)
	{
		auto const oracle{oracle_code_unit<char_type>(expected[index])};
		if (ordinary_output[index] != oracle ||
			replacement_output[index] != oracle)
		{
			return false;
		}
	}
	return true;
}

template <::std::integral char_type, typename floating_type, auto flags,
		  ::fast_io::details::punning_result<floating_type> fields,
		  ::std::size_t requested_precision, expected_literal expected>
struct regression_case
{
	[[nodiscard]] static bool run() noexcept
	{
		return check<char_type, floating_type, flags>(
			fields, requested_precision, expected.value);
	}
};

template <typename... cases>
[[nodiscard]] bool run_cases() noexcept
{
	return (cases::run() && ...);
}
} // namespace

int main()
{
	bool result{true};

#if defined(__FLT16_MANT_DIG__) && __FLT16_MANT_DIG__ == 11
	// 234 * 2^-24 is between 1e-5 and 2e-5.  Significant precision zero is
	// normalized to one digit, so nearest-even is exactly 1e-5.  The full exact
	// 19-digit carrier must not enter the shorter Ryu coefficient-width path.
	using binary16_decimal_zero_precision = regression_case<char, _Float16,
															make_flags<format::general, precision_mode::significant,
																	   rounding::nearest_to_even>(),
															::fast_io::details::punning_result<_Float16>{234u, 0u, false}, 0u,
															expected_literal{"+1e-05"}>;

	// 0x1.ffcp+5 at one fractional hexadecimal digit rounds through 0x2p+5
	// and must normalize to 0x1p+6.  The carry belongs to the aligned fraction
	// field even though its uint16 carrier has unused high bits.
	using binary16_hex_carry = regression_case<wchar_t, _Float16,
											   make_flags<format::hexfloat, precision_mode::fractional,
														  rounding::nearest_to_odd, true>(),
											   ::fast_io::details::punning_result<_Float16>{1023u, 20u, false}, 1u,
											   expected_literal{"+0X1P+6"}>;
	result = result && run_cases<
						   binary16_decimal_zero_precision, binary16_hex_carry>();
#endif

#if (defined(__GNUC__) && !defined(__clang__) && \
	 defined(__BFLT16_MANT_DIG__)) ||            \
	defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE)
	// This exact bfloat16 magnitude needs an 18-digit guard carrier.  P=17
	// therefore rounds to exactly 17 significant decimal digits.
	using bfloat16_extended_decimal_carrier = regression_case<
		char16_t, __bf16,
		make_flags<format::general, precision_mode::significant,
				   rounding::nearest_to_even>(),
		::fast_io::details::punning_result<__bf16>{0u, 250u, false}, 17u,
		expected_literal{"+1.0633823966279327e+37"}>;
	result = result && run_cases<bfloat16_extended_decimal_carrier>();
#endif

	// 1/64 has the canonical decimal coefficient 15625e-6.  An exact-window
	// carrier may contain representation-only trailing zeroes, but general
	// fractional-preserving output must make the same notation decision as the
	// canonical run-time carrier.
	using binary32_canonical_decimal_carrier = regression_case<wchar_t, float,
															   make_flags<format::general,
																		  precision_mode::fractional_preserve_trailing_zero,
																		  rounding::toward_zero, true>(),
															   ::fast_io::details::punning_result<float>{0u, 121u, false}, 36u,
															   expected_literal{"+1.5625E-02"}>;

	// Rounding the largest binary32 subnormal upward at five hexadecimal
	// fractional digits produces the minimum normal value, 0x1p-126.
	using binary32_subnormal_hex_carry = regression_case<char32_t, float,
														 make_flags<format::hexfloat,
																	precision_mode::fractional_preserve_trailing_zero,
																	rounding::nearest_toward_minus_infinity, false, true>(),
														 ::fast_io::details::punning_result<float>{8388607u, 0u, false}, 5u,
														 expected_literal{"+0x1,00000p-126"}>;

	// The exact precision window retains 19 decimal digits here.  Scientific
	// P=17 fractional means 18 significant digits; directed +infinity selects
	// the final 7.  Sending that extended carrier to the native 17-digit writer
	// used to corrupt both the coefficient and exponent.
	using binary64_extended_decimal_carrier = regression_case<char, double,
															  make_flags<format::scientific,
																		 precision_mode::fractional_preserve_trailing_zero,
																		 rounding::toward_plus_infinity>(),
															  ::fast_io::details::punning_result<double>{
																  1073741824u, 1792u, false},
															  17u, expected_literal{"+3.10503692489973307e+231"}>;

	// The negative value is just above the hexadecimal rounding boundary in
	// magnitude.  Nearest-odd is non-tied here and normalizes the carried
	// coefficient from 0x2p-975 to 0x1p-974.
	using binary64_hex_carry = regression_case<char, double,
											   make_flags<format::hexfloat, precision_mode::fractional,
														  rounding::nearest_to_odd, true>(),
											   ::fast_io::details::punning_result<double>{
												   4503599611707392ULL, 48u, true},
											   3u, expected_literal{"-0X1P-974"}>;

	result = result && run_cases<binary32_canonical_decimal_carrier,
								 binary32_subnormal_hex_carry, binary64_extended_decimal_carrier,
								 binary64_hex_carry>();

#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
	// Maximum finite binary128 rounded to nine significant hexadecimal digits
	// carries to the representable formatting boundary 0x1p+16384.  Formatting
	// is not constrained to return another finite binary128 value.
	using binary128_hex_carry = regression_case<char16_t, __float128,
												make_flags<format::hexfloat, precision_mode::significant,
														   rounding::nearest_to_even>(),
												::fast_io::details::punning_result<__float128>{
													(static_cast<__uint128_t>(1u) << 112u) - 1u, 32766u, false},
												9u, expected_literal{"+0x1p+16384"}>;
	result = result && run_cases<binary128_hex_carry>();
#endif

	return result ? 0 : 1;
}
