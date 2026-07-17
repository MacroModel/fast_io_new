#pragma once

#include "cache.h"

namespace fast_io::details::da
{

// A provisional shortest decimal carrier.  If has_last_digit is true, the
// decimal coefficient is significand * 10 + last_digit at exponent `exponent`.
// Otherwise it is significand at exponent `exponent + 1`.  finalize() performs
// exactly this representation change; the direct ASCII writer consumes the
// same fields without first constructing the finalized aggregate.
struct conversion_result
{
	::std::uint_least64_t significand;
	::std::int_least32_t exponent;
	::std::uint_least32_t last_digit;
	bool has_last_digit;
};

// Convert the irregular interval of a finite normal power of two.  The caller
// passes the implicit-bit significand with a zero stored mantissa.  Its lower
// boundary is closer than the regular interval, hence the
// floor(log10((3/4) * 2^e)) exponent and separate half-ULP test.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr conversion_result compute_irregular(
	::std::uint_least64_t binary_significand, ::std::int_least32_t binary_exponent) noexcept
{
	constexpr ::std::uint_least8_t extra_shift{exponent_shift_cache::extra_shift};
	constexpr auto uint64_max{(::std::numeric_limits<::std::uint_least64_t>::max)()};
	auto const decimal_exponent{compute_decimal_exponent(binary_exponent, false)};
	auto const shift{static_cast<::std::uint_least8_t>(
		compute_exponent_shift(binary_exponent, decimal_exponent + 1) + extra_shift)};
	auto const power{cached_data.powers[-decimal_exponent - 1]};
	auto const product{::fast_io::details::da::umul64x128_high(binary_significand << shift, power)};
	auto integral{product.hi >> extra_shift};
	auto const fractional{static_cast<::std::uint_least64_t>(
		(product.hi << (64u - extra_shift)) | (product.lo >> extra_shift))};
	auto const half_ulp{power.hi >> (extra_shift + 1u - shift)};
	auto const round_up{half_ulp > uint64_max - fractional};
	auto const round_down{(half_ulp >> 1u) > fractional};
	integral += round_up;
	auto digit{static_cast<::std::uint_least32_t>(::fast_io::details::da::umul64x64_add_high(
		fractional, 10u, (static_cast<::std::uint_least64_t>(1) << 63u) - 1u))};
	auto const lower{static_cast<::std::uint_least32_t>(::fast_io::details::da::umul64x64_add_high(
		fractional - (half_ulp >> 1u), 10u, uint64_max))};
	if (digit < lower)
	{
		digit = lower;
	}
	return {integral, decimal_exponent, digit, !(round_up || round_down)};
}

// Convert a nonzero regular finite binary32 value.  raw_exponent is the
// effective table exponent: callers map a subnormal raw exponent to one while
// retaining its subnormal significand.  Exact normal powers of two use
// compute_irregular instead.  The significand parity term selects the correct
// open or closed binary interval endpoint.
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void compute_binary32_fields(
	::std::uint_least32_t binary_significand, ::std::uint_least32_t raw_exponent,
	::std::uint_least64_t &integral_result, ::std::int_least32_t &decimal_exponent_result,
	::std::uint_least32_t &last_digit_result, bool &has_last_digit_result) noexcept
{
	constexpr ::std::uint_least8_t extra_shift{34u};
	auto const binary_exponent{static_cast<::std::int_least32_t>(raw_exponent) - 150};
	auto const decimal_exponent{compute_decimal_exponent_reduced(binary_exponent)};
	auto const shift{static_cast<::std::uint_least8_t>(
		cached_data.exponent_shifts.data[static_cast<::std::size_t>(raw_exponent + 925u)] +
		(extra_shift - exponent_shift_cache::extra_shift))};
	auto const power_high{cached_data.powers[-decimal_exponent - 1].hi};
	auto const product{::fast_io::intrinsics::umulh(
		power_high + 1u, static_cast<::std::uint_least64_t>(binary_significand) << shift)};
	constexpr ::std::uint_least64_t fractional_mask{(static_cast<::std::uint_least64_t>(1) << extra_shift) - 1u};
	auto const fractional{product & fractional_mask};
	auto const half_ulp{static_cast<::std::uint_least64_t>(
		(power_high >> (65u - shift)) + (1u - (binary_significand & 1u)))};
	auto const round_up{static_cast<bool>((fractional + half_ulp) >> extra_shift)};
	auto const round_down{half_ulp > fractional};
	auto const integral{static_cast<::std::uint_least64_t>((product >> extra_shift) + round_up)};
	auto digit{static_cast<::std::uint_least32_t>(
		(fractional * 10u + (static_cast<::std::uint_least64_t>(1) << (extra_shift - 1u))) >> extra_shift)};
	if (fractional == (static_cast<::std::uint_least64_t>(1) << (extra_shift - 2u)))
	{
		// Here the fractional value is exactly one quarter, so multiplying by
		// ten gives the halfway value 2.5.  The shortest-interval convention
		// selects its even lower digit, 2; spell out that representable tie.
		digit = 2u;
	}
	integral_result = integral;
	decimal_exponent_result = decimal_exponent;
	last_digit_result = digit;
	has_last_digit_result = !(round_up || round_down);
}

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr conversion_result compute_binary32(
	::std::uint_least32_t binary_significand, ::std::uint_least32_t raw_exponent) noexcept
{
	conversion_result result;
	::fast_io::details::da::compute_binary32_fields(binary_significand, raw_exponent,
		result.significand, result.exponent, result.last_digit, result.has_last_digit);
	return result;
}

