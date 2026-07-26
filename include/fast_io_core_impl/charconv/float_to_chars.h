#pragma once

/*
Floating to_chars: proof and bounded-store contract
===================================================

Exact source model
------------------

For every admitted IEC 60559 representation, punning.h decomposes a finite
value into sign s, integer significand m, and binary exponent e:

                         x = (-1)^s m 2^e.

This identity is exact, including subnormals (whose hidden bit is absent).
NaN, infinity, and signed zero are classified from the same integer fields
before any floating arithmetic, which also preserves narrow signaling-NaN
payloads on ABIs where a native by-value conversion would quiet them.

Shortest conversion constructs the rounding interval I(x): the set of reals
which the selected policy maps back to x.  Its endpoints are the adjacent
binary midpoints for a nearest policy and adjacent representable values for a
directed policy; endpoint openness is precisely the policy's tie rule.  For a
decimal candidate c = M*10^q, cached-power multiplication compares the integer
image of c with those endpoints.  The cache error is carried as an interval,
so a candidate is accepted only when its entire computed interval has the same
comparison result.  The exact fallback uses the dyadic identity above.

Existence follows because sufficiently fine decimal grids intersect the
nonempty interior of I(x).  The implementation tests digit counts in increasing
order; the first admitted (M,q) is therefore shortest.  When two candidates of
the same length exist, the documented distance/tie comparison selects the one
required by the rounding policy.  Hence parsing the emitted carrier under that
policy returns x, and no shorter carrier can do so.

Precision conversion asks for a decimal grid rather than a shortest interval:
10^-P for fixed/scientific hexadecimal-fraction precision, or P significant
digits for general.  Exact binary expansion gives quotient Q and remainder R.
Comparing 2R with the divisor implements the six nearest rules; testing R!=0
implements the four directed rules.  Carry is propagated before presentation,
so powers of ten and binade boundaries need no exceptional rounding rule.

Presentation is injective in the selected carrier: fixed inserts the radix
according to q, scientific writes one leading digit and adjusts the exponent,
and hexadecimal writes the binary exponent with no `0x` prefix.  General
without precision compares the complete fixed and scientific spelling lengths
and selects the shorter (fixed on a tie).  With P significant digits, let X be
the scientific exponent *after rounding*; the standard `g` rule selects fixed
iff -4 <= X < max(P,1).  The charconv_significant precision tag records only
this final layout rule and reuses the same numeric rounding engine.

Digits and punctuation are produced with char_literal_v/charliteralofnumber,
so char, wchar_t, char8_t, char16_t, and char32_t encode the same abstract
spelling.  ASCII SIMD is gated by both one-byte width and is_ascii; EBCDIC and
wide execution characters use the scalar table.

Bounded-store theorem
---------------------

The ordinary floating reserve writer may issue a fixed-width SIMD store beyond
its *logical* returned pointer.  Such a store is safe only when the full
type/format reserve extent is available.  to_chars_floating_emit therefore has
two proved branches:

  1. capacity >= print_reserve_size(tag[,value]): the ordinary writer's
     physical-store contract is satisfied, so it performs the fast single
     conversion.  Runtime-precision manipulators necessarily use the
     value-dependent form because precision is part of `value`;
  2. otherwise, print_reserve_precise_size computes the exact logical length.
     If it does not fit, no write occurs and {last,value_too_large} is returned.
     If it fits, print_reserve_precise_define selects exact-bounds stores whose
     every written code unit is inside that measured slice.

These cases cover every capacity and prove both absence of overrun at an exact
boundary and the standard all-or-error result.  The second conversion in the
tight branch is intentional: the first call is a non-writing size proof.

Constant/runtime equivalence
----------------------------

The compiler-constant proxy captures exactly the same (s,m,e) fields, rounding
policy, and presentation flags as the native runtime manipulator.  The proof
above makes the output a deterministic function F(s,m,e,flags,precision).
Both paths therefore emit F of identical arguments.  __builtin_constant_p is
only a profitability gate: true materializes the integer-field proxy; false
uses the native path.  It is not a semantic test and is distinct from constant
evaluation.  During consteval, the same constexpr field arithmetic is used and
runtime SIMD branches are unavailable.  A runtime `chars_format` switch and a
literal-format arm call the same fixed-format instantiation, so format
dispatch is equivalent by direct substitution as well.
*/

