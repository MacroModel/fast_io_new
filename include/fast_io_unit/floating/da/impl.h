#pragma once

#include "compute.h"

namespace fast_io::details::da
{

template <typename flt>
using decimal_mantissa_type =
	::std::conditional_t<(sizeof(flt) <= sizeof(float)), ::std::uint_least32_t, ::std::uint_least64_t>;

template <typename flt>
struct decimal_result
{
	decimal_mantissa_type<flt> m10;
	::std::int_least32_t e10;
};

/// @brief  Indicates whether this floating customization provides staged conversion on the target ISA.
/// @details Staging changes only evaluation order: each value still uses the same DA conversion and presentation
///          rules.  It is enabled for binary32 and binary64 on AArch64 and x86-64 because those are the ISA families
///          with complete conversion-plus-emission measurements and specialized ASCII emitters.  Apple M-series
///          Clang and x86-64 System V GCC/Clang are the performance-audited combinations.  Other front ends on the
///          same ISA families retain this correctness-equivalent capability without a performance claim; other ISAs
///          use scalar orchestration until comparable whole-call evidence exists.  This target policy belongs to
///          the floating customization rather than to the platform-independent staged_printable protocol.
template <typename flt>
inline constexpr bool staged_supported{
#if defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64) || defined(__arm64ec__) || \
	defined(_M_ARM64EC) || \
	((defined(__x86_64__) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC)))
	::std::same_as<::std::remove_cvref_t<flt>, float> ||
	::std::same_as<::std::remove_cvref_t<flt>, double>
#else
	false
#endif
};

/// @brief Indicates whether an in-caller scalar fallback is profitable for a staged decimal value.
/// @details This policy controls code placement after staged eligibility fails; it cannot change conversion or
///          presentation semantics. Whole-call paired measurements used one ineligible normal power of two per
///          group, retained the regular-normal corpus as a non-regression control, and inspected text size, frames,
///          spills, and calls. The decision is both compiler- and floating-type-specific. On Apple M4, Apple Clang 21
///          binary64 and GCC 15 binary32 regress the regular corpus by up to 1.8%; on Linux x86-64, GCC 13--15
///          binary32 either regress or enlarge the complete hot body. GCC 16 binary32 improves in isolation, but
///          removing its earlier cold specialization shifts the binary64 cold path and regresses a mixed
///          binary32/binary64 object by 11--17%, so GCC 16 is excluded as a whole. These combinations retain the cold
///          fallback. The accepted combinations improve the ineligible corpus with neutral or better regular timing
///          and reduce the complete object. Closed major-version, operating-system, ISA, and LP64 gates prevent an
///          unaudited optimizer or ABI from inheriting this code-placement decision. Re-audit complete callers before
///          extending any gate.
template <typename flt>
inline constexpr bool staged_inline_fallback_supported{
// Apple AArch64 is restricted to the three front ends measured on an Apple M4. Apple Clang is distinguished from
// upstream Clang by __apple_build_version__ because both define the ordinary Clang compatibility macros.
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__)) && \
	defined(__clang__) && defined(__apple_build_version__) && __clang_major__ == 21
	::std::same_as<::std::remove_cvref_t<flt>, float>
#elif defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__)) && \
	defined(__clang__) && !defined(__apple_build_version__) && __clang_major__ == 23
	::std::same_as<::std::remove_cvref_t<flt>, float> ||
	::std::same_as<::std::remove_cvref_t<flt>, double>
#elif defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__)) && \
	defined(__GNUC__) && !defined(__clang__) && __GNUC__ == 15
	::std::same_as<::std::remove_cvref_t<flt>, double>
// Linux System V x86-64 LP64 uses the per-front-end and per-type decisions established by isolated same-process
// AB/BA measurements. x32 and non-System-V targets have different argument and stack-placement costs.
#elif defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__clang__) && __clang_major__ == 23 && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	::std::same_as<::std::remove_cvref_t<flt>, float> ||
	::std::same_as<::std::remove_cvref_t<flt>, double>
