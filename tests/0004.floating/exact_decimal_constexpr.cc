#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include <fast_io.h>

namespace
{

template <typename floating_type, ::std::size_t expected_size,
		  ::std::int_least32_t expected_exponent>
[[nodiscard]] consteval bool check_layout(
	typename ::fast_io::details::iec559_traits<
		floating_type>::mantissa_type mantissa,
	::std::uint_least32_t exponent) noexcept
{
	auto const layout{
		::fast_io::details::exact_decimal_layout_from_binary<floating_type>(
			mantissa, exponent)};
	return layout.size == expected_size &&
		   layout.exponent == expected_exponent;
}

static_assert(check_layout<_Float16, 17u, -24>(1u, 0u));
static_assert(check_layout<float, 105u, -149>(1u, 0u));
static_assert(check_layout<double, 751u, -1074>(1u, 0u));

template <typename floating_type, ::std::uint_least64_t expected_hash>
[[nodiscard]] consteval bool check_decimal_hash(
	typename ::fast_io::details::iec559_traits<
		floating_type>::mantissa_type mantissa,
	::std::uint_least32_t exponent) noexcept
{
	auto const decimal{
		::fast_io::details::exact_decimal_from_binary<floating_type>(
			mantissa, exponent)};
	::std::uint_least64_t hash{UINT64_C(1469598103934665603)};
	for (::std::size_t index{}; index != decimal.size; ++index)
	{
		hash = (hash ^ decimal.digits[index]) * UINT64_C(1099511628211);
	}
	hash ^= static_cast<::std::uint_least64_t>(decimal.exponent);
	return hash == expected_hash;
}

static_assert(check_decimal_hash<_Float16,
								 UINT64_C(13539654618359975597)>(1u, 0u));
static_assert(check_decimal_hash<float,
								 UINT64_C(14212036803821002760)>(1u, 0u));
static_assert(check_decimal_hash<double,
								 UINT64_C(15966028217008695624)>(1u, 0u));
static_assert(check_decimal_hash<double,
								 UINT64_C(15118341019182984374)>(
	(static_cast<::std::uint_least64_t>(1u) << 52u) - 1u, 2046u));

#if defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE) ||  \
	(defined(__GNUC__) && !defined(__clang__) && \
	 defined(__BFLT16_MANT_DIG__) && __BFLT16_MANT_DIG__ == 8)
static_assert(check_layout<__bf16, 93u, -133>(1u, 0u));
static_assert(check_decimal_hash<__bf16,
								 UINT64_C(10558733490027363591)>(1u, 0u));
#endif

#if defined(__LDBL_MANT_DIG__) && __LDBL_MANT_DIG__ == 64 && \
	defined(__LDBL_MAX_EXP__) && __LDBL_MAX_EXP__ == 16384
static_assert(check_layout<long double, 11495u, -16445>(1u, 0u));
static_assert(check_decimal_hash<long double,
								 UINT64_C(6211380206846176672)>(1u, 0u));
static_assert(check_decimal_hash<long double,
								 UINT64_C(15567391464451687705)>(
	(static_cast<::std::uint_least64_t>(1u) << 63u) - 1u, 32766u));
#endif

#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
static_assert(check_layout<__float128, 11529u, -16494>(1u, 0u));
static_assert(check_decimal_hash<__float128,
								 UINT64_C(9267228990344584943)>(1u, 0u));
static_assert(check_decimal_hash<__float128,
								 UINT64_C(9524673291707184476)>(
	(static_cast<__uint128_t>(1u) << 112u) - 1u, 32766u));
#endif

template <::std::integral char_type, typename printable_type>
[[nodiscard]] consteval ::std::size_t precise_size_of(
	printable_type const &value) noexcept
{
	using clean_type = ::std::remove_cvref_t<printable_type>;
	return print_reserve_precise_size(
		::fast_io::io_reserve_type_t<char_type, clean_type>{}, value);
}

[[nodiscard]] consteval bool check_precise_sizes() noexcept
{
	using namespace ::fast_io::mnp;
	constexpr auto float_min{
		(::std::numeric_limits<float>::denorm_min)()};
	constexpr auto double_min{
		(::std::numeric_limits<double>::denorm_min)()};
	constexpr auto double_max{
		(::std::numeric_limits<double>::max)()};
	constexpr auto double_infinity{
		(::std::numeric_limits<double>::infinity)()};
	constexpr auto fixed_float_min{exact_decimal(fixed(float_min))};
	constexpr auto decimal_float_min{exact_decimal(decimal(float_min))};
	constexpr auto fixed_double_min{exact_decimal(fixed(double_min))};
	constexpr auto decimal_double_min{exact_decimal(decimal(double_min))};
	constexpr auto fixed_double_max{exact_decimal(fixed(double_max))};
	constexpr auto json_one{json_float(exact_decimal(decimal(1.0)))};
	constexpr auto negative_zero{exact_decimal(-0.0)};
	constexpr auto infinity{exact_decimal(double_infinity)};
	return precise_size_of<char>(fixed_float_min) == 151u &&
		   precise_size_of<char>(decimal_float_min) == 110u &&
		   precise_size_of<char>(fixed_double_min) == 1076u &&
		   precise_size_of<wchar_t>(fixed_double_min) == 1076u &&
		   precise_size_of<char8_t>(fixed_double_min) == 1076u &&
		   precise_size_of<char16_t>(fixed_double_min) == 1076u &&
		   precise_size_of<char32_t>(fixed_double_min) == 1076u &&
		   precise_size_of<char>(decimal_double_min) == 757u &&
		   precise_size_of<char>(fixed_double_max) == 309u &&
		   precise_size_of<char>(json_one) == 3u &&
		   precise_size_of<char>(negative_zero) == 2u &&
		   precise_size_of<char>(infinity) == 3u;
}

static_assert(check_precise_sizes());

template <::std::size_t extent, typename printable_type>
[[nodiscard]] consteval bool check_spelling(
	printable_type const &value, char const (&expected)[extent]) noexcept
{
	using clean_type = ::std::remove_cvref_t<printable_type>;
	using tag = ::fast_io::io_reserve_type_t<char, clean_type>;
	auto const precise_size{print_reserve_precise_size(tag{}, value)};
	if (precise_size != extent - 1u)
	{
		return false;
	}
	char storage[extent]{};
	auto *const end{print_reserve_precise_define(
		tag{}, storage, precise_size, value)};
	if (end != storage + precise_size)
	{
		return false;
	}
	for (::std::size_t index{}; index != precise_size; ++index)
	{
		if (storage[index] != expected[index])
		{
			return false;
		}
	}
	return true;
}

[[nodiscard]] consteval bool check_spellings() noexcept
{
	using namespace ::fast_io::mnp;
	return check_spelling(
			   exact_decimal(0.1),
			   "0.1000000000000000055511151231257827021181583404541015625") &&
		   check_spelling(exact_decimal(scientific(0.5)), "5e-01") &&
		   check_spelling(json_float(exact_decimal(decimal(1.0))), "1.0") &&
		   check_spelling(exact_decimal(-0.0), "-0");
}

static_assert(check_spellings());

} // namespace

int main()
{}