namespace fast_io
{

namespace details
{

template <::std::chars_format format, bool shortest_general,
		  ::fast_io::manipulators::floating_rounding rounding>
inline consteval ::fast_io::manipulators::scalar_flags
to_chars_floating_flags() noexcept
{
	auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.rounding = rounding;
	flags.showpos = false;
	flags.showbase = false;
	flags.comma = false;
	if constexpr (format == ::std::chars_format::fixed)
	{
		/*
		Fixed presentation inserts the radix according to the decimal exponent
		and never emits an exponent field.  The carrier itself is unchanged.
		*/
		flags.floating = ::fast_io::manipulators::floating_format::fixed;
	}
	else if constexpr (format == ::std::chars_format::scientific)
	{
		/*
		Scientific presentation normalizes to one leading decimal digit and
		moves the compensating power into an exponent.  This is an exact
		identity M*10^q=(M/10^(L-1))*10^(q+L-1).
		*/
		flags.floating = ::fast_io::manipulators::floating_format::scientific;
	}
	else if constexpr (format == ::std::chars_format::hex)
	{
		/*
		Hexadecimal presentation consumes the exact m*2^e decomposition and
		charconv suppresses the `0x` prefix through showbase=false above.
		*/
		flags.floating = ::fast_io::manipulators::floating_format::hexfloat;
	}
	else
	{
		static_assert(format == ::std::chars_format::general);
		/*
		The three-argument overload (`shortest_general=true`) minimizes the
		complete fixed/scientific spelling.  Explicit `chars_format::general`
		instead applies the standard `%g`-style exponent window.  Distinct
		presentation tags are necessary even though both reuse one shortest
		numeric carrier.
		*/
		flags.floating = shortest_general
			? ::fast_io::manipulators::floating_format::decimal
			: ::fast_io::manipulators::floating_format::general;
	}
	return flags;
}

template <::std::chars_format format,
		  ::fast_io::manipulators::floating_rounding rounding>
inline consteval ::fast_io::manipulators::scalar_flags
to_chars_floating_precision_flags() noexcept
{
	auto flags{
		::fast_io::details::to_chars_floating_flags<
			format, false, rounding>()};
	if constexpr (format == ::std::chars_format::general)
	{
		/*
		Precision P is a significant-digit grid for general format.  The
		charconv tag changes only the post-rounding layout test
		-4<=X<max(P,1); quotient, remainder, and carry are shared.
		*/
		flags.floating =
			::fast_io::manipulators::floating_format::general;
		flags.precision =
			::fast_io::manipulators::floating_precision::
				charconv_significant;
	}
	else
	{
		if constexpr (format == ::std::chars_format::hex)
		{
			/*
			Hex precision counts fractional hexadecimal digits.  Its dedicated
			tag preserves a leading carry as `2p+E`, which is numerically equal
			to `1p+(E+1)` but is the spelling mandated for explicit precision.
			*/
			flags.precision =
				::fast_io::manipulators::floating_precision::
					charconv_hex_fractional;
		}
		else
		{
			/*
			Fixed and scientific precision count decimal fractional digits and
			must retain trailing zeroes, so both select the exact same 10^-P
			rounding grid.
			*/
			flags.precision =
				::fast_io::manipulators::floating_precision::
					fractional_preserve_trailing_zero;
		}
	}
	return flags;
}

template <::fast_io::details::character char_type, typename printable>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars_floating_emit(char_type *first, char_type *last,
					   printable const &value) noexcept
{
	using printable_type = ::std::remove_cvref_t<printable>;
	constexpr auto tag{::fast_io::io_reserve_type<char_type, printable_type>};
	auto const capacity{static_cast<::std::size_t>(last - first)};
	auto const reserve_size{[&]() constexpr noexcept
	{
		if constexpr (requires { print_reserve_size(tag); })
		{
			/*
			A static reserve extent is a type-level upper bound on every
			physical store performed by the ordinary writer.
			*/
			return print_reserve_size(tag);
		}
		else
		{
			/*
			Runtime precision participates in the store bound, so the
			value-dependent query is required to prove the same containment.
			*/
			return print_reserve_size(tag, value);
		}
	}()};
	if (reserve_size <= capacity) [[likely]]
	{
		/*
		The ordinary writer may overstore past its logical result but never
		past `reserve_size`.  This inequality embeds that whole physical range
		in [first,last), proving the fast call memory-safe.
		*/
		return {print_reserve_define(tag, first, value), {}};
	}

	/*
	The precise-size query is non-writing and returns the exact number L of
	logical code units.  It is reached only when the broad SIMD reserve proof
	does not fit; no output has yet been modified.
	*/
	auto const precise_size{print_reserve_precise_size(tag, value)};
	if (capacity < precise_size) [[unlikely]]
	{
		/*
		L exceeds the destination extent, so no valid complete spelling fits.
		Returning `last` with value_too_large while performing no write is the
		strong bounded-buffer contract.
		*/
		return {last, ::std::errc::value_too_large};
	}
	/*
	Now L<=capacity.  The exact-bounds writer is parameterized by that same L
	and restricts every store to [first,first+L), which proves safety at the
	exact boundary even when the ordinary SIMD writer needed more slack.
	*/
	return {
		print_reserve_precise_define(tag, first, precise_size, value), {}};
}

template <::std::chars_format format,
		  ::fast_io::manipulators::floating_rounding rounding,
		  bool shortest_general = false,
		  ::fast_io::details::my_floating_point T,
		  ::fast_io::details::character char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars_floating_fixed(char_type *first, char_type *last, T value) noexcept
{
	constexpr auto flags{
		::fast_io::details::to_chars_floating_flags<
			format, shortest_general, rounding>()};
	auto source{
		::fast_io::details::make_floating_scalar_manip<flags>(value)};
	using source_type = decltype(source);
	constexpr auto source_tag{
		::fast_io::io_reserve_type<char_type, source_type>};
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
	if constexpr (requires {
					  print_compiler_constant_materialization_eligible(
						  source_tag, source);
					  print_compiler_constant_materialize_gate_proven(
						  source_tag, source);
				  })
	{
		if (print_compiler_constant_materialization_eligible(
				source_tag, source))
		{
			/*
			Eligibility proves that extracting the compiler-known raw fields
			can remove runtime classification/conversion work.  The materialized
			proxy contains the identical sign, exponent, mantissa, and flags, so
			the deterministic formatter theorem gives byte-identical output.
			*/
			auto constant_source{
				print_compiler_constant_materialize_gate_proven(
					source_tag, source)};
			return ::fast_io::details::to_chars_floating_emit(
				first, last, constant_source);
		}
	}
#endif
	/*
	If the optimizer cannot prove eligibility, retaining the native source
	avoids proxy construction.  This branch changes representation of the
	input object only in the rejected alternative, never its formatting rules.
	*/
	return ::fast_io::details::to_chars_floating_emit(first, last, source);
}

template <::std::chars_format format,
		  ::fast_io::manipulators::floating_rounding rounding,
		  ::fast_io::details::my_floating_point T,
		  ::fast_io::details::character char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars_floating_precision_fixed(
	char_type *first, char_type *last, T value,
	::std::size_t precision) noexcept
{
	constexpr auto flags{
		::fast_io::details::to_chars_floating_precision_flags<
			format, rounding>()};
	auto source{
		::fast_io::details::make_floating_scalar_manip_precision<flags>(
			value, precision)};
	using source_type = decltype(source);
	constexpr auto source_tag{
		::fast_io::io_reserve_type<char_type, source_type>};
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
	if constexpr (requires {
					  print_compiler_constant_materialization_eligible(
						  source_tag, source);
					  print_compiler_constant_materialize_gate_proven(
						  source_tag, source);
				  })
	{
		if (print_compiler_constant_materialization_eligible(
				source_tag, source))
		{
			/*
			The precision value is stored in both source forms unchanged.
			Consequently the proxy and native paths round on the same grid as
			well as sharing the same raw floating fields.
			*/
			auto constant_source{
				print_compiler_constant_materialize_gate_proven(
					source_tag, source)};
			return ::fast_io::details::to_chars_floating_emit(
				first, last, constant_source);
		}
	}
#endif
	/*
	The dynamic precision path is the semantic baseline.  Falling through
	preserves one copy of the arithmetic implementation and avoids a runtime
	format proxy when no compile-time saving is proved.
	*/
	return ::fast_io::details::to_chars_floating_emit(first, last, source);
}

template <::fast_io::details::my_floating_point T>
[[nodiscard]] inline constexpr auto
to_chars_floating_capture_fields(T value) noexcept
{
	using floating_type = ::std::remove_cv_t<T>;
	if constexpr (
		::fast_io::details::
			floating_scalar_requires_integer_proxy<floating_type>)
	{
		/*
		Some narrow types (notably bfloat16 on affected ABIs) may be promoted
		or have signaling NaNs quieted by an ordinary by-value floating
		operation.  Capturing their object representation first preserves all
		source bits, after which the proxy exposes the same logical fields.
		*/
		auto const representation{
			::fast_io::details::capture_bfloat16_representation(value)};
		return ::fast_io::details::floating_scalar_proxy_fields<
			floating_type>(representation);
	}
	else
	{
		/*
		For directly supported IEC layouts, get_punned_result is a bitwise
		decomposition.  No floating arithmetic or ambient rounding mode enters
		the classification.
		*/
		return ::fast_io::details::get_punned_result(value);
	}
}

template <::fast_io::details::my_floating_point T>
[[nodiscard]] inline constexpr bool
to_chars_floating_is_integer(T value) noexcept
{
	using floating_type = ::std::remove_cv_t<T>;
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	using mantissa_type = typename trait::mantissa_type;
	auto const fields{
		::fast_io::details::to_chars_floating_capture_fields(value)};
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<mantissa_type>(1u) << trait::ebits) - 1u)};
	if (fields.exponent == exponent_mask)
	{
		/*
		An all-ones exponent denotes infinity or NaN.  Neither is an integer in
		the finite divisibility sense used to select an exact fixed candidate.
		*/
		return false;
	}
	if (!fields.exponent)
	{
		/*
		With exponent zero, a zero mantissa is signed zero and hence integral.
		Any nonzero subnormal has magnitude strictly between zero and the least
		normal; for every admitted format it cannot be a nonzero integer.
		*/
		return fields.mantissa == 0u;
	}
	constexpr auto bias{static_cast<::std::int_least32_t>(
		(static_cast<::std::uint_least32_t>(1u)
		 << (trait::ebits - 1u)) -
		1u)};
	auto const binary_exponent{
		static_cast<::std::int_least32_t>(fields.exponent) - bias -
		static_cast<::std::int_least32_t>(trait::mbits)};
	if (0 <= binary_exponent)
	{
		/*
		The exact value is integer_significand*2^binary_exponent.  A
		nonnegative exponent multiplies an integer by an integer power of two,
		so divisibility by one is immediate.
		*/
		return true;
	}
	auto const discarded_bits{
		static_cast<::std::uint_least32_t>(-binary_exponent)};
	if (trait::mbits < discarded_bits)
	{
		/*
		The hidden significand has its top bit at `mbits`.  Divisibility by
		2^discarded_bits with discarded_bits>mbits would require the entire
		nonzero significand to vanish, which is impossible.
		*/
		return false;
	}
	auto const significand{static_cast<mantissa_type>(
		fields.mantissa |
		(static_cast<mantissa_type>(1u) << trait::mbits))};
	auto const mask{static_cast<mantissa_type>(
		(static_cast<mantissa_type>(1u) << discarded_bits) - 1u)};
	/*
	For a negative binary exponent, m*2^-k is integral exactly when 2^k divides
	m.  The low-k-bit mask is therefore a necessary and sufficient integer
	test, with no conversion or rounding.
	*/
	return (significand & mask) == 0u;
}

