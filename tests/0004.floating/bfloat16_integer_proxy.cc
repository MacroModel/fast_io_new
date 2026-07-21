#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <fast_io_freestanding.h>

namespace
{

#if defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE) && defined(__clang__) && \
	(defined(__x86_64__) || defined(_M_X64)) &&                       \
	!defined(__AVX512BF16__) &&                                      \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))

using precision = ::fast_io::manipulators::floating_precision;
using rounding = ::fast_io::manipulators::floating_rounding;

inline constexpr ::std::uint_least16_t signaling_nan_bits{0x7f81u};
inline constexpr __bf16 signaling_nan{
	::std::bit_cast<__bf16>(signaling_nan_bits)};

template <typename proxy_type>
[[nodiscard]] consteval bool exact_proxy(proxy_type const &proxy) noexcept
{
	using clean_type = ::std::remove_cvref_t<proxy_type>;
	return requires {
		typename clean_type::value_type;
		{ proxy.representation } -> ::std::convertible_to<::std::uint_least16_t>;
	} && ::std::same_as<typename clean_type::value_type, __bf16> &&
		   proxy.representation == signaling_nan_bits;
}

template <typename proxy_type>
[[nodiscard]] consteval bool exact_precision_proxy(
	proxy_type const &proxy) noexcept
{
	return exact_proxy(proxy) && proxy.precision == 3u;
}

[[nodiscard]] consteval bool constructor_matrix() noexcept
{
	using namespace ::fast_io::mnp;
	constexpr auto generic_flags{
		::fast_io::manipulators::floating_point_default_scalar_flags};
	bool result{
		exact_proxy(::fast_io::print_alias_define(
			::fast_io::io_alias, signaling_nan)) &&
		exact_proxy(scalar_generic<generic_flags>(signaling_nan)) &&
		exact_proxy(hexfloat(signaling_nan)) &&
		exact_proxy(hexfloat0x(signaling_nan)) &&
		exact_proxy(comma_hexfloat(signaling_nan)) &&
		exact_proxy(comma_hexfloat0x(signaling_nan)) &&
		exact_proxy(decimal(signaling_nan)) &&
		exact_proxy(comma_decimal(signaling_nan)) &&
		exact_proxy(general(signaling_nan)) &&
		exact_proxy(comma_general(signaling_nan)) &&
		exact_proxy(fixed(signaling_nan)) &&
		exact_proxy(comma_fixed(signaling_nan)) &&
		exact_proxy(scientific(signaling_nan)) &&
		exact_proxy(comma_scientific(signaling_nan))};

	result = result &&
			 exact_precision_proxy(hexfloat<true, precision::significant,
											rounding::nearest_to_odd>(signaling_nan, 3u)) &&
			 exact_precision_proxy(hexfloat<precision::fractional,
											rounding::toward_zero, true>(signaling_nan, 3u)) &&
			 exact_precision_proxy(hexfloat0x<true, precision::significant,
											  rounding::nearest_to_even>(signaling_nan, 3u)) &&
			 exact_precision_proxy(hexfloat0x<precision::fractional,
											  rounding::away_from_zero, true>(signaling_nan, 3u)) &&
			 exact_precision_proxy(comma_hexfloat<true, precision::significant,
												  rounding::nearest_toward_zero>(signaling_nan, 3u)) &&
			 exact_precision_proxy(comma_hexfloat<precision::fractional,
												  rounding::toward_plus_infinity, true>(signaling_nan, 3u)) &&
			 exact_precision_proxy(comma_hexfloat0x<true, precision::significant,
													rounding::nearest_away_from_zero>(signaling_nan, 3u)) &&
			 exact_precision_proxy(comma_hexfloat0x<precision::fractional,
													rounding::toward_minus_infinity, true>(signaling_nan, 3u));

	result = result &&
			 exact_precision_proxy(decimal<true, precision::significant,
										   rounding::nearest_to_even>(signaling_nan, 3u)) &&
			 exact_precision_proxy(decimal<precision::fractional,
										   rounding::nearest_to_odd, true>(signaling_nan, 3u)) &&
			 exact_precision_proxy(comma_decimal<true, precision::significant,
												 rounding::nearest_toward_plus_infinity>(signaling_nan, 3u)) &&
			 exact_precision_proxy(comma_decimal<precision::fractional,
												 rounding::nearest_toward_minus_infinity, true>(signaling_nan, 3u)) &&
			 exact_precision_proxy(general<true, precision::significant,
										   rounding::nearest_toward_zero>(signaling_nan, 3u)) &&
			 exact_precision_proxy(general<precision::fractional,
										   rounding::nearest_away_from_zero, true>(signaling_nan, 3u)) &&
			 exact_precision_proxy(comma_general<true, precision::significant,
												 rounding::toward_plus_infinity>(signaling_nan, 3u)) &&
			 exact_precision_proxy(comma_general<precision::fractional,
												 rounding::toward_minus_infinity, true>(signaling_nan, 3u));

	result = result &&
			 exact_precision_proxy(fixed<true, precision::fractional,
										 rounding::toward_zero>(signaling_nan, 3u)) &&
			 exact_precision_proxy(fixed<precision::significant,
										 rounding::away_from_zero, true>(signaling_nan, 3u)) &&
			 exact_precision_proxy(comma_fixed<true, precision::fractional,
											   rounding::nearest_to_even>(signaling_nan, 3u)) &&
			 exact_precision_proxy(comma_fixed<precision::significant,
											   rounding::nearest_to_odd, true>(signaling_nan, 3u)) &&
			 exact_precision_proxy(scientific<true, precision::fractional,
											  rounding::nearest_toward_plus_infinity>(signaling_nan, 3u)) &&
			 exact_precision_proxy(scientific<precision::significant,
											  rounding::nearest_toward_minus_infinity, true>(signaling_nan, 3u)) &&
			 exact_precision_proxy(comma_scientific<true, precision::fractional,
													rounding::nearest_toward_zero>(signaling_nan, 3u)) &&
			 exact_precision_proxy(comma_scientific<precision::significant,
													rounding::nearest_away_from_zero, true>(signaling_nan, 3u));

	result = result &&
			 exact_precision_proxy(::fast_io::mnp::rounding<rounding::nearest_to_even>(
				 fixed(signaling_nan, 3u))) &&
			 exact_precision_proxy(::fast_io::mnp::rounding<rounding::nearest_to_odd>(
				 fixed(signaling_nan, 3u))) &&
			 exact_precision_proxy(::fast_io::mnp::rounding<
								   rounding::nearest_toward_plus_infinity>(
				 fixed(signaling_nan, 3u))) &&
			 exact_precision_proxy(::fast_io::mnp::rounding<
								   rounding::nearest_toward_minus_infinity>(
				 fixed(signaling_nan, 3u))) &&
			 exact_precision_proxy(::fast_io::mnp::rounding<rounding::nearest_toward_zero>(
				 fixed(signaling_nan, 3u))) &&
			 exact_precision_proxy(::fast_io::mnp::rounding<rounding::nearest_away_from_zero>(
				 fixed(signaling_nan, 3u))) &&
			 exact_precision_proxy(::fast_io::mnp::rounding<rounding::toward_plus_infinity>(
				 fixed(signaling_nan, 3u))) &&
			 exact_precision_proxy(::fast_io::mnp::rounding<rounding::toward_minus_infinity>(
				 fixed(signaling_nan, 3u))) &&
			 exact_precision_proxy(::fast_io::mnp::rounding<rounding::toward_zero>(
				 fixed(signaling_nan, 3u))) &&
			 exact_precision_proxy(::fast_io::mnp::rounding<rounding::away_from_zero>(
				 fixed(signaling_nan, 3u))) &&
			 exact_proxy(json_float(decimal(signaling_nan))) &&
			 exact_proxy(json_float<false>(decimal(signaling_nan)));
	return result;
}

