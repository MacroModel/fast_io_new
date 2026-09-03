#include <bit>
#include <cstddef>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#include <fast_io.h>

namespace
{

template <typename value_type>
concept can_apply_rounding = requires(value_type value) {
	::fast_io::mnp::rounding<
		::fast_io::manipulators::floating_rounding::nearest_to_even>(value);
};

template <typename value_type>
concept can_apply_exact_decimal = requires(value_type value) {
	::fast_io::mnp::exact_decimal(value);
};

using exact_double_type = decltype(::fast_io::mnp::exact_decimal(0.1));
static_assert(!can_apply_rounding<exact_double_type>);
using directed_double_type = decltype(::fast_io::mnp::rounding<
									  ::fast_io::manipulators::floating_rounding::toward_zero>(
	::fast_io::mnp::decimal(0.1)));
static_assert(!can_apply_exact_decimal<directed_double_type>);

[[nodiscard]] consteval bool check_consteval_exact_decimal()
{
	using namespace ::fast_io::mnp;
	constexpr auto value{exact_decimal(0.5)};
	using value_type = ::std::remove_cvref_t<decltype(value)>;
	char storage[print_reserve_size(
		::fast_io::io_reserve_type<char, value_type>)]{};
	auto *const end{print_reserve_define(
		::fast_io::io_reserve_type<char, value_type>, storage, value)};
	return end == storage + 3u && storage[0] == '0' &&
		   storage[1] == '.' && storage[2] == '5';
}

static_assert(check_consteval_exact_decimal());

template <typename char_type>
[[nodiscard]] bool equals_ascii(
	::std::basic_string<char_type> const &text, char const *expected) noexcept
{
	::std::size_t size{};
	for (; expected[size]; ++size)
	{
	}
	if (text.size() != size)
	{
		return false;
	}
	for (::std::size_t index{}; index != size; ++index)
	{
		auto const ascii{static_cast<unsigned char>(expected[index])};
		char_type translated{};
		if (u8'0' <= ascii && ascii <= u8'9')
		{
			translated = ::fast_io::char_literal_add<char_type>(ascii - u8'0');
		}
		else
		{
			switch (ascii)
			{
			case u8'.':
				translated = ::fast_io::char_literal_v<u8'.', char_type>;
				break;
			case u8',':
				translated = ::fast_io::char_literal_v<u8',', char_type>;
				break;
			case u8'+':
				translated = ::fast_io::char_literal_v<u8'+', char_type>;
				break;
			case u8'-':
				translated = ::fast_io::char_literal_v<u8'-', char_type>;
				break;
			case u8'e':
				translated = ::fast_io::char_literal_v<u8'e', char_type>;
				break;
			case u8'E':
				translated = ::fast_io::char_literal_v<u8'E', char_type>;
				break;
			case u8'i':
				translated = ::fast_io::char_literal_v<u8'i', char_type>;
				break;
			case u8'n':
				translated = ::fast_io::char_literal_v<u8'n', char_type>;
				break;
			case u8'f':
				translated = ::fast_io::char_literal_v<u8'f', char_type>;
				break;
			default:
				return false;
			}
		}
		if (text[index] != translated)
		{
			return false;
		}
	}
	return true;
}

template <typename char_type, typename value_type>
[[nodiscard]] auto concat_exact(value_type value)
{
	using namespace ::fast_io::mnp;
	if constexpr (::std::same_as<char_type, char>)
	{
		return ::fast_io::concat_std(exact_decimal(value));
	}
	else if constexpr (::std::same_as<char_type, wchar_t>)
	{
		return ::fast_io::wconcat_std(exact_decimal(value));
	}
	else if constexpr (::std::same_as<char_type, char8_t>)
	{
		return ::fast_io::u8concat_std(exact_decimal(value));
	}
	else if constexpr (::std::same_as<char_type, char16_t>)
	{
		return ::fast_io::u16concat_std(exact_decimal(value));
	}
	else
	{
		return ::fast_io::u32concat_std(exact_decimal(value));
	}
}

template <typename char_type>
[[nodiscard]] bool check_character_type()
{
	return equals_ascii(
			   concat_exact<char_type>(0.1),
			   "0.1000000000000000055511151231257827021181583404541015625") &&
		   equals_ascii(
			   concat_exact<char_type>(0.1f),
			   "0.100000001490116119384765625");
}

[[nodiscard]] bool check_presentations()
{
	using namespace ::fast_io::mnp;
	return ::fast_io::concat_std(exact_decimal(scientific(0.1))) ==
			   "1.000000000000000055511151231257827021181583404541015625e-01" &&
		   ::fast_io::concat_std(exact_decimal(scientific<true>(0.1))) ==
			   "1.000000000000000055511151231257827021181583404541015625E-01" &&
		   ::fast_io::concat_std(exact_decimal(fixed(0.1))) ==
			   "0.1000000000000000055511151231257827021181583404541015625" &&
		   ::fast_io::concat_std(exact_decimal(comma_fixed(0.1))) ==
			   "0,1000000000000000055511151231257827021181583404541015625" &&
		   ::fast_io::concat_std(exact_decimal(general(0.1))) ==
			   "0.1000000000000000055511151231257827021181583404541015625" &&
		   ::fast_io::concat_std(json_float(exact_decimal(1.0))) == "1.0" &&
		   ::fast_io::concat_std(
			   json_float(exact_decimal(comma_decimal(1.0)))) == "1,0" &&
		   ::fast_io::concat_std(exact_decimal(json_float(decimal(1.0)))) == "1.0" &&
		   ::fast_io::concat_std(exact_decimal(-0.0)) == "-0" &&
		   ::fast_io::concat_std(json_float(exact_decimal(-0.0))) == "-0.0" &&
		   ::fast_io::concat_std(exact_decimal(scientific(-0.0))) == "-0e+00" &&
		   ::fast_io::concat_std(exact_decimal(
			   (::std::numeric_limits<double>::infinity)())) == "inf";
}

[[nodiscard]] bool check_exact_buffer_protocol()
{
	using namespace ::fast_io::mnp;
	auto const value{exact_decimal(scientific(0.1))};
	using value_type = ::std::remove_cvref_t<decltype(value)>;
	auto const size{print_reserve_precise_size(
		::fast_io::io_reserve_type<char, value_type>, value)};
	::std::vector<char> storage(size);
	auto *const end{print_reserve_precise_define(
		::fast_io::io_reserve_type<char, value_type>, storage.data(), size,
		value)};
	auto const expected{
		"1.000000000000000055511151231257827021181583404541015625e-01"};
	if (end != storage.data() + size ||
		::std::string(storage.data(), storage.size()) != expected)
	{
		return false;
	}
	/* The output area is exactly the logical spelling (far below the type-level
	reserve bound), so this also guards against fixed-width ASCII over-stores. */
	::std::vector<char> print_storage(size);
	::fast_io::basic_obuffer_view<char> output{
		print_storage.data(), print_storage.data() + print_storage.size()};
	::fast_io::print(output, value);
	if (output.size() != size ||
		::std::string(print_storage.data(), print_storage.size()) != expected)
	{
		return false;
	}
	::std::vector<char> exact_fit(size + 1u, '!');
	auto const converted{::fast_io::to_chars(
		exact_fit.data(), exact_fit.data() + size, value)};
	if (converted.ec != ::std::errc{} ||
		converted.ptr != exact_fit.data() + size || exact_fit[size] != '!' ||
		::std::string(exact_fit.data(), size) != expected)
	{
		return false;
	}
	::std::vector<char> too_small(size, '!');
	auto const rejected{::fast_io::to_chars(
		too_small.data(), too_small.data() + size - 1u, value)};
	if (rejected.ec != ::std::errc::value_too_large ||
		rejected.ptr != too_small.data() + size - 1u)
	{
		return false;
	}
	for (auto const character : too_small)
	{
		if (character != '!')
		{
			return false;
		}
	}
	return true;
}

template <typename value_type>
[[nodiscard]] bool precise_matches_ordinary(value_type const &value)
{
	using clean_type = ::std::remove_cvref_t<value_type>;
	using tag = ::fast_io::io_reserve_type_t<char, clean_type>;
	auto const bound{print_reserve_size(tag{})};
	::std::vector<char> ordinary(bound);
	auto *const ordinary_end{
		print_reserve_define(tag{}, ordinary.data(), value)};
	auto const precise_size{print_reserve_precise_size(tag{}, value)};
	::std::vector<char> precise(precise_size + 2u, '!');
	auto *const precise_end{print_reserve_precise_define(
		tag{}, precise.data(), precise_size, value)};
	if (ordinary_end != ordinary.data() + precise_size ||
		precise_end != precise.data() + precise_size ||
		precise[precise_size] != '!' || precise[precise_size + 1u] != '!')
	{
		return false;
	}
	for (::std::size_t index{}; index != precise_size; ++index)
	{
		if (ordinary[index] != precise[index])
		{
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool check_precise_layout_matrix()
{
	using namespace ::fast_io::mnp;
	auto const infinity{(::std::numeric_limits<double>::infinity)()};
	auto const quiet_nan{(::std::numeric_limits<double>::quiet_NaN)()};
	return precise_matches_ordinary(exact_decimal(fixed(0.1))) &&
		   precise_matches_ordinary(exact_decimal(scientific(0.1))) &&
		   precise_matches_ordinary(exact_decimal(general(1.0e20))) &&
		   precise_matches_ordinary(exact_decimal(decimal(1.0e-20))) &&
		   precise_matches_ordinary(exact_decimal(fixed(1024.0))) &&
		   precise_matches_ordinary(json_float(exact_decimal(decimal(1024.0)))) &&
		   precise_matches_ordinary(json_float(exact_decimal(decimal(9000.0)))) &&
		   precise_matches_ordinary(exact_decimal(-0.0)) &&
		   precise_matches_ordinary(exact_decimal(scientific(-0.0))) &&
		   precise_matches_ordinary(exact_decimal(infinity)) &&
		   precise_matches_ordinary(exact_decimal(-quiet_nan));
}

[[nodiscard]] bool check_extremes()
{
	using namespace ::fast_io::mnp;
	auto const minimum{::fast_io::concat_std(exact_decimal(fixed(
		(::std::numeric_limits<double>::denorm_min)())))};
	/* 2^-1074 has 1074 fractional places and a nonzero final digit. */
	return minimum.size() == 1076u && minimum[0] == '0' &&
		   minimum[1] == '.' && minimum.back() == '5';
}

template <typename floating_type>
[[nodiscard]] bool check_wide_minimum(floating_type minimum)
{
	if constexpr (::fast_io::details::
					  print_floating_decimal_exact_supported<floating_type>)
	{
		using namespace ::fast_io::mnp;
		using trait = ::fast_io::details::iec559_traits<floating_type>;
		constexpr ::std::size_t bias{
			(static_cast<::std::size_t>(1u) << (trait::ebits - 1u)) - 1u};
		constexpr ::std::size_t denominator_power{bias + trait::mbits - 1u};
		auto const text{::fast_io::concat_std(exact_decimal(fixed(minimum)))};
		return text.size() == denominator_power + 2u && text[0] == '0' &&
			   text[1] == '.' && text.back() == '5';
	}
	else
	{
		return true;
	}
}

template <typename floating_type>
[[nodiscard]] bool exact_backends_equal(floating_type value)
{
	if constexpr (::fast_io::details::
					  print_floating_decimal_exact_supported<floating_type>)
	{
		auto const fields{::fast_io::details::get_punned_result(value)};
		auto const anchored{
			::fast_io::details::exact_decimal_from_binary<floating_type>(
				fields.mantissa, fields.exponent)};
		auto const original{
			::fast_io::details::exact_precision_from_binary<floating_type>(
				fields.mantissa, fields.exponent)};
		if (anchored.size != original.size ||
			anchored.exponent != original.exponent)
		{
			return false;
		}
		for (::std::size_t index{}; index != anchored.size; ++index)
		{
			if (anchored.digits[index] != original.digits[index])
			{
				return false;
			}
		}
	}
	return true;
}

[[nodiscard]] bool check_wide_types()
{
	auto const long_double_minimum{
		(::std::numeric_limits<long double>::denorm_min)()};
	if (!check_wide_minimum(long_double_minimum) ||
		!exact_backends_equal(long_double_minimum) ||
		!exact_backends_equal(static_cast<long double>(0.1L)) ||
		!exact_backends_equal((::std::numeric_limits<long double>::min)()) ||
		!exact_backends_equal((::std::numeric_limits<long double>::max)()))
	{
		return false;
	}
#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16 &&     \
	(!defined(__FLT128_MANT_DIG__) || __FLT128_MANT_DIG__ == 113)
	auto const binary128_minimum{::std::bit_cast<__float128>(
		static_cast<__uint128_t>(1u))};
	if (!check_wide_minimum(binary128_minimum))
	{
		return false;
	}
	constexpr ::std::uint_least32_t exponents[]{
		0u, 1u, 2u, 127u, 1023u, 4095u, 8191u, 12287u, 15359u,
		16382u, 16383u, 16384u, 20000u, 24575u, 30000u, 32766u};
	for (auto const exponent : exponents)
	{
		auto const fraction{
			(static_cast<__uint128_t>(exponent) *
			 static_cast<__uint128_t>(UINT64_C(0x9e3779b97f4a7c15))) &
			((static_cast<__uint128_t>(1u) << 112u) - 1u)};
		auto const bits{(static_cast<__uint128_t>(exponent) << 112u) |
						(fraction ? fraction : 1u)};
		if (!exact_backends_equal(::std::bit_cast<__float128>(bits)))
		{
			return false;
		}
	}
	return true;
#else
	return true;
#endif
}

#if defined(__FLT16_MANT_DIG__) && __FLT16_MANT_DIG__ == 11
[[nodiscard]] bool check_binary16()
{
	using namespace ::fast_io::mnp;
	return ::fast_io::concat_std(exact_decimal(static_cast<_Float16>(0.1))) ==
		   "0.0999755859375";
}
#else
[[nodiscard]] constexpr bool check_binary16() noexcept
{
	return true;
}
#endif

#if defined(__GNUC__) && !defined(__clang__) && \
	defined(__BFLT16_MANT_DIG__) && __BFLT16_MANT_DIG__ == 8
[[nodiscard]] bool check_bfloat16()
{
	using namespace ::fast_io::mnp;
	return ::fast_io::concat_std(
			   exact_decimal(static_cast<__bf16>(0.1f))) == "0.10009765625";
}
#else
[[nodiscard]] constexpr bool check_bfloat16() noexcept
{
	return true;
}
#endif

} // namespace

int main()
{
	return !(check_character_type<char>() &&
			 check_character_type<wchar_t>() &&
			 check_character_type<char8_t>() &&
			 check_character_type<char16_t>() &&
			 check_character_type<char32_t>() &&
			 check_presentations() && check_exact_buffer_protocol() &&
			 check_precise_layout_matrix() && check_extremes() &&
			 check_binary16() && check_bfloat16() && check_wide_types());
}