template <::fast_io::details::my_floating_point T>
[[nodiscard]] inline constexpr bool
to_chars_floating_fixed_cannot_win_shortest(T value) noexcept
{
	using floating_type = ::std::remove_cv_t<T>;
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	auto const fields{
		::fast_io::details::to_chars_floating_capture_fields(value)};
	constexpr auto bias{static_cast<::std::int_least32_t>(
		(static_cast<::std::uint_least32_t>(1u)
		 << (trait::ebits - 1u)) -
		1u)};
	auto const unbiased_exponent{
		static_cast<::std::int_least32_t>(fields.exponent) - bias};
	constexpr auto maximum_shortest_size{
		::fast_io::details::print_rsv_cache<
			floating_type,
			::fast_io::manipulators::floating_format::decimal>};
	constexpr auto fixed_loss_exponent{
		static_cast<::std::int_least32_t>(
			(10u * maximum_shortest_size + 2u) / 3u)};
	/*
	Let C be `maximum_shortest_size`, a conservative complete-spelling bound
	that already includes a sign, significand separator, exponent marker/sign,
	and the maximum exponent digit count.  A positive normal value with unbiased
	binary exponent E satisfies x>=2^E.  Since

	    2^10 = 1024 > 1000 = 10^3,

	raising both sides to C/3 gives 2^(10C/3)>10^C.  Therefore
	E>=ceil(10C/3) implies x>10^C, so its exact fixed integer needs at least
	C+1 magnitude digits before any sign.  It is strictly longer than every
	possible shortest spelling and cannot win either the primary length order
	or its equal-length distance tie.  The test uses only exponent fields and
	is deliberately one-sided: returning false merely requests the exact
	comparison, so values near the decimal threshold retain full correctness.
	*/
	return fixed_loss_exponent <= unbiased_exponent;
}

