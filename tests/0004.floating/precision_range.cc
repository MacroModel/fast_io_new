#include <bit>
#include <cfenv>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#define FAST_IO_ENABLE_FLOATING_POINT_ENVIRONMENT 1
#include <fast_io.h>

namespace
{

template <typename value_type>
concept can_apply_exact_decimal = requires(value_type value) {
	::fast_io::mnp::exact_decimal(value);
};

template <typename value_type>
concept can_apply_precision_range = requires(value_type value) {
	::fast_io::mnp::precision_range(value, 1u, 17u);
};

using range_type = decltype(::fast_io::mnp::precision_range(0.1, 1u, 17u));
using exact_type = decltype(::fast_io::mnp::exact_decimal(0.1));
static_assert(!can_apply_exact_decimal<range_type>);
static_assert(!can_apply_precision_range<exact_type>);

[[nodiscard]] consteval bool check_consteval_precision_range()
{
	using namespace ::fast_io::mnp;
	constexpr auto value{precision_range(decimal(0.5), 3u, 5u)};
	using value_type = ::std::remove_cvref_t<decltype(value)>;
	char storage[32u]{};
	auto *const end{print_reserve_define(
		::fast_io::io_reserve_type<char, value_type>, storage, value)};
	return end == storage + 5u && storage[0] == '0' &&
		   storage[1] == '.' && storage[2] == '5' && storage[3] == '0' &&
		   storage[4] == '0';
}

static_assert(check_consteval_precision_range());

template <typename char_type>
[[nodiscard]] bool check_character_type()
{
	using namespace ::fast_io::mnp;
	auto const value{precision_range(decimal(0.1), 3u, 17u)};
	if constexpr (::std::same_as<char_type, char>)
	{
		return ::fast_io::concat_std(value) == "0.100";
	}
	else if constexpr (::std::same_as<char_type, wchar_t>)
	{
		return ::fast_io::wconcat_std(value) == L"0.100";
	}
	else if constexpr (::std::same_as<char_type, char8_t>)
	{
		return ::fast_io::u8concat_std(value) == u8"0.100";
	}
	else if constexpr (::std::same_as<char_type, char16_t>)
	{
		return ::fast_io::u16concat_std(value) == u"0.100";
	}
	else
	{
		return ::fast_io::u32concat_std(value) == U"0.100";
	}
}

[[nodiscard]] bool check_semantics()
{
	using namespace ::fast_io::mnp;
	using rounding_mode = ::fast_io::manipulators::floating_rounding;
	return ::fast_io::concat_std(
			   precision_range(decimal(0.1), 1u, 17u)) == "0.1" &&
		   ::fast_io::concat_std(
			   precision_range(decimal(0.1), 3u, 17u)) == "0.100" &&
		   ::fast_io::concat_std(
			   precision_range(decimal(1.234567), 1u, 5u)) == "1.2346" &&
		   ::fast_io::concat_std(rounding<rounding_mode::toward_zero>(
			   precision_range(decimal(1.234567), 1u, 5u))) == "1.2345" &&
		   ::fast_io::concat_std(precision_range(
			   rounding<rounding_mode::toward_zero>(decimal(1.234567)), 1u, 5u)) ==
			   "1.2345" &&
		   ::fast_io::concat_std(
			   precision_range(scientific(0.1), 3u, 17u)) == "1.00e-01" &&
		   ::fast_io::concat_std(json_float(
			   precision_range(decimal(1.0), 1u, 17u))) == "1.0" &&
		   ::fast_io::concat_std(json_float(
			   precision_range(decimal(1.0), 3u, 17u))) == "1.00";
}

[[nodiscard]] bool check_buffer_protocol()
{
	using namespace ::fast_io::mnp;
	auto const value{precision_range(decimal(1.234567), 1u, 5u)};
	using value_type = ::std::remove_cvref_t<decltype(value)>;
	using tag = ::fast_io::io_reserve_type_t<char, value_type>;
	auto const precise_size{print_reserve_precise_size(tag{}, value)};
	std::vector<char> precise(precise_size + 1u, '!');
	auto *const precise_end{print_reserve_precise_define(
		tag{}, precise.data(), precise_size, value)};
	if (precise_end != precise.data() + precise_size ||
		precise[precise_size] != '!' ||
		std::string(precise.data(), precise_size) != "1.2346")
	{
		return false;
	}
	std::vector<char> exact_fit(precise_size + 1u, '!');
	auto const converted{::fast_io::to_chars(
		exact_fit.data(), exact_fit.data() + precise_size, value)};
	if (converted.ec != ::std::errc{} ||
		converted.ptr != exact_fit.data() + precise_size ||
		exact_fit[precise_size] != '!' ||
		std::string(exact_fit.data(), precise_size) != "1.2346")
	{
		return false;
	}
	std::vector<char> too_small(precise_size + 1u, '!');
	auto const rejected{::fast_io::to_chars(
		too_small.data(), too_small.data() + precise_size - 1u, value)};
	if (rejected.ec != ::std::errc::value_too_large ||
		rejected.ptr != too_small.data() + precise_size - 1u)
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
	return ::fast_io::concat_std(
			   precision_range(decimal(0.1), 0u, 17u)) == "0.1" &&
		   ::fast_io::concat_std(precision_range(
			   decimal((::std::numeric_limits<double>::infinity)()), 3u,
			   5u)) == "inf";
}

[[nodiscard]] bool check_current_environment()
{
	using namespace ::fast_io::mnp;
	using rounding_mode = ::fast_io::manipulators::floating_rounding;
	auto const original{::std::fegetround()};
	if (::std::fesetround(FE_TOWARDZERO))
	{
		return true;
	}
	auto const result{::fast_io::concat_std(
		rounding<rounding_mode::current_environment>(
			precision_range(decimal(1.234567), 1u, 5u)))};
	(void)::std::fesetround(original);
	return result == "1.2345";
}

template <typename printable_type>
[[nodiscard]] bool precise_matches_ordinary(
	printable_type const &value) noexcept
{
	using clean_type = ::std::remove_cvref_t<printable_type>;
	using tag = ::fast_io::io_reserve_type_t<char, clean_type>;
	char ordinary[1024u]{};
	char precise[1024u];
	for (auto &character : precise)
	{
		character = '!';
	}
	auto const reserve_size{print_reserve_size(tag{}, value)};
	if (sizeof(ordinary) < reserve_size)
	{
		return false;
	}
	auto *const ordinary_end{
		print_reserve_define(tag{}, ordinary, value)};
	auto const precise_size{print_reserve_precise_size(tag{}, value)};
	if (sizeof(precise) <= precise_size)
	{
		return false;
	}
	auto *const precise_end{print_reserve_precise_define(
		tag{}, precise, precise_size, value)};
	if (ordinary_end != ordinary + precise_size ||
		precise_end != precise + precise_size || precise[precise_size] != '!')
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

template <typename left_type, typename right_type>
[[nodiscard]] bool spellings_equal(
	left_type const &left, right_type const &right) noexcept
{
	using left_tag = ::fast_io::io_reserve_type_t<char, left_type>;
	using right_tag = ::fast_io::io_reserve_type_t<char, right_type>;
	char left_storage[1024u]{};
	char right_storage[1024u]{};
	auto *const left_end{
		print_reserve_define(left_tag{}, left_storage, left)};
	auto *const right_end{
		print_reserve_define(right_tag{}, right_storage, right)};
	auto const size{static_cast<::std::size_t>(left_end - left_storage)};
	if (right_end != right_storage + size)
	{
		return false;
	}
	for (::std::size_t index{}; index != size; ++index)
	{
		if (left_storage[index] != right_storage[index])
		{
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool check_differential_samples()
{
	using namespace ::fast_io::mnp;
	using rounding_mode = ::fast_io::manipulators::floating_rounding;
	::std::uint_least64_t state{UINT64_C(0x243f6a8885a308d3)};
	for (::std::size_t index{}; index != 8192u; ++index)
	{
		state ^= state << 13u;
		state ^= state >> 7u;
		state ^= state << 17u;
		auto const minimum{static_cast<::std::size_t>((state >> 9u) & 7u)};
		auto const normalized_minimum{minimum ? minimum : 1u};
		auto const maximum{normalized_minimum +
						   static_cast<::std::size_t>((state >> 17u) & 15u)};
		auto const double_value{::std::bit_cast<double>(state)};
		auto const float_value{::std::bit_cast<float>(
			static_cast<::std::uint_least32_t>(state))};
		auto const double_decimal{decimal(double_value)};
		auto const float_general{general(float_value)};
		if (!spellings_equal(
				double_decimal,
				precision_range(double_decimal, 1u, 40u)) ||
			!spellings_equal(
				float_general,
				precision_range(float_general, 1u, 20u)) ||
			!precise_matches_ordinary(precision_range(
				decimal(double_value), minimum, maximum)) ||
			!precise_matches_ordinary(precision_range(
				general(float_value), minimum, maximum)) ||
			!precise_matches_ordinary(precision_range(
				scientific(double_value), minimum, maximum)) ||
			!precise_matches_ordinary(rounding<rounding_mode::toward_zero>(
				precision_range(fixed(float_value), minimum, maximum))) ||
			!precise_matches_ordinary(json_float(precision_range(
				decimal(double_value), minimum, maximum))))
		{
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool check_wide()
{
	using namespace ::fast_io::mnp;
	bool result{true};
	if constexpr (::fast_io::details::
					  print_floating_decimal_exact_supported<long double>)
	{
		result = result && ::fast_io::concat_std(
							   precision_range(decimal(static_cast<long double>(1.25L)),
											   3u, 40u)) == "1.25";
	}
#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16 &&     \
	(!defined(__FLT128_MANT_DIG__) || __FLT128_MANT_DIG__ == 113)
	result = result && ::fast_io::concat_std(precision_range(
						   decimal(static_cast<__float128>(1.25L)), 3u, 40u)) == "1.25";
#endif
	return result;
}

[[nodiscard]] bool check_narrow()
{
	using namespace ::fast_io::mnp;
	bool result{true};
#if defined(__FLT16_MANT_DIG__) && __FLT16_MANT_DIG__ == 11
	result = result && ::fast_io::concat_std(precision_range(
						   decimal(static_cast<_Float16>(0.1)), 3u, 5u)) == "0.100";
#endif
#if defined(__GNUC__) && !defined(__clang__) && \
	defined(__BFLT16_MANT_DIG__) && __BFLT16_MANT_DIG__ == 8
	result = result && ::fast_io::concat_std(precision_range(
						   decimal(static_cast<__bf16>(0.1f)), 3u, 5u)) == "0.100";
#endif
	return result;
}

} // namespace

int main()
{
	return !(check_character_type<char>() &&
			 check_character_type<wchar_t>() &&
			 check_character_type<char8_t>() &&
			 check_character_type<char16_t>() &&
			 check_character_type<char32_t>() && check_semantics() &&
			 check_buffer_protocol() && check_current_environment() &&
			 check_differential_samples() && check_narrow() && check_wide());
}
