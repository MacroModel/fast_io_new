#include <cstddef>
#include <cstdint>

#include <fast_io.h>

namespace
{

template <typename floating_type>
[[nodiscard]] bool equal_backends(
	typename ::fast_io::details::iec559_traits<floating_type>::mantissa_type
		mantissa,
	::std::uint_least32_t exponent) noexcept
{
	auto const optimized{
		::fast_io::details::exact_decimal_from_binary<floating_type>(
			mantissa, exponent)};
	auto const reference{
		::fast_io::details::exact_precision_from_binary<floating_type>(
			mantissa, exponent)};
	if (optimized.size != reference.size ||
		optimized.exponent != reference.exponent)
	{
		return false;
	}
	for (::std::size_t index{}; index != optimized.size; ++index)
	{
		if (optimized.digits[index] != reference.digits[index])
		{
			return false;
		}
	}
	return true;
}

template <typename floating_type>
[[nodiscard]] bool check_every_finite_exponent() noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr auto mantissa_mask{
		(static_cast<mantissa_type>(1u) << trait::mbits) - 1u};
	constexpr auto exponent_mask{
		(static_cast<::std::uint_least32_t>(1u) << trait::ebits) - 1u};
	for (::std::uint_least32_t exponent{}; exponent != exponent_mask;
		 ++exponent)
	{
		mantissa_type const mantissas[]{
			static_cast<mantissa_type>(1u),
			static_cast<mantissa_type>(
				(mantissa_mask / static_cast<mantissa_type>(3u)) |
				static_cast<mantissa_type>(1u)),
			mantissa_mask};
		for (auto const mantissa : mantissas)
		{
			if (!equal_backends<floating_type>(mantissa, exponent))
			{
				return false;
			}
		}
	}
	return true;
}

} // namespace

int main()
{
	if (!check_every_finite_exponent<float>())
	{
		return 1;
	}
	if (!check_every_finite_exponent<double>())
	{
		return 2;
	}
#if defined(__LDBL_MANT_DIG__) && __LDBL_MANT_DIG__ == 64 && \
	defined(__LDBL_MAX_EXP__) && __LDBL_MAX_EXP__ == 16384
	if (!check_every_finite_exponent<long double>())
	{
		return 3;
	}
#endif
#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
	if (!check_every_finite_exponent<__float128>())
	{
		return 4;
	}
#endif
}