// High-product spelling of floor(binary_exponent * 78913 / 2^18).  If e is
// negative, converting it to u64 adds 2^64 before multiplication.  The resulting
// high-word offset is 78913 * 2^46, whose low 32 bits are zero; narrowing the
// high word to int32 therefore removes that offset.  The true result is in the
// int32 range for every binary32/binary64 conversion exponent.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::int_least32_t
compute_decimal_exponent_high_product(::std::int_least32_t binary_exponent) noexcept
{
	return static_cast<::std::int_least32_t>(::fast_io::intrinsics::umulh(
		static_cast<::std::uint_least64_t>(static_cast<::std::int_least64_t>(binary_exponent)),
		static_cast<::std::uint_least64_t>(78913) << 46u));
}

// Exhaust the complete reduced-reciprocal domain so the Apple code-generation
// spelling cannot silently diverge when either reciprocal constant is changed.
[[nodiscard]] inline constexpr bool verify_decimal_exponent_high_product() noexcept
{
	for (::std::int_least32_t exponent{-1074}; exponent <= 971; ++exponent)
	{
		if (compute_decimal_exponent_high_product(exponent) !=
			compute_decimal_exponent_reduced(exponent))
		{
			return false;
		}
	}
	return true;
}

static_assert(verify_decimal_exponent_high_product());

// Staged binary32 conversion with the AArch64 power-table base exposed as a
// loop invariant.  Its fixed-point arithmetic and returned fields are
// identical to compute_binary32; only the address expression differs.  The
// empty asm keeps the base as one opaque register value so staged batches do
// not rematerialize it.  Retain this variant only while the supported assembly
// matrix confirms the shared base and no additional spill or call.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr conversion_result compute_binary32_staged(
	::std::uint_least32_t binary_significand, ::std::uint_least32_t raw_exponent) noexcept
{
	// GNU-driver AArch64 uses the reversed split cache layout whose prepared base
	// is profitable across staged values.  `_MSC_VER` excludes both native MSVC
	// and clang-cl because their source/ABI contracts are not covered by the GNU
	// extended-assembly artifact.  Apple Clang 23 on M4 is the performance-audited
	// case; other GNU-driver AArch64 frontends inherit only the source-capability
	// and semantic-equivalence hypothesis until their complete staged caller is
	// re-audited.  Every other configuration delegates to the field-for-field
	// identical ordinary conversion.
#if (defined(__aarch64__) || defined(__arm64__)) && !defined(_MSC_VER) && \
	(defined(__clang__) || (defined(__GNUC__) && !defined(__clang__)))
	constexpr ::std::uint_least8_t extra_shift{34u};
	auto const binary_exponent{static_cast<::std::int_least32_t>(raw_exponent) - 150};
	auto const decimal_exponent{compute_decimal_exponent_reduced(binary_exponent)};
	auto const shift{static_cast<::std::uint_least8_t>(
		cached_data.exponent_shifts.data[static_cast<::std::size_t>(raw_exponent + 925u)] +
		(extra_shift - exponent_shift_cache::extra_shift))};
	auto base{cached_data.powers.data + power10_cache::size + power10_cache::minimum_exponent};
	if (!::std::is_constant_evaluated())
	{
		__asm__("" : "+r"(base));
	}
	auto const power_high{base[static_cast<::std::ptrdiff_t>(decimal_exponent)]};
	auto const product{::fast_io::intrinsics::umulh(
		power_high + 1u, static_cast<::std::uint_least64_t>(binary_significand) << shift)};
	constexpr ::std::uint_least64_t fractional_mask{(static_cast<::std::uint_least64_t>(1) << extra_shift) - 1u};
	auto const fractional{product & fractional_mask};
	auto const half_ulp{static_cast<::std::uint_least64_t>(
		(power_high >> (65u - shift)) + (1u - (binary_significand & 1u)))};
	auto const round_up{static_cast<bool>((fractional + half_ulp) >> extra_shift)};
	auto const round_down{half_ulp > fractional};
	auto const integral{static_cast<::std::uint_least64_t>((product >> extra_shift) + round_up)};
	auto digit{static_cast<::std::uint_least32_t>(
		(fractional * 10u + (static_cast<::std::uint_least64_t>(1) << (extra_shift - 1u))) >> extra_shift)};
	if (fractional == (static_cast<::std::uint_least64_t>(1) << (extra_shift - 2u)))
	{
		// Same exact one-quarter/tie-to-even case as compute_binary32_fields.
		digit = 2u;
	}
	return {integral, decimal_exponent, digit, !(round_up || round_down)};
#else
	return ::fast_io::details::da::compute_binary32(binary_significand, raw_exponent);
#endif
}