#if defined(__SIZEOF_INT128__)
using to_chars_floating_small_integer_type = __uint128_t;
#else
using to_chars_floating_small_integer_type = ::std::uint_least64_t;
#endif

struct to_chars_floating_small_integer
{
	::fast_io::details::to_chars_floating_small_integer_type magnitude{};
	::std::size_t size{};
	bool negative{};
	bool available{};
};

template <::fast_io::details::my_floating_point T>
[[nodiscard]] inline constexpr
	::fast_io::details::to_chars_floating_small_integer
to_chars_floating_make_small_integer(T value) noexcept
{
	using floating_type = ::std::remove_cv_t<T>;
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	using mantissa_type = typename trait::mantissa_type;
	using carrier =
		::fast_io::details::to_chars_floating_small_integer_type;
	auto const fields{
		::fast_io::details::to_chars_floating_capture_fields(value)};
	if (!fields.exponent)
	{
		/*
		The caller has already proved integrality, so exponent zero can only be
		signed zero.  Its exact fixed magnitude is the one decimal digit `0`;
		the sign remains a separate code unit and no shift is required.
		*/
		return {0u, 1u + static_cast<::std::size_t>(fields.sign),
				static_cast<bool>(fields.sign), true};
	}
	constexpr auto bias{static_cast<::std::int_least32_t>(
		(static_cast<::std::uint_least32_t>(1u)
		 << (trait::ebits - 1u)) -
		1u)};
	auto const binary_exponent{
		static_cast<::std::int_least32_t>(fields.exponent) - bias -
		static_cast<::std::int_least32_t>(trait::mbits)};
	auto const source_significand{static_cast<carrier>(
		static_cast<mantissa_type>(
			fields.mantissa |
			(static_cast<mantissa_type>(1u) << trait::mbits)))};
	carrier magnitude{};
	if (binary_exponent < 0)
	{
		/*
		Integrality proves 2^(-e) divides the source significand.  Right shift
		therefore performs exact integer division, discarding only zero bits.
		*/
		magnitude = static_cast<carrier>(
			source_significand >>
			static_cast<unsigned>(-binary_exponent));
	}
	else
	{
		constexpr auto carrier_bits{
			::std::numeric_limits<carrier>::digits};
		auto const shift{static_cast<unsigned>(binary_exponent)};
		constexpr auto carrier_max{
			(::std::numeric_limits<carrier>::max)()};
		if (carrier_bits <= shift ||
			static_cast<carrier>(carrier_max >> shift) <
				source_significand)
		{
			/*
			The exact product m*2^e does not fit this target's small-integer
			carrier.  Returning unavailable is conservative: the caller retains
			the arbitrary-precision exact-size/output path, so this optimization
			can never truncate a candidate.
			*/
			return {};
		}
		/*
		The guard proves source_significand<=MAX/2^shift, hence the left shift
		is representable and equals the exact binary integer product.
		*/
		magnitude = static_cast<carrier>(
			source_significand << shift);
	}
	auto const negative{static_cast<bool>(fields.sign)};
	auto const size{
		::fast_io::details::chars_len<10u, false>(magnitude) +
		static_cast<::std::size_t>(negative)};
	/*
	chars_len is the exact decimal digit count of the exact magnitude derived
	above.  Adding the independently captured sign therefore gives the complete
	fixed spelling length without constructing decimal floating metadata.
	*/
	return {magnitude, size, negative, true};
}