static_assert(constructor_matrix());

template <::std::integral char_type, typename proxy_type>
[[nodiscard]] bool emits(proxy_type proxy) noexcept
{
	using clean_type = ::std::remove_cvref_t<proxy_type>;
	using tag = ::fast_io::io_reserve_type_t<char_type, clean_type>;
	char_type storage[512u]{};
	auto const size{print_reserve_precise_size(tag{}, proxy)};
	if (sizeof(storage) / sizeof(*storage) < size)
	{
		return false;
	}
	return print_reserve_precise_define(tag{}, storage, size, proxy) ==
		   storage + size;
}

#endif

} // namespace

int main()
{
#if defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE) && defined(__clang__) && \
	(defined(__x86_64__) || defined(_M_X64)) &&                       \
	!defined(__AVX512BF16__) &&                                      \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	auto const value{
		::std::bit_cast<__bf16>(::std::uint_least16_t{0x3fc0u})};
	auto const decimal{
		::fast_io::mnp::comma_decimal<
			true, precision::fractional,
			rounding::nearest_to_even>(value, 3u)};
	auto const hexadecimal{
		::fast_io::mnp::comma_hexfloat0x<
			true, precision::significant,
			rounding::nearest_to_even>(value, 3u)};
	return !(emits<char>(decimal) && emits<wchar_t>(decimal) &&
			 emits<char8_t>(decimal) && emits<char16_t>(hexadecimal) &&
			 emits<char32_t>(hexadecimal));
#else
	return 0;
#endif
}