// Binary64 form of the regular finite conversion contract documented for
// compute_binary32_fields.  The caller supplies effective raw exponent one for
// subnormals, while exact normal powers of two use compute_irregular.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr conversion_result compute_binary64(
	::std::uint_least64_t binary_significand, ::std::uint_least32_t raw_exponent) noexcept
{
	auto const binary_exponent{static_cast<::std::int_least32_t>(raw_exponent) - 1075};
	::std::int_least32_t decimal_exponent;
	// On Apple AArch64, the unsigned high product below computes the same reduced
	// reciprocal as floor(binary_exponent * 78913 / 2^18).  For a negative
	// exponent, its unsigned 2^64 offset is multiplied by 78913 * 2^46; that
	// offset has 46 zero low bits and vanishes when the high word is narrowed to
	// int32.  The complete conversion domain fits int32.  This spelling preserves
	// the audited single-umulh Apple Clang 23/M4 assembly shape.  The native-u128
	// conjunct is a compiler-capability boundary for that audited configuration,
	// not part of the reciprocal identity.  All other targets use the constexpr
	// reciprocal whose equality is exhaustively checked above.
#if defined(__APPLE__) && defined(__SIZEOF_INT128__) && \
	(defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64))
	decimal_exponent = compute_decimal_exponent_high_product(binary_exponent);
#else
	decimal_exponent = compute_decimal_exponent_reduced(binary_exponent);
#endif
	constexpr ::std::uint_least8_t extra_shift{exponent_shift_cache::extra_shift};
	auto const shift{cached_data.exponent_shifts.data[raw_exponent]};
	auto const power{cached_data.powers[-decimal_exponent - 1]};
	auto const product{::fast_io::details::da::umul64x128_high(binary_significand << shift, power)};
	auto integral{product.hi >> extra_shift};
	auto const fractional{static_cast<::std::uint_least64_t>(
		(product.hi << (64u - extra_shift)) | (product.lo >> extra_shift))};
	auto digit{static_cast<::std::uint_least32_t>(::fast_io::details::da::umul64x64_add_high(
		fractional, 10u, (static_cast<::std::uint_least64_t>(1) << 63u) + 6u))};
	if (fractional == (static_cast<::std::uint_least64_t>(1) << 62u))
	{
		// fractional / 2^64 is exactly one quarter; 10 * fractional is the
		// halfway decimal digit 2.5, whose even endpoint is digit 2.
		digit = 2u;
	}
	auto const half_ulp{static_cast<::std::uint_least64_t>(
		(power.hi >> (extra_shift + 1u - shift)) + (1u - (binary_significand & 1u)))};
	auto const round_up{fractional + half_ulp < fractional};
	auto const round_down{half_ulp > fractional};
	integral += round_up;
	return {integral, decimal_exponent, digit, !(round_up || round_down)};
}

struct binary64_scientific_precision_result
{
	// On success, significand contains exactly the requested number of decimal
	// digits and exponent is the base-ten exponent of its leading digit.  Failure
	// means only that the cached approximation could not prove rounding; the
	// exact precision path remains authoritative.
	::std::uint_least64_t significand;
	::std::int_least32_t exponent;
	bool success;
};