template <::fast_io::manipulators::floating_rounding rounding,
		  ::fast_io::details::my_floating_point T,
		  ::fast_io::details::character char_type>
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars_floating_standard_fixed(
	char_type *first, char_type *last, T value) noexcept
{
	/*
	For an integral binary value, every fixed spelling has the same mandatory
	integer-place count.  The standard's equal-length tie rule therefore selects
	the exact integer, not a shortest scientific carrier followed by zeroes.
	Nonintegral values retain the shortest fixed carrier.  The field test above
	is the exact divisibility condition m*2^e in Z and performs no floating
	arithmetic or environment-dependent conversion.
	*/
	if (::fast_io::details::to_chars_floating_is_integer(value))
	{
		return ::fast_io::details::to_chars_floating_precision_fixed<
			::std::chars_format::fixed, rounding>(
				first, last, value, 0u);
	}
	return ::fast_io::details::to_chars_floating_fixed<
		::std::chars_format::fixed, rounding>(first, last, value);
}

template <::fast_io::manipulators::floating_rounding rounding,
		  ::fast_io::details::my_floating_point T,
		  ::fast_io::details::character char_type>
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars_floating_standard_shortest(
	char_type *first, char_type *last, T value) noexcept
{
	if (::fast_io::details::to_chars_floating_is_integer(value))
	{
		if (::fast_io::details::
				to_chars_floating_fixed_cannot_win_shortest(value))
		{
			/*
			The exponent theorem above proves exact fixed is strictly longer,
			so no exact decimal construction or second size pass can affect the
			selected spelling.  Returning the ordinary shortest path here is
			therefore both semantically final and the common large-integer fast
			path.
			*/
			return ::fast_io::details::to_chars_floating_fixed<
				::std::chars_format::general, rounding, true>(
					first, last, value);
		}
		/*
		For an integral x, let D be the number of digits in its exact fixed
		spelling and S the length selected from the shortest decimal carrier.
		Every D-character fixed candidate has the same mandatory place count.
		If D<=S, the standard's primary length ordering selects fixed, and its
		secondary minimum-distance ordering selects exact x (distance zero).
		If S<D, no distance comparison may displace the strictly shorter
		scientific carrier.  Exact reserve sizing obtains D and S without writing,
		so a rejected output range remains untouched.

		This branch is restricted to exact binary integers.  For a nonintegral
		value, precision-zero fixed would round to a different decimal grid and
		cannot participate in the shortest round-trip candidate set.
		*/
		constexpr auto shortest_flags{
			::fast_io::details::to_chars_floating_flags<
				::std::chars_format::general, true, rounding>()};
		auto shortest_source{
			::fast_io::details::make_floating_scalar_manip<
				shortest_flags>(value)};
		using shortest_source_type = decltype(shortest_source);
		constexpr auto shortest_tag{
			::fast_io::io_reserve_type<
				char_type, shortest_source_type>};
		auto const shortest_size{
			print_reserve_precise_size(
				shortest_tag, shortest_source)};

		auto const small_integer{
			::fast_io::details::
				to_chars_floating_make_small_integer(value)};
		if (small_integer.available)
		{
			if (small_integer.size <= shortest_size)
			{
				/*
				The standard orders candidates by total character count first
				and distance second.  `<=` includes the equal-length case, where
				this exact integer has distance zero and is uniquely preferred.
				to_chars_integral_checked first proves full capacity, then emits
				exactly `size` code units, retaining the floating overload's
				no-partial-write contract for every character type.
				*/
				return ::fast_io::details::
					to_chars_integral_checked<10u>(
						first, last, small_integer.magnitude,
						small_integer.negative);
			}
		}
		else
		{
			/*
			Wide residual integers that exceed the native carrier retain the
			exact precision-zero path.  This branch is cold for binary32/64 on
			uint128 targets but is required for binary128 and uint64-only ABIs.
			*/
			constexpr auto fixed_flags{
				::fast_io::details::
					to_chars_floating_precision_flags<
						::std::chars_format::fixed, rounding>()};
			auto fixed_source{
				::fast_io::details::
					make_floating_scalar_manip_precision<fixed_flags>(
						value, 0u)};
			using fixed_source_type = decltype(fixed_source);
			constexpr auto fixed_tag{
				::fast_io::io_reserve_type<
					char_type, fixed_source_type>};
			auto const fixed_size{
				print_reserve_precise_size(
					fixed_tag, fixed_source)};
			if (fixed_size <= shortest_size)
			{
				/*
				The arbitrary-precision size comparison proves the same
				length/distance ordering when no native integer carrier exists.
				*/
				return ::fast_io::details::
					to_chars_floating_precision_fixed<
						::std::chars_format::fixed, rounding>(
							first, last, value, 0u);
			}
		}
	}
	/*
	For nonintegers, or when the shortest carrier is strictly shorter than the
	exact fixed integer, the ordinary shortest interval result is the standard
	winner.  No precision-zero rounding is introduced on this path.
	*/
	return ::fast_io::details::to_chars_floating_fixed<
		::std::chars_format::general, rounding, true>(
			first, last, value);
}