#elif defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__GNUC__) && !defined(__clang__) && __GNUC__ == 13 && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	::std::same_as<::std::remove_cvref_t<flt>, double>
#elif defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__GNUC__) && !defined(__clang__) && __GNUC__ == 14 && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	::std::same_as<::std::remove_cvref_t<flt>, double>
#elif defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__GNUC__) && !defined(__clang__) && __GNUC__ == 15 && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	::std::same_as<::std::remove_cvref_t<flt>, double>
// Every unmeasured ISA, ABI, operating system, compiler, and compiler major keeps the size-bounded cold fallback.
#else
	false
#endif
};

/// @brief Indicates whether the staged decimal state carries the original sign on this compiler target.
/// @details This is a register-allocation policy, not part of the decimal result.  On Linux System V x86-64 LP64,
///          Clang 23 reduces the six-value binary64 explicit stack allocation from 128 to 64 bytes when the sign is
///          prepared, while GCC 15 improves complete two-, four- and six-value runs despite a larger frame.  GCC 13
///          is deliberately excluded: paired runs make independent sign extraction 3--7% faster and reduce text;
///          the four- and six-value bodies also have less stack traffic.  Closed compiler-version sets prevent an
///          unmeasured future major or ABI from inheriting an empirical schedule.  x32, MinGW, non-Linux x86-64,
///          native MSVC and clang-cl use the portable independent extraction.
///          Re-audit the complete caller, not only sign extraction, before extending either version set.
template <typename flt>
inline constexpr bool staged_prepares_sign{
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	((defined(__clang__) && __clang_major__ == 23) || \
	 (defined(__GNUC__) && !defined(__clang__) && __GNUC__ == 15)) && \
	 !(defined(__arm64ec__) || defined(_M_ARM64EC))
	::std::same_as<::std::remove_cvref_t<flt>, double>
#else
	false
#endif
};

/// @brief Indicates whether the direct scalar ASCII emitter is available for the current implementation.
inline constexpr bool scalar_ascii_shortest_supported{
	true
};

struct signed_conversion_result
{
	::std::uint_least64_t significand;
	::std::int_least32_t exponent;
	::std::uint_least32_t last_digit;
	bool has_last_digit;
	bool negative;
};

template <typename flt>
using staged_conversion_result = ::std::conditional_t<
	::fast_io::details::da::staged_prepares_sign<flt>,
	::fast_io::details::da::signed_conversion_result,
	::fast_io::details::da::conversion_result>;

/// @brief  Returns the minimum compatible argument count that enables staged floating emission.
/// @details This value is a threshold, not a fixed batch width: after the threshold is reached, the print core
///          prepares and emits every staged member of the compatible group.  Two is the smallest count that exposes
///          independent conversion work.  Complete two-value calls beat the ordinary per-value path for binary32
///          and binary64 in decimal, general, fixed and scientific format on the measured Apple M4 Clang 23 and
///          x86-64 GCC 13, GCC 15 and Clang 23 targets; widths two through eight were also checked for the default
///          decimal format.  A one-value group cannot overlap independent conversions and remains scalar.  The
///          threshold changes scheduling only and cannot change any emitted byte.
/// @tparam flt the floating-point type
template <typename flt>
[[nodiscard]] inline consteval ::std::size_t staged_width() noexcept
{
	return 2u;
}