// Compute the correctly rounded P16-P19 scientific coefficient for a positive
// normal binary64 magnitude.  The caller supplies the implicit bit, so
// binary_significand is in [2^52, 2^53), and raw_exponent is in [1, 2046].
// This normal-only contract is essential: subnormals do not share the relative
// error bound used below and are intentionally handled by the exact path.
//
// The DA cache converts the binary value to a fixed-point decimal interval.  A
// provisional integer contributes either fifteen or sixteen leading digits;
// multiplier extends it to the requested width without materializing the
// hundreds of digits available from the exact binary expansion.
//
// P14-P15 need a quotient/remainder half-boundary proof rather than this
// multiplier interval.  A combined runtime P14/P15 entry was not retained.
// In two-link-order Apple-Clang-23/M4 measurements it added 2,504 bytes of text,
// regressed the aggregate normal controls by 0.74%, and regressed the P17/P20
// subnormal controls by 3.55%/3.02%.  A smaller P15-only entry made P15
// 4.9%--6.0% faster, but added 5,196 bytes and still regressed common/subnormal
// P20 by 2.62%/1.78%.  The extra quotient, scale and branch state therefore
// lengthens callers outside its target precision.  P14-P15 keep the exact
// fallback; this split is measured code-generation policy, not a missing
// mathematical rounding rule.
template <::std::size_t digits>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr binary64_scientific_precision_result
compute_binary64_scientific_precision(::std::uint_least64_t binary_significand,
	::std::uint_least32_t raw_exponent) noexcept
{
	static_assert(16u <= digits && digits <= 19u);
	auto const binary_exponent{static_cast<::std::int_least32_t>(raw_exponent) - 1075};
	::std::int_least32_t decimal_exponent;
	// Use the same proved Apple-AArch64 reciprocal identity and single-umulh
	// Apple Clang 23/M4 code-generation policy as compute_binary64.  The native-u128
	// conjunct delimits the audited compiler capability; precision changes neither
	// the identity's domain nor its integer result.  Every other configuration uses
	// the exhaustively equivalent reduced reciprocal below.
#if defined(__APPLE__) && defined(__SIZEOF_INT128__) && \
	(defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64))
	decimal_exponent = compute_decimal_exponent_high_product(binary_exponent);
#else
	decimal_exponent = compute_decimal_exponent_reduced(binary_exponent);
#endif
	constexpr ::std::uint_least8_t extra_shift{exponent_shift_cache::extra_shift};
	auto const shift{cached_data.exponent_shifts.data[raw_exponent]};
	auto const power{cached_data.powers[-decimal_exponent - 1]};
	auto const product{::fast_io::details::da::umul64x128_high(
		binary_significand << shift, power)};
	auto const integral{product.hi >> extra_shift};
	auto const fractional{static_cast<::std::uint_least64_t>(
		(product.hi << (64u - extra_shift)) | (product.lo >> extra_shift))};
	auto const has_extra_digit{integral >= static_cast<::std::uint_least64_t>(1000000000000000ULL)};
	constexpr ::std::uint_least64_t extra_digit_multiplier{[]
	{
		::std::uint_least64_t value{1u};
		for (::std::size_t index{16u}; index != digits; ++index)
		{
			value *= 10u;
		}
		return value;
	}()};
	auto const multiplier{has_extra_digit ? extra_digit_multiplier :
		extra_digit_multiplier * 10u};
	auto const fractional_product{
		::fast_io::details::da::umul64x64(fractional, multiplier)};
	auto significand{integral * multiplier + fractional_product.hi};

	// Let A = I + F / 2^64 be the scaled value obtained from the cached lower
	// endpoint, where I is integral and F is fractional.  The provisional I has
	// either fifteen or sixteen digits.  M = multiplier extends it to the
	// requested width, and F * M = H * 2^64 + L contributes H to significand
	// while L = fractional_product.lo decides rounding.
	//
	// Each cached power is the lower 128-bit endpoint of its normalized exact
	// power.  With binary_significand < 2^53 and shift <= 6 (verified for the
	// complete normal domain in cache.h), the discarded cache and product tails
	// place the approximation less than 2049/2048 fixed-point units below the
	// exact value.  After scaling by M = multiplier, the greatest integral
	// distance which this one-sided error can cross is
	//
	//   B = ceil(M * 2049 / 2048) - 1
	//     = M + floor((M - 1) / 2048).
	//
	// Unsigned subtraction implements the closed interval test without another
	// branch: L - (half - B) <= B iff L is in [half - B, half].  The interval
	// therefore rejects every exact half and every value whose comparison with
	// half could change after restoring the omitted tail.
	// Every accepted value lies strictly on the same side of half as the exact
	// product.  All six nearest policies consequently agree here, since they
	// differ only at an exact tie; directed policies never dispatch this carrier.
	auto const ambiguity_bound{multiplier + ((multiplier - 1u) >> 11u)};
	constexpr ::std::uint_least64_t half{static_cast<::std::uint_least64_t>(1ULL) << 63u};
	if (fractional_product.lo - (half - ambiguity_bound) <= ambiguity_bound)
	{
		return {};
	}
	if (half < fractional_product.lo)
	{
		++significand;
	}
	auto real_exponent{decimal_exponent +
		static_cast<::std::int_least32_t>(has_extra_digit ? 16u : 15u)};
	constexpr ::std::uint_least64_t normalization_threshold{[]
	{
		::std::uint_least64_t value{1u};
		for (::std::size_t index{}; index != digits; ++index)
		{
			value *= 10u;
		}
		return value;
	}()};
	if (significand == normalization_threshold)
	{
		// Rounding 9.99... can produce 10^digits.  Dividing that coefficient
		// by ten and advancing the scientific exponent represents the same
		// decimal value while restoring the promised fixed coefficient width.
		significand = normalization_threshold / 10u;
		++real_exponent;
	}
	return {significand, real_exponent, true};
}