template <::fast_io::manipulators::floating_rounding rounding,
		  ::fast_io::details::my_floating_point T,
		  ::fast_io::details::character char_type>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars_floating_subnormal_hex(
	char_type *first, char_type *last, T value) noexcept
{
	(void)rounding; // An exact radix-16 expansion has no rounding decision.
	using floating_type = ::std::remove_cv_t<T>;
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	using mantissa_type = typename trait::mantissa_type;
	auto const fields{
		::fast_io::details::to_chars_floating_capture_fields(value)};
	auto mantissa{fields.mantissa};

	::std::uint_least32_t highest_bit{};
	/*
	This helper is called only for exponent==0 and mantissa!=0.  Repeated
	division by two therefore terminates with probe==1, and the iteration count
	is exactly floor(log2(mantissa)).  That bit becomes the explicit leading
	hexadecimal `1` of the normalized subnormal spelling.
	*/
	for (auto probe{mantissa}; 1u < probe; probe >>= 1u)
	{
		++highest_bit;
	}
	auto fractional_digits{
		static_cast<::std::uint_least32_t>((highest_bit + 3u) / 4u)};
	/*
	After removing the leading bit, ceil(highest_bit/4) nibbles cover every
	remaining lower bit.  The lambda aligns nibble `index` immediately below
	the leading bit; its two shift branches are the algebraic cases for a
	nonnegative or negative alignment distance.
	*/
	auto const nibble_at = [mantissa, highest_bit](
							 ::std::uint_least32_t index) constexpr noexcept
	{
		auto const right_position{
			static_cast<::std::int_least32_t>(highest_bit) -
			static_cast<::std::int_least32_t>(4u * (index + 1u))};
		if (0 <= right_position)
		{
			/*
			A nonnegative position selects bits [r,r+3] by a right shift and
			mask.  Since r<=highest_bit<the carrier width, the shift is defined.
			*/
			return static_cast<::std::uint_least32_t>(
				(mantissa >> static_cast<unsigned>(right_position)) &
				static_cast<mantissa_type>(0xfu));
		}
		/*
		For a negative position, at most three zero bits must be appended on
		the right to complete the final nibble.  Left-shifting by that amount
		and masking is exactly the same four-bit window, with no significant
		bit shifted beyond the carrier because this is the terminal partial
		nibble.
		*/
		return static_cast<::std::uint_least32_t>(
			(mantissa << static_cast<unsigned>(-right_position)) &
			static_cast<mantissa_type>(0xfu));
	};
	while (fractional_digits &&
		   nibble_at(fractional_digits - 1u) == 0u)
	{
		/*
		Removing a terminal zero nibble multiplies the written fractional
		coefficient by 16 while reducing its radix scale by 16, so the value is
		unchanged.  The loop stops at the last nonzero nibble and thus proves
		shortest hexadecimal fractional length.
		*/
		--fractional_digits;
	}

	/*
	The largest supported exact spelling is binary128:
	sign + "1." + 28 hexadecimal digits + "p-" + 5 exponent digits.
	Sixty-four code units therefore cover every admitted type independently of
	the destination character width.
	*/
	char_type buffer[64]{};
	auto iter{buffer};
	if (fields.sign)
	{
		/*
		The magnitude derivation is sign-independent; prefixing `-` is the
		exact multiplication by -1 and preserves negative subnormals.
		*/
		*iter++ = ::fast_io::char_literal_v<u8'-', char_type>;
	}
	*iter++ = ::fast_io::char_literal_v<u8'1', char_type>;
	if (fractional_digits)
	{
		/*
		A radix point is necessary iff at least one nonzero fractional nibble
		remains.  Omitting both for an exact power of two yields the shorter
		equivalent spelling `1pE`.
		*/
		*iter++ = ::fast_io::char_literal_v<u8'.', char_type>;
		for (::std::uint_least32_t index{};
			 index != fractional_digits; ++index)
		{
			*iter++ =
				::fast_io::details::charliteralofnumber<char_type, false>(
					static_cast<char8_t>(nibble_at(index)));
		}
	}
	*iter++ = ::fast_io::char_literal_v<u8'p', char_type>;
	constexpr auto bias{static_cast<::std::int_least32_t>(
		(static_cast<::std::uint_least32_t>(1u)
		 << (trait::ebits - 1u)) -
		1u)};
	auto const exponent{static_cast<::std::int_least32_t>(
		1 - bias - static_cast<::std::int_least32_t>(trait::mbits) +
		static_cast<::std::int_least32_t>(highest_bit))};
	/*
	A subnormal is mantissa*2^(1-bias-mbits).  Factoring its highest set bit
	into the written leading one adds `highest_bit` to the exponent, giving the
	expression above exactly.
	*/
	iter = ::fast_io::details::with_sign_prt_rsv_exponent_hex_impl<
		trait::e2hexdigits>(iter, exponent);
	auto const size{static_cast<::std::size_t>(iter - buffer)};
	if (static_cast<::std::size_t>(last - first) < size)
	{
		/*
		All construction occurred in local storage, so an insufficient
		destination has not been touched.  Returning `last` satisfies the
		to_chars failure cursor contract.
		*/
		return {last, ::std::errc::value_too_large};
	}
	/*
	The preceding inequality proves [first,first+size) is inside the caller's
	range.  This scalar copy writes exactly that interval and no SIMD slack.
	*/
	for (::std::size_t index{}; index != size; ++index)
	{
		first[index] = buffer[index];
	}
	return {first + size, {}};
}

