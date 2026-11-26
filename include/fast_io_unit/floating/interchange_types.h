#pragma once

namespace fast_io
{

/// The two IEEE 754 interchange encodings of a decimal significand.
enum class decimal_encoding : unsigned char
{
	bid,
	dpd
};

/// Integer words are ordered by significance independently of machine byte order.
struct decimal128_words
{
	::std::uint_least64_t low{};
	::std::uint_least64_t high{};
};

namespace details
{

template <unsigned width>
using decimal_storage = ::std::conditional_t<width == 32u, ::std::uint_least32_t,
											 ::std::conditional_t<width == 64u, ::std::uint_least64_t,
#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
																  __uint128_t
#else
																  decimal128_words
#endif
																  >>;

} // namespace details

// GCC exposes its C decimal types in C++ through machine modes. The decimal
// precision macros test the target capability, including cross compilers.
// Native values cross the I/O boundary solely through their object bits.
#if defined(__GNUC__) && !defined(__clang__) && defined(__DEC32_MANT_DIG__) && \
	defined(__DEC64_MANT_DIG__) && defined(__DEC128_MANT_DIG__)
#define FAST_IO_HAS_NATIVE_DECIMAL_FLOAT 1
using decimal32 [[gnu::mode(SD)]] = float;
using decimal64 [[gnu::mode(DD)]] = float;
using decimal128 [[gnu::mode(TD)]] = float;
#endif

// Direct casts accept precisely the arithmetic domains supported by the
// library. Both software families advertise their domain after their first
// declaration. The constructor owns wrapper destinations to avoid competing
// with a conversion operator for the same class-to-class cast.
namespace details
{
template <typename T>
inline constexpr bool is_software_interchange{};

// A native IEEE quad can exist without a native 128-bit integer carrier.
template <typename T>
inline constexpr bool native_binary128_interchange{
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
	::std::same_as<T, __float128> ||
#endif
#if defined(__STDCPP_FLOAT128_T__)
	::std::same_as<T, _Float128> ||
#endif
	(::std::same_as<T, long double> && ::std::numeric_limits<long double>::digits == 113 &&
	 ::std::numeric_limits<long double>::max_exponent == 16384)};

template <typename T>
concept native_interchange_operand = my_integral<T> || native_binary128_interchange<T> || scan_decfloat_supported_floating_point<T>
#if defined(FAST_IO_HAS_NATIVE_DECIMAL_FLOAT)
									 || ::std::same_as<T, decimal32> || ::std::same_as<T, decimal64> || ::std::same_as<T, decimal128>
#endif
	;

template <typename T>
concept interchange_operand = native_interchange_operand<T> || is_software_interchange<T>;

template <typename To, typename From>
[[nodiscard]] inline constexpr To interchange_numeric_cast(From const &source) noexcept;
} // namespace details

/// A software interchange object owns the IEEE bit pattern, including the sign of
/// zero, the quantum exponent, special values and noncanonical encodings.
template <unsigned width, decimal_encoding encoding>
	requires(width == 32u || width == 64u || width == 128u)
struct basic_decimal_interchange
{
	using storage_type = details::decimal_storage<width>;
	storage_type bits{};

	constexpr basic_decimal_interchange() noexcept = default;

	/// Converts the mathematical value. Raw interchange encodings use from_bits.
	template <details::interchange_operand T>
	explicit constexpr basic_decimal_interchange(T const &source) noexcept
		: basic_decimal_interchange(details::interchange_numeric_cast<basic_decimal_interchange>(source))
	{}

	/// Floating destinations use round-to-nearest, ties-to-even. Integer
	/// destinations require a finite, representable truncated value.
	template <details::native_interchange_operand T>
	explicit constexpr operator T() const noexcept
	{
		return details::interchange_numeric_cast<T>(*this);
	}

	/// Reconstruct the representation exactly, including all noncanonical bits.
	[[nodiscard]] static constexpr basic_decimal_interchange from_bits(storage_type raw) noexcept
	{
		basic_decimal_interchange result{};
		result.bits = raw;
		return result;
	}

	[[nodiscard]] constexpr storage_type to_bits() const noexcept
	{
		return bits;
	}
};

using decimal32_bid = basic_decimal_interchange<32u, decimal_encoding::bid>;
using decimal64_bid = basic_decimal_interchange<64u, decimal_encoding::bid>;
using decimal128_bid = basic_decimal_interchange<128u, decimal_encoding::bid>;
using decimal32_dpd = basic_decimal_interchange<32u, decimal_encoding::dpd>;
using decimal64_dpd = basic_decimal_interchange<64u, decimal_encoding::dpd>;
using decimal128_dpd = basic_decimal_interchange<128u, decimal_encoding::dpd>;

#if !defined(FAST_IO_HAS_NATIVE_DECIMAL_FLOAT)
using decimal32 = decimal32_bid;
using decimal64 = decimal64_bid;
using decimal128 = decimal128_bid;
#endif

/// @brief Identifies the two OCP OFP8 interchange encodings.
enum class fp8_format
{
	e4m3fn,
	e5m2
};

/// @brief Owns an OCP floating-point encoding, including signed zero and NaN bits.
template <fp8_format Format>
	requires(Format == fp8_format::e4m3fn || Format == fp8_format::e5m2)
struct basic_float8
{
	inline static constexpr auto format{Format};
	::std::uint_least8_t bits{};

	/// @brief Constructs positive zero.
	constexpr basic_float8() noexcept = default;

	/// Converts the mathematical value. Raw interchange encodings use from_bits.
	template <details::interchange_operand T>
	explicit constexpr basic_float8(T const &source) noexcept
		: basic_float8(details::interchange_numeric_cast<basic_float8>(source))
	{}

	/// Floating destinations use round-to-nearest, ties-to-even. Integer
	/// destinations require a finite, representable truncated value.
	template <details::native_interchange_operand T>
	explicit constexpr operator T() const noexcept
	{
		return details::interchange_numeric_cast<T>(*this);
	}

	/// @brief Imports the low eight bits of an OFP8 interchange encoding without numerical conversion.
	[[nodiscard]] static constexpr basic_float8 from_bits(::std::uint_least8_t encoding) noexcept
	{
		basic_float8 result{};
		result.bits = encoding & 0xffu;
		return result;
	}

	/// @brief Exports the OFP8 interchange encoding, including the sign of zero and NaN payload bits.
	[[nodiscard]] constexpr ::std::uint_least8_t to_bits() const noexcept
	{
		return bits;
	}
};

using float8_e4m3fn = basic_float8<fp8_format::e4m3fn>;
using float8_e5m2 = basic_float8<fp8_format::e5m2>;

namespace details
{
template <unsigned width, decimal_encoding code>
inline constexpr bool is_software_interchange<basic_decimal_interchange<width, code>>{true};
template <fp8_format F>
inline constexpr bool is_software_interchange<basic_float8<F>>{true};
} // namespace details
} // namespace fast_io
