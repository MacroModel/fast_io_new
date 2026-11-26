#pragma once

#include "../decimal/interchange.h"

namespace fast_io
{

enum class binary_format : unsigned char
{
	binary16,
	bfloat16,
	binary32,
	binary64,
	binary128
};

using binary128_words = decimal128_words;

namespace details
{
template <binary_format F>
struct binary_interchange_layout
{
	inline static constexpr unsigned mbits{F == binary_format::binary16 ? 10u : F == binary_format::bfloat16 ? 7u
																			: F == binary_format::binary32   ? 23u
																			: F == binary_format::binary64   ? 52u
																											 : 112u};
	inline static constexpr unsigned ebits{F == binary_format::binary16 ? 5u : F == binary_format::binary64 ? 11u
																		   : F == binary_format::binary128  ? 15u
																											: 8u};
	inline static constexpr unsigned bits{1u + mbits + ebits};
	inline static constexpr int bias{static_cast<int>((1u << (ebits - 1u)) - 1u)};
	using storage_type = ::std::conditional_t<bits == 16u, ::std::uint_least16_t, decimal_storage<bits>>;
};
} // namespace details

/// An IEEE binary interchange object with numeric explicit conversion and exact named bit access.
template <binary_format F>
struct basic_binary_interchange
{
	inline static constexpr auto format{F};
	using storage_type = typename details::binary_interchange_layout<F>::storage_type;
	storage_type bits{};
	constexpr basic_binary_interchange() noexcept = default;

	template <details::interchange_operand T>
	explicit constexpr basic_binary_interchange(T const &value) noexcept
		: basic_binary_interchange(details::interchange_numeric_cast<basic_binary_interchange>(value))
	{}

	template <details::native_interchange_operand T>
	explicit constexpr operator T() const noexcept
	{
		return details::interchange_numeric_cast<T>(*this);
	}

	[[nodiscard]] static constexpr basic_binary_interchange from_bits(storage_type raw) noexcept
	{
		basic_binary_interchange result{};
		result.bits = raw;
		return result;
	}
#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
	[[nodiscard]] static constexpr basic_binary_interchange from_bits(binary128_words raw) noexcept
		requires(F == binary_format::binary128)
	{
		return from_bits((static_cast<__uint128_t>(raw.high) << 64u) | raw.low);
	}
#endif
	[[nodiscard]] constexpr storage_type to_bits() const noexcept
	{
		return bits;
	}
};

using software_float16 = basic_binary_interchange<binary_format::binary16>;
using software_bfloat16 = basic_binary_interchange<binary_format::bfloat16>;
using software_float32 = basic_binary_interchange<binary_format::binary32>;
using software_float64 = basic_binary_interchange<binary_format::binary64>;
using software_float128 = basic_binary_interchange<binary_format::binary128>;

// GCC advertises _Float16 and __bf16 on IA-32 even when the selected ABI
// cannot return either type. The native binary16 and bfloat16 ABI needs SSE2.
#if defined(__GNUC__) && !defined(__clang__) && defined(__i386__) && !defined(__SSE2__)
#define FAST_IO_DETAIL_I386_SOFTWARE_SHORT_FLOAT 1
#endif

#if defined(FAST_IO_DETAIL_I386_SOFTWARE_SHORT_FLOAT)
using float16 = software_float16;
#elif defined(__STDCPP_FLOAT16_T__) || defined(__FLT16_MANT_DIG__)
using float16 = _Float16;
#elif defined(__SIZEOF_FLOAT16__) || defined(__FLOAT16__)
using float16 = __float16;
#else
using float16 = software_float16;
#endif
// Use the same interchange class in an ABI without native BF16 or when a
// build explicitly chooses the library's directly rounded radix conversions.
#if defined(FAST_IO_FORCE_SOFTWARE_BFLOAT16) || defined(FAST_IO_DETAIL_I386_SOFTWARE_SHORT_FLOAT)
using bfloat16 = software_bfloat16;
#elif defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE) || (defined(__GNUC__) && !defined(__clang__) && defined(__BFLT16_MANT_DIG__))
using bfloat16 = __bf16;
#elif defined(__STDCPP_BFLOAT16_T__)
using bfloat16 = decltype(0.0bf16);
#else
using bfloat16 = software_bfloat16;
#endif
#if defined(__STDCPP_FLOAT32_T__)
using float32 = _Float32;
#else
using float32 = ::std::conditional_t<sizeof(float) * ::std::numeric_limits<unsigned char>::digits == 32u && ::std::numeric_limits<float>::digits == 24 && ::std::numeric_limits<float>::max_exponent == 128, float, software_float32>;
#endif
#if defined(FAST_IO_HAS_FLOAT64_TYPE)
using float64 = _Float64;
#else
using float64 = ::std::conditional_t<sizeof(double) * ::std::numeric_limits<unsigned char>::digits == 64u && ::std::numeric_limits<double>::digits == 53 && ::std::numeric_limits<double>::max_exponent == 1024, double, software_float64>;
#endif
// Some targets expose a compiler quad type without the integer carrier used
// by the native binary128 text I/O. The software carrier has its own I/O.
#if !defined(__SIZEOF_INT128__) || __SIZEOF_INT128__ != 16
using float128 = software_float128;
#elif defined(__STDCPP_FLOAT128_T__)
using float128 = _Float128;
#elif defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
using float128 = __float128;
#else
using float128 = ::std::conditional_t<::std::numeric_limits<long double>::digits == 113 && ::std::numeric_limits<long double>::max_exponent == 16384, long double, software_float128>;
#endif

namespace details
{
template <binary_format F>
inline constexpr bool is_software_interchange<basic_binary_interchange<F>>{true};
template <typename T>
inline constexpr bool is_software_binary{};
template <binary_format F>
inline constexpr bool is_software_binary<basic_binary_interchange<F>>{true};
template <binary_format F>
inline constexpr binary128_words binary_interchange_words(basic_binary_interchange<F> value) noexcept
{
	return decimal_split_words(value.bits);
}
template <typename T>
	requires is_software_binary<T>
inline constexpr T binary_interchange_from_words(binary128_words words) noexcept
{
	return T::from_bits(decimal_join_words<typename T::storage_type>(words));
}
} // namespace details
} // namespace fast_io