template <::fast_io::manipulators::floating_rounding rounding,
		  ::fast_io::details::my_floating_point T,
		  ::fast_io::details::character char_type>
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars_floating_standard_hex(
	char_type *first, char_type *last, T value) noexcept
{
	auto const fields{
		::fast_io::details::to_chars_floating_capture_fields(value)};
	if (!fields.exponent && fields.mantissa)
	{
		/*
		Only nonzero subnormals need renormalization relative to the stream
		formatter's `0.xxxp(min_normal_exp)` form.  The predicate excludes zero
		and all normal/special encodings exactly by their IEC fields.
		*/
		return ::fast_io::details::to_chars_floating_subnormal_hex<
			rounding>(first, last, value);
	}
	/*
	Normal values, signed zero, infinity, and NaN already use the charconv
	canonical layout in the shared hexadecimal writer, so retaining that hot
	path avoids a duplicate formatter.
	*/
	return ::fast_io::details::to_chars_floating_fixed<
		::std::chars_format::hex, rounding>(first, last, value);
}

} // namespace details

template <
	::fast_io::manipulators::floating_rounding rounding =
		::fast_io::manipulators::floating_rounding::nearest_to_even,
	::fast_io::details::my_floating_point T,
	::fast_io::details::character char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars(char_type *first, char_type *last, T value) noexcept
{
	if constexpr (
		rounding ==
		::fast_io::manipulators::floating_rounding::current_environment)
	{
		/*
		The floating environment exposes the four IEC hardware modes.  Each
		switch arm tail-calls the corresponding explicit integer policy, so the
		ambient mode is sampled once and no later floating arithmetic can make
		the result drift.
		*/
		switch (::fast_io::details::current_floating_rounding())
		{
		case ::fast_io::manipulators::floating_rounding::
			toward_plus_infinity:
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::
					toward_plus_infinity>(first, last, value);
		case ::fast_io::manipulators::floating_rounding::
			toward_minus_infinity:
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::
					toward_minus_infinity>(first, last, value);
		case ::fast_io::manipulators::floating_rounding::toward_zero:
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::toward_zero>(
					first, last, value);
		default:
			/*
			FE_TONEAREST and every unsupported environment encoding map to
			nearest-to-even, the IEC default returned by
			current_floating_rounding().
			*/
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::
					nearest_to_even>(first, last, value);
		}
	}
	else
	{
		/*
		An explicit policy is already a compile-time constant.  Calling the
		shortest helper directly removes the environment switch entirely.
		*/
		return ::fast_io::details::
			to_chars_floating_standard_shortest<rounding>(
				first, last, value);
	}
}

template <
	::fast_io::manipulators::floating_rounding rounding =
		::fast_io::manipulators::floating_rounding::nearest_to_even,
	::fast_io::details::my_floating_point T,
	::fast_io::details::character char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars(char_type *first, char_type *last, T value,
		 ::std::chars_format format) noexcept
{
	if constexpr (
		rounding ==
		::fast_io::manipulators::floating_rounding::current_environment)
	{
		/*
		As in the three-argument overload, environment dispatch substitutes one
		explicit policy while forwarding the format unchanged.  Therefore it
		changes only the rounding interval, never the grammar selection.
		*/
		switch (::fast_io::details::current_floating_rounding())
		{
		case ::fast_io::manipulators::floating_rounding::
			toward_plus_infinity:
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::
					toward_plus_infinity>(
				first, last, value, format);
		case ::fast_io::manipulators::floating_rounding::
			toward_minus_infinity:
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::
					toward_minus_infinity>(
				first, last, value, format);
		case ::fast_io::manipulators::floating_rounding::toward_zero:
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::toward_zero>(
				first, last, value, format);
		default:
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::
					nearest_to_even>(
				first, last, value, format);
		}
	}
	else
	{
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
		/*
		For a literal format, each conjunction proves both constancy and the
		exact enumerator value before entering its fixed instantiation.  Those
		instantiations are the same callees used by the runtime switch below,
		which proves semantic equivalence by substitution.
		*/
		if (__builtin_constant_p(format) &&
			format == ::std::chars_format::general)
		{
			return ::fast_io::details::to_chars_floating_fixed<
				::std::chars_format::general, rounding>(
				first, last, value);
		}
		if (__builtin_constant_p(format) &&
			format == ::std::chars_format::scientific)
		{
			return ::fast_io::details::to_chars_floating_fixed<
				::std::chars_format::scientific, rounding>(
				first, last, value);
		}
		if (__builtin_constant_p(format) &&
			format == ::std::chars_format::fixed)
		{
			return ::fast_io::details::
				to_chars_floating_standard_fixed<rounding>(
				first, last, value);
		}
		if (__builtin_constant_p(format) &&
			format == ::std::chars_format::hex)
		{
			return ::fast_io::details::
				to_chars_floating_standard_hex<rounding>(
				first, last, value);
		}
#endif
		switch (format)
		{
		/*
		The runtime switch is the disjoint union of the four standard formats.
		General and scientific use their shared carriers directly; fixed and
		hex add only the exact standard-specific branches proved above.
		*/
		case ::std::chars_format::general:
			return ::fast_io::details::to_chars_floating_fixed<
				::std::chars_format::general, rounding>(
				first, last, value);
		case ::std::chars_format::scientific:
			return ::fast_io::details::to_chars_floating_fixed<
				::std::chars_format::scientific, rounding>(
				first, last, value);
		case ::std::chars_format::fixed:
			return ::fast_io::details::
				to_chars_floating_standard_fixed<rounding>(
				first, last, value);
		case ::std::chars_format::hex:
			return ::fast_io::details::
				to_chars_floating_standard_hex<rounding>(
				first, last, value);
		default:
			/*
			No valid grammar corresponds to another bit pattern.  Returning
			`first` before any formatter call proves zero output mutation.
			*/
			return {first, ::std::errc::invalid_argument};
		}
	}
}