// This block owns every precision carrier whose production representation uses
// a native unsigned 128-bit type.  Complete scientific P20-P33 and the accepted
// branch of partial P34 necessarily exceed u64.
// The fixed-fractional implementation below also keeps K16-K19 in the same u128
// result so K16-K28 has one compute/fallback contract and no second partial
// backend.  This is a representation choice, not an ISA rounding choice:
// targets without native u128 retain exact block materialization for the entire
// block, and no coefficient is ever narrowed to enable it.
#if defined(__SIZEOF_INT128__)
struct binary64_scientific_wide_precision_result
{
	// 10^34 < 2^113, so both complete P20-P33 and accepted partial-P34
	// rounded coefficients fit in u128.
	__uint128_t significand;
	::std::int_least32_t exponent;
	bool success;
};

// P20-P33 use the same cached lower endpoint and one-sided ambiguity proof as
// the u64 precision window above.  Preconditions are also identical:
// binary_significand is normalized, raw_exponent is in [1, 2046], and only a
// nearest policy may consume a success.  For a digit count P, callers provide
// extra_digit_multiplier = 10^(P-16), normalization_threshold = 10^P, and
// normalized_significand = 10^(P-1) from one consistent P.
//
// Only the rounded coefficient is wider.  M <= 10^18 and B remain in u64,
// while 10^33 < 2^110 fits in u128.  Keep M, B, and the half-boundary comparison
// in u64: the intended emitted shape is one 64x64-to-128 product followed by
// one u64 interval test.  Revalidate that shape, spills, and calls on AArch64
// and x86-64 when changing this helper.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr
binary64_scientific_wide_precision_result compute_binary64_scientific_wide_precision(
	::std::uint_least64_t binary_significand,
	::std::uint_least32_t raw_exponent,
	::std::uint_least64_t extra_digit_multiplier,
	__uint128_t normalization_threshold,
	__uint128_t normalized_significand) noexcept
{
	auto const binary_exponent{static_cast<::std::int_least32_t>(raw_exponent) - 1075};
	::std::int_least32_t decimal_exponent;
	// Same Apple-AArch64 reciprocal identity as the narrow precision carrier.
	// The outer __int128 availability guard already proves this helper can hold
	// the wide coefficient.  Apple Clang 23/M4 is the measured single-umulh case;
	// other targets use the exhaustively equivalent reduced reciprocal, so this
	// inner split controls exponent code generation only.
#if defined(__APPLE__) && \
	(defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64))
	decimal_exponent = compute_decimal_exponent_high_product(binary_exponent);
#else
	decimal_exponent = compute_decimal_exponent_reduced(binary_exponent);
#endif
	constexpr ::std::uint_least8_t extra_shift{exponent_shift_cache::extra_shift};
	auto const shift{cached_data.exponent_shifts.data[raw_exponent]};
	auto const power{cached_data.powers[-decimal_exponent - 1]};
	auto const product{::fast_io::details::da::umul64x128_high(
		binary_significand << shift, power)};
	auto const integral{product.hi >> extra_shift};
	auto const fractional{static_cast<::std::uint_least64_t>(
		(product.hi << (64u - extra_shift)) | (product.lo >> extra_shift))};
	auto const has_extra_digit{integral >= static_cast<::std::uint_least64_t>(1000000000000000ULL)};
	auto const multiplier{has_extra_digit ? extra_digit_multiplier :
		extra_digit_multiplier * 10u};
	auto const fractional_product{
		::fast_io::details::da::umul64x64(fractional, multiplier)};
	auto significand{static_cast<__uint128_t>(integral) * multiplier +
		fractional_product.hi};
	// Same one-sided bound as the narrow carrier:
	// B = M + floor((M - 1) / 2048).  Reject [2^63 - B, 2^63] so every exact
	// or possible tie remains on the exact-materialization fallback.
	auto const ambiguity_bound{multiplier + ((multiplier - 1u) >> 11u)};
	constexpr ::std::uint_least64_t half{static_cast<::std::uint_least64_t>(1ULL) << 63u};
	if (fractional_product.lo - (half - ambiguity_bound) <= ambiguity_bound)
	{
		return {};
	}
	if (half < fractional_product.lo)
	{
		++significand;
	}
	auto real_exponent{decimal_exponent +
		static_cast<::std::int_least32_t>(has_extra_digit ? 16u : 15u)};
	if (significand == normalization_threshold)
	{
		// The caller supplies both constants so runtime precision dispatch can
		// share this body without a u128 division on the normalization edge.
		significand = normalized_significand;
		++real_exponent;
	}
	return {significand, real_exponent, true};
}

