#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <fast_io.h>

struct precise_size_synthetic_ibm_double_double
{
	unsigned char storage[16u];
};

#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
namespace fast_io::details
{

template <>
struct iec559_traits<::precise_size_synthetic_ibm_double_double>
{
	using mantissa_type = __uint128_t;
	inline static constexpr ::std::size_t mbits{105u};
	inline static constexpr ::std::size_t ebits{11u};
	inline static constexpr ::std::uint_least32_t m10digits{33u};
	inline static constexpr ::std::uint_least32_t m2hexdigits{27u};
	inline static constexpr ::std::uint_least32_t e10digits{3u};
	inline static constexpr ::std::uint_least32_t e2hexdigits{4u};
	inline static constexpr ::std::uint_least32_t e10max{308u};
};

} // namespace fast_io::details
#endif

namespace
{

[[nodiscard]] ::std::uint_least64_t load64(
	unsigned char const *data, ::std::size_t size,
	::std::size_t offset = 0u) noexcept
{
	::std::uint_least64_t value{};
	for (::std::size_t index{}; index != 8u; ++index)
	{
		value |= static_cast<::std::uint_least64_t>(
					 index + offset < size ? data[index + offset] : 0u)
				 << (index * 8u);
	}
	return value;
}

template <typename floating_type>
[[nodiscard]] bool check_layout(
	typename ::fast_io::details::iec559_traits<
		floating_type>::mantissa_type mantissa,
	::std::uint_least32_t exponent) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	constexpr auto exponent_mask{
		(static_cast<::std::uint_least32_t>(1u) << trait::ebits) - 1u};
	if (exponent == exponent_mask || (!exponent && !mantissa))
	{
		return true;
	}
	auto const metadata{
		::fast_io::details::floating_precise_exact_decimal_layout_from_binary<
			floating_type>(mantissa, exponent)};
	auto const limb_layout{
		::fast_io::details::exact_decimal_layout_from_binary<floating_type>(
			mantissa, exponent)};
	auto const reference{
		::fast_io::details::exact_precision_from_binary<floating_type>(
			mantissa, exponent)};
	return metadata.size == reference.size &&
		   metadata.exponent == reference.exponent &&
		   limb_layout.size == reference.size &&
		   limb_layout.exponent == reference.exponent;
}

[[nodiscard]] unsigned selected_domain() noexcept
{
	static unsigned const domain{[]() noexcept {
		auto const *const text{::std::getenv("FAST_IO_PRECISE_SIZE_FUZZ_DOMAIN")};
		if (text == nullptr || *text == '\0')
		{
			return 2u;
		}
		char *end{};
		auto const value{::std::strtoul(text, __builtin_addressof(end), 10)};
		if (end == text || *end != '\0' || 6u < value)
		{
			::std::abort();
		}
		return static_cast<unsigned>(value);
	}()};
	return domain;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(unsigned char const *data,
									  ::std::size_t size)
{
	auto const low{load64(data, size)};
	auto const high{load64(data, size, 8u)};
	bool result{};
	switch (selected_domain())
	{
	case 0u:
#if defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE)
		result = check_layout<__bf16>(
			static_cast<::std::uint_least16_t>(low & 0x7fu),
			static_cast<::std::uint_least32_t>((low >> 7u) & 0xffu));
#else
		result = true;
#endif
		break;
	case 1u:
#if defined(__FLT16_MANT_DIG__) && __FLT16_MANT_DIG__ == 11
		result = check_layout<_Float16>(
			static_cast<::std::uint_least16_t>(low & 0x3ffu),
			static_cast<::std::uint_least32_t>((low >> 10u) & 0x1fu));
#else
		result = true;
#endif
		break;
	case 2u:
		result = check_layout<float>(
			static_cast<::std::uint_least32_t>(low & 0x7fffffu),
			static_cast<::std::uint_least32_t>((low >> 23u) & 0xffu));
		break;
	case 3u:
		result = check_layout<double>(
			low & UINT64_C(0x000fffffffffffff),
			static_cast<::std::uint_least32_t>((low >> 52u) & 0x7ffu));
		break;
	case 4u:
#if defined(__LDBL_MANT_DIG__) && __LDBL_MANT_DIG__ == 64 && \
	defined(__LDBL_MAX_EXP__) && __LDBL_MAX_EXP__ == 16384
		result = check_layout<long double>(
			low & UINT64_C(0x7fffffffffffffff),
			static_cast<::std::uint_least32_t>(high & 0x7fffu));
#else
		result = true;
#endif
		break;
	case 5u:
#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
		result = check_layout<__float128>(
			(static_cast<__uint128_t>(high & UINT64_C(0x0000ffffffffffff))
			 << 64u) |
				low,
			static_cast<::std::uint_least32_t>((high >> 48u) & 0x7fffu));
#else
		result = true;
#endif
		break;
	default:
#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
		result = check_layout<::precise_size_synthetic_ibm_double_double>(
			((static_cast<__uint128_t>(high) << 64u) | low) &
				((static_cast<__uint128_t>(1u) << 105u) - 1u),
			static_cast<::std::uint_least32_t>((high >> 41u) & 0x7ffu));
#else
		result = true;
#endif
		break;
	}
	if (!result)
	{
		::std::abort();
	}
	return 0;
}