template <
	::fast_io::manipulators::floating_rounding rounding =
		::fast_io::manipulators::floating_rounding::nearest_to_even,
	::fast_io::details::my_floating_point T,
	::fast_io::details::character char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars(char_type *first, char_type *last, T value,
		 ::std::chars_format format, int precision) noexcept
{
	if constexpr (
		rounding ==
		::fast_io::manipulators::floating_rounding::current_environment)
	{
		/*
		Precision is forwarded unchanged through environment dispatch.  Since
		each explicit callee owns the same negative-precision normalization,
		sampling the environment cannot alter precision semantics.
		*/
		switch (::fast_io::details::current_floating_rounding())
		{
		case ::fast_io::manipulators::floating_rounding::
			toward_plus_infinity:
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::
					toward_plus_infinity>(
				first, last, value, format, precision);
		case ::fast_io::manipulators::floating_rounding::
			toward_minus_infinity:
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::
					toward_minus_infinity>(
				first, last, value, format, precision);
		case ::fast_io::manipulators::floating_rounding::toward_zero:
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::toward_zero>(
				first, last, value, format, precision);
		default:
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::
					nearest_to_even>(
				first, last, value, format, precision);
		}
	}
	else
	{
		if (precision < 0)
		{
			if (format == ::std::chars_format::general ||
				format == ::std::chars_format::hex)
			{
				/*
				For general and hexadecimal formats, a negative precision means
				"precision omitted"; delegating to the format-only overload is
				therefore exact and also reuses its shortest special cases.
				*/
				return ::fast_io::to_chars<rounding>(
					first, last, value, format);
			}
			/*
			Fixed and scientific use the standard default precision six.
			Replacing the negative sentinel before unsigned conversion prevents
			wraparound and selects exactly the mandated 10^-6 grid.
			*/
			precision = 6;
		}
		/*
		At this point precision>=0, so conversion to size_t is value-preserving
		on every supported platform (size_t can represent all nonnegative int
		values).
		*/
		auto const unsigned_precision{
			static_cast<::std::size_t>(precision)};
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
		/*
		Literal-format dispatch supplies the same runtime precision to the same
		four fixed-format templates used below.  `__builtin_constant_p` removes
		only the format branch; it cannot fold or reinterpret the precision.
		*/
		if (__builtin_constant_p(format) &&
			format == ::std::chars_format::general)
		{
			return ::fast_io::details::to_chars_floating_precision_fixed<
				::std::chars_format::general, rounding>(
				first, last, value, unsigned_precision);
		}
		if (__builtin_constant_p(format) &&
			format == ::std::chars_format::scientific)
		{
			return ::fast_io::details::to_chars_floating_precision_fixed<
				::std::chars_format::scientific, rounding>(
				first, last, value, unsigned_precision);
		}
		if (__builtin_constant_p(format) &&
			format == ::std::chars_format::fixed)
		{
			return ::fast_io::details::to_chars_floating_precision_fixed<
				::std::chars_format::fixed, rounding>(
				first, last, value, unsigned_precision);
		}
		if (__builtin_constant_p(format) &&
			format == ::std::chars_format::hex)
		{
			return ::fast_io::details::to_chars_floating_precision_fixed<
				::std::chars_format::hex, rounding>(
				first, last, value, unsigned_precision);
		}
#endif
		switch (format)
		{
		/*
		All cases share the exact quotient/remainder precision engine.  The
		template format changes only radix, grid definition, and final layout
		as recorded in to_chars_floating_precision_flags.
		*/
		case ::std::chars_format::general:
			return ::fast_io::details::to_chars_floating_precision_fixed<
				::std::chars_format::general, rounding>(
				first, last, value, unsigned_precision);
		case ::std::chars_format::scientific:
			return ::fast_io::details::to_chars_floating_precision_fixed<
				::std::chars_format::scientific, rounding>(
				first, last, value, unsigned_precision);
		case ::std::chars_format::fixed:
			return ::fast_io::details::to_chars_floating_precision_fixed<
				::std::chars_format::fixed, rounding>(
				first, last, value, unsigned_precision);
		case ::std::chars_format::hex:
			return ::fast_io::details::to_chars_floating_precision_fixed<
				::std::chars_format::hex, rounding>(
				first, last, value, unsigned_precision);
		default:
			/*
			An invalid format reaches no writer.  The failure therefore returns
			the original cursor and leaves the destination untouched.
			*/
			return {first, ::std::errc::invalid_argument};
		}
	}
}

} // namespace fast_io