template <::std::size_t digits>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr
binary64_scientific_wide_precision_result compute_binary64_scientific_wide_precision(
	::std::uint_least64_t binary_significand,
	::std::uint_least32_t raw_exponent) noexcept
{
	// The complete-domain proof stops at P33.  M <= 10^18 fits in u64 and
	// 10^33 fits in u128.  P34's fifteen-digit provisional branch needs M=10^19,
	// whose ambiguity radius exceeds one half; accepting that branch requires an
	// additional fractional word and a new error analysis.  The independent
	// partial helper below safely accepts only the sixteen-digit branch.
	static_assert(20u <= digits && digits <= 33u);
	// Generate specialization constants at compile time without a P20-P33 table.
	// Runtime renderers obtain decimal scales from uint64_power10_table in
	// cache.h.
	constexpr ::std::uint_least64_t extra_digit_multiplier{[]
	{
		::std::uint_least64_t value{1u};
		for (::std::size_t index{16u}; index != digits; ++index)
		{
			value *= 10u;
		}
		return value;
	}()};
	constexpr __uint128_t normalization_threshold{[]
	{
		__uint128_t value{1u};
		for (::std::size_t index{}; index != digits; ++index)
		{
			value *= 10u;
		}
		return value;
	}()};
	return compute_binary64_scientific_wide_precision(binary_significand,
		raw_exponent, extra_digit_multiplier, normalization_threshold,
		normalization_threshold / 10u);
}

// P34 admits a useful but deliberately partial extension of the P20-P33 proof.
// The cached-power integer before decimal extension has either fifteen or
// sixteen digits.  In the sixteen-digit case, a 34-digit coefficient needs
// M = 10^(34-16) = 10^18 and the existing one-sided cache error gives
//
//   B = M + floor((M - 1) / 2048) < 2^63.
//
// Rejecting [2^63-B, 2^63] therefore proves the same strict comparison with one
// half as the established wide carrier.  Moreover 10^34 < 2^113, so both the
// rounded coefficient and its all-nine normalization edge fit in u128.  The
// fifteen-digit case would require M = 10^19, for which B exceeds 2^63 and the
// one-word closed-interval test is no longer valid; it must fall back before M
// is formed.  Keeping this as a separate helper preserves the P20-P33 runtime
// body's assembly and proof domain while exact materialization remains
// authoritative for every rejected P34 value.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr
binary64_scientific_wide_precision_result
compute_binary64_scientific_p34_partial_precision(
	::std::uint_least64_t binary_significand,
	::std::uint_least32_t raw_exponent) noexcept
{
	auto const binary_exponent{static_cast<::std::int_least32_t>(raw_exponent) - 1075};
	::std::int_least32_t decimal_exponent;
	// This is the same exhaustively equivalent exponent computation used by the
	// P20-P33 carrier.  Apple AArch64 retains its measured single-umulh spelling;
	// all other targets use the reduced reciprocal.  The conditional selects code
	// generation only and does not alter the partial P34 acceptance proof.
#if defined(__APPLE__) && \
	(defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64))
	decimal_exponent = compute_decimal_exponent_high_product(binary_exponent);
#else
	decimal_exponent = compute_decimal_exponent_reduced(binary_exponent);