/// @brief  Tests the regular-normal precondition required by the prepared decimal conversion.
/// @details For an IEC 60559 encoding, `1 <= exponent < exponent_mask` selects finite normal values.  Unsigned
///          subtraction expresses both strict bounds without a short-circuit branch: zero wraps and the all-ones
///          special exponent maps exactly to the rejected upper endpoint.  A nonzero explicit mantissa additionally
///          selects the regular-boundary DA case; exact powers of two use the existing scalar irregular-boundary
///          path.  The bitwise conjunction intentionally evaluates both independent predicates and is identical on
///          every target.  A former Apple-only empty-asm barrier was removed after whole-call M4 measurements at the
///          two-value threshold and at six/eight values showed both a regression and larger binary64 bodies, so this
///          semantic predicate contains no ISA policy.
/// @tparam flt           the floating-point type
/// @tparam mantissa_type the unsigned representation used by the floating mantissa
/// @tparam exponent_type the unsigned representation used by the raw exponent
/// @param  mantissa      the explicit binary mantissa bits
/// @param  exponent      the raw binary exponent bits
/// @param  exponent_mask a mask containing every raw exponent bit
/// @return bool true when the direct staged conversion accepts the value
template <typename flt, typename mantissa_type, typename exponent_type>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool staged_eligible(
	mantissa_type mantissa, exponent_type exponent, mantissa_type exponent_mask) noexcept
{
	return (mantissa != 0u) &
		   (static_cast<mantissa_type>(exponent - 1u) < exponent_mask - 1u);
}

template <typename flt>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr decimal_result<flt> finalize(
	conversion_result converted) noexcept
{
	if (converted.has_last_digit)
	{
		return {static_cast<decimal_mantissa_type<flt>>(
					converted.significand * 10u + converted.last_digit),
				converted.exponent};
	}
	return {static_cast<decimal_mantissa_type<flt>>(converted.significand), converted.exponent + 1};
}

template <typename decimal_type>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr decimal_type trim_trailing_zeros(
	decimal_type result) noexcept
{
	if (result.m10 % 10u == 0u) [[unlikely]]
	{
		auto const [m10, zeroes]{::fast_io::bitops::rtz_iec559(result.m10)};
		result.m10 = m10;
		result.e10 += static_cast<::std::int_least32_t>(static_cast<::std::uint_least32_t>(zeroes));
	}
	return result;
}

template <typename flt, typename mantissa_type>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr conversion_result to_conversion_result(
	mantissa_type mantissa, ::std::int_least32_t raw_exponent) noexcept
{
	constexpr bool binary32{sizeof(flt) <= sizeof(float)};
	constexpr ::std::uint_least32_t significand_bits{binary32 ? 23u : 52u};
	constexpr ::std::int_least32_t exponent_offset{binary32 ? 150 : 1075};
	constexpr ::std::uint_least64_t implicit_bit{static_cast<::std::uint_least64_t>(1) << significand_bits};
	auto binary_exponent{raw_exponent - exponent_offset};
	auto effective_raw_exponent{static_cast<::std::uint_least32_t>(raw_exponent)};
	auto binary_significand{static_cast<::std::uint_least64_t>(mantissa)};
	bool regular{true};
	if (raw_exponent == 0)
	{
		++binary_exponent;
		effective_raw_exponent = 1u;
	}
	else
	{
		regular = binary_significand != 0u;
		binary_significand |= implicit_bit;
	}
	conversion_result converted;
	if (!regular)
	{
		converted = ::fast_io::details::da::compute_irregular(binary_significand, binary_exponent);
	}
	else if constexpr (binary32)
	{
		converted = ::fast_io::details::da::compute_binary32(
			static_cast<::std::uint_least32_t>(binary_significand), effective_raw_exponent);
	}
	else
	{
		converted = ::fast_io::details::da::compute_binary64(binary_significand, effective_raw_exponent);
	}
	return converted;
}

template <typename flt, typename mantissa_type>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr decimal_result<flt> to_decimal(
	mantissa_type mantissa, ::std::int_least32_t raw_exponent) noexcept
{
	return ::fast_io::details::da::finalize<flt>(
		::fast_io::details::da::to_conversion_result<flt>(mantissa, raw_exponent));
}

} // namespace fast_io::details::da

#include "ascii.h"