#endif
	constexpr ::std::uint_least8_t extra_shift{exponent_shift_cache::extra_shift};
	auto const shift{cached_data.exponent_shifts.data[raw_exponent]};
	auto const power{cached_data.powers[-decimal_exponent - 1]};
	auto const product{::fast_io::details::da::umul64x128_high(
		binary_significand << shift, power)};
	auto const integral{product.hi >> extra_shift};
	constexpr ::std::uint_least64_t sixteen_digit_threshold{
		static_cast<::std::uint_least64_t>(1000000000000000ULL)};
	if (integral < sixteen_digit_threshold)
	{
		// A fifteen-digit provisional integer needs M=10^19.  Its ambiguity
		// radius crosses one half, so this one-word proof cannot decide it.
		return {};
	}
	auto const fractional{static_cast<::std::uint_least64_t>(
		(product.hi << (64u - extra_shift)) | (product.lo >> extra_shift))};
	constexpr ::std::uint_least64_t multiplier{static_cast<::std::uint_least64_t>(1000000000000000000ULL)};
	constexpr ::std::uint_least64_t ambiguity_bound{
		multiplier + ((multiplier - 1u) >> 11u)};
	constexpr ::std::uint_least64_t half{static_cast<::std::uint_least64_t>(1ULL) << 63u};
	static_assert(ambiguity_bound < half);
	auto const fractional_product{
		::fast_io::details::da::umul64x64(fractional, multiplier)};
	if (fractional_product.lo - (half - ambiguity_bound) <= ambiguity_bound)
	{
		return {};
	}
	auto significand{static_cast<__uint128_t>(integral) * multiplier +
		fractional_product.hi};
	if (half < fractional_product.lo)
	{
		++significand;
	}
	auto real_exponent{decimal_exponent + 16};
	constexpr __uint128_t decimal_limb{static_cast<::std::uint_least64_t>(10000000000000000000ULL)};
	constexpr __uint128_t normalization_threshold{
		decimal_limb * static_cast<::std::uint_least64_t>(1000000000000000ULL)};
	constexpr __uint128_t normalized_significand{
		decimal_limb * static_cast<::std::uint_least64_t>(100000000000000ULL)};
	static_assert(normalization_threshold < (static_cast<__uint128_t>(1u) << 113u));
	if (significand == normalization_threshold)
	{
		significand = normalized_significand;
		++real_exponent;
	}
	return {significand, real_exponent, true};
}

struct binary64_fixed_fractional_precision_result
{
	// The coefficient has `significant` decimal digits.  real_exponent is the
	// exponent of its leading digit after the possible all-nine carry.  A false
	// success flag means that the exact fixed-precision implementation must decide
	// the result; it does not describe a malformed input.
	__uint128_t significand;
	::std::int_least32_t real_exponent;
	::std::size_t significant;
	bool success;
};

// Convert the profitable runtime fixed-fractional window without constructing
// the complete exact binary expansion.  Write the positive normal input as
//
//   x = d * 10^r,  1 <= d < 10.
//
// Rounding x to P digits after the decimal point is rounding x * 10^P to an
// integer.  Before an all-nine carry, that integer has
//
//   K = r + 1 + P
//
// digits, so it is exactly the K-significant-digit scientific DA carrier.  The
// implementation nevertheless remains a separate runtime body: the scientific
// P16-P33 templates deliberately retain compile-time scales and writer widths
// where assembly measurements show shorter dependency chains.  Numeric cache
// data and the generated 10^0..10^19 table are shared.
//
// The accepted triangle is a performance policy established by paired M4,
// x86-64 GCC and x86-64 Clang measurements:
//
//   4 <= P <= 13 and 16 <= K <= P + 15, or
//   P == 14       and 16 <= K <= 28.
//
// It implies 2 <= r + 1 <= 15 before carry.  Thus the matching writer always
// has a nonempty integer part and a nonempty requested fractional part.  P15+
// remains on the existing exact scaled path.  Values outside this measured
// region, subnormals and every ambiguous half-boundary return failure.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr
binary64_fixed_fractional_precision_result compute_binary64_fixed_fractional_precision(
	::std::uint_least64_t binary_significand,
	::std::uint_least32_t raw_exponent,
	::std::size_t fractional_precision) noexcept
{
	// This helper is normal-only.  Keep the check local as well as at the call
	// site because cached_data.exponent_shifts has no subnormal error contract.
	// Rejecting P outside the measured interval before its signed conversion also
	// makes the subsequent K arithmetic defined for every size_t input.
	if (fractional_precision < 4u || 14u < fractional_precision ||
		!raw_exponent || 2046u < raw_exponent)
	{
		return {};
	}
	auto const binary_exponent{static_cast<::std::int_least32_t>(raw_exponent) - 1075};
	::std::int_least32_t decimal_exponent;
	// Apple AArch64 uses the proved unsigned-high-product spelling already used
	// by the scientific DA carrier.  For negative e, conversion to u64 adds 2^64;
	// multiplying by 78913*2^46 adds a high-word offset whose low 32 bits are
	// zero, so narrowing recovers floor(e*78913/2^18).  The exhaustive verifier
	// above covers the entire binary64 domain.  This target split is retained
	// because M-series assembly measurements require one umulh without a signed
	// correction chain in the measured Apple Clang 23/M4 caller.  Later Apple
	// front ends inherit that layout as a conservative hypothesis; other targets
	// generate their semantically equivalent form from the reduced reciprocal.
#if defined(__APPLE__) && \
	(defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64))
	decimal_exponent = compute_decimal_exponent_high_product(binary_exponent);
#else
	decimal_exponent = compute_decimal_exponent_reduced(binary_exponent);
#endif
	constexpr ::std::uint_least8_t extra_shift{exponent_shift_cache::extra_shift};
	auto const shift{cached_data.exponent_shifts.data[raw_exponent]};
	auto const power{cached_data.powers[-decimal_exponent - 1]};
	auto const product{::fast_io::details::da::umul64x128_high(
		binary_significand << shift, power)};
	auto const integral{product.hi >> extra_shift};
	auto const fractional{static_cast<::std::uint_least64_t>(
		(product.hi << (64u - extra_shift)) | (product.lo >> extra_shift))};
	auto const has_extra_digit{integral >= static_cast<::std::uint_least64_t>(1000000000000000ULL)};
	auto real_exponent{decimal_exponent +
		static_cast<::std::int_least32_t>(has_extra_digit ? 16u : 15u)};
	auto const significant_signed{static_cast<::std::int_least64_t>(real_exponent) + 1 +
		static_cast<::std::int_least64_t>(fractional_precision)};
	if (significant_signed < 16 || 33 < significant_signed)
	{
		return {};
	}
	auto const significant{static_cast<::std::size_t>(significant_signed)};
	auto const portable_triangle{
		(4u <= fractional_precision && fractional_precision <= 13u &&
		 significant <= fractional_precision + 15u) ||
		(fractional_precision == 14u && significant <= 28u)};
	if (!portable_triangle)
	{
		return {};
	}

	// K <= 33 makes 10^(K-16) fit u64.  The generated table is the same storage
	// used by scientific precision and the older exact-window code; this path
	// introduces no fixed-specific numeric table.
	auto const extra_digit_multiplier{uint64_power10_table[significant - 16u]};
	auto const multiplier{has_extra_digit ? extra_digit_multiplier :
		extra_digit_multiplier * 10u};
	auto const fractional_product{
		::fast_io::details::da::umul64x64(fractional, multiplier)};
	auto significand{static_cast<__uint128_t>(integral) * multiplier +
		fractional_product.hi};

	// Let A = I + F/2^64 be the cached lower approximation and M be multiplier.
	// The cache/product proof bounds the omitted one-sided tail by less than
	// 2049/2048 units.  Scaling by M can therefore cross at most
	//
	//   B = ceil(M * 2049 / 2048) - 1
	//     = M + floor((M - 1) / 2048)
	//
	// integer positions.  Rejecting the closed approximate interval
	// [2^63-B, 2^63] includes every exact tie and every comparison whose side can
	// change when the omitted tail is restored.  Hence each accepted comparison
	// is on the exact side of half, and all six nearest policies agree.  Directed
	// rounding never dispatches this helper.
	auto const ambiguity_bound{multiplier + ((multiplier - 1u) >> 11u)};
	constexpr ::std::uint_least64_t half{static_cast<::std::uint_least64_t>(1ULL) << 63u};
	if (fractional_product.lo - (half - ambiguity_bound) <= ambiguity_bound)
	{
		return {};
	}
	if (half < fractional_product.lo)
	{
		++significand;
	}

	// Build 10^K and 10^(K-1) from the shared u64 table.  Splitting at 10^19
	// avoids runtime u128 division while covering the complete K16..K33 domain.
	constexpr __uint128_t decimal_limb{static_cast<::std::uint_least64_t>(10000000000000000000ULL)};
	auto const normalization_threshold{significant <= 19u
		? static_cast<__uint128_t>(uint64_power10_table[significant])
		: decimal_limb * uint64_power10_table[significant - 19u]};
	auto const normalized_significand{significant <= 19u
		? static_cast<__uint128_t>(uint64_power10_table[significant - 1u])
		: decimal_limb * uint64_power10_table[significant - 20u]};
	if (significand == normalization_threshold)
	{
		// 9.99... rounded to 10^K is represented as 10^(K-1) with r+1.
		// The fixed writer appends the newly implied final zero, so the rounded
		// value and the requested P-place field are unchanged.
		significand = normalized_significand;
		++real_exponent;
	}
	return {significand, real_exponent, significant, true};
}
#endif

} // namespace fast_io::details::da
