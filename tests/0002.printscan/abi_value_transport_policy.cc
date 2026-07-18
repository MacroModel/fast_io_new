#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <fast_io_core.h>

namespace abi_value_transport_policy
{

struct one_byte
{
	unsigned char value;
};

struct three_bytes
{
	unsigned char value[3];
};

struct two_words
{
	::std::uintptr_t first;
	::std::uintptr_t second;
};

struct four_words
{
	::std::uintptr_t value[4];
};

struct nontrivial_destructor
{
	::std::uintptr_t value;
	~nontrivial_destructor() {}
};

struct assignment_only_nontrivial
{
	::std::uintptr_t value;

	inline assignment_only_nontrivial &operator=(assignment_only_nontrivial const &other) noexcept
	{
		value = other.value;
		return *this;
	}
};

struct forced_reference
{
	::std::uintptr_t value;
};

inline constexpr ::std::true_type abi_value_transport_force_reference(
	::fast_io::io_type_t<forced_reference>) noexcept
{
	return {};
}

struct forced_direct
{
	::std::uintptr_t value[4];
};

inline constexpr ::std::true_type abi_value_transport_force_direct(
	::fast_io::io_type_t<forced_direct>) noexcept
{
	return {};
}

#if defined(__GNUC__) || defined(__clang__)
struct __attribute__((packed)) packed_misaligned
{
	unsigned char tag;
	::std::uint64_t value;
};

// Outer alignment does not repair the unaligned member at offset one. This second shape prevents a future policy from
// mistaking `alignof(T)` for proof that every SysV eightbyte can be classified independently of member layout.
struct __attribute__((packed, aligned(8))) packed_misaligned_outer_aligned
{
	unsigned char tag;
	::std::uint64_t value;
};

struct __attribute__((packed)) packed_misaligned_forced_reference
{
	unsigned char tag;
	::std::uint64_t value;
};

inline constexpr ::std::true_type abi_value_transport_force_reference(
	::fast_io::io_type_t<packed_misaligned_forced_reference>) noexcept
{
	return {};
}

static_assert(sizeof(packed_misaligned) == 9u && alignof(packed_misaligned) == 1u);
static_assert(sizeof(packed_misaligned_outer_aligned) == 16u &&
			  alignof(packed_misaligned_outer_aligned) == 8u);

// SysV AMD64 assigns both exact types above the MEMORY class because `value` is unaligned. The reflection-free
// envelope intentionally still admits their gross size/alignment: true means that a bounded source copy is eligible,
// not that this exact type is register-lowered. C++20 has no portable field-offset query with which to refine the
// generic rule. A known indirect type can and should use the explicit reference override demonstrated below.
static_assert(::fast_io::details::abi_small_trivial_argument_layout_envelope(
	::fast_io::details::abi_small_aggregate_model::sysv_amd64,
	sizeof(packed_misaligned), alignof(packed_misaligned), false, 16u, 8u));
static_assert(::fast_io::details::abi_small_trivial_argument_layout_envelope(
	::fast_io::details::abi_small_aggregate_model::sysv_amd64,
	sizeof(packed_misaligned_outer_aligned), alignof(packed_misaligned_outer_aligned),
	false, 16u, 8u));
static_assert(!::fast_io::details::abi_small_trivial_argument_object<
	packed_misaligned_forced_reference>());
static_assert(!::fast_io::details::abi_small_trivial_result_object<
	packed_misaligned_forced_reference>());
#endif

static_assert(::std::is_trivially_copy_constructible_v<assignment_only_nontrivial>);
static_assert(::std::is_trivially_move_constructible_v<assignment_only_nontrivial>);
static_assert(::std::is_trivially_destructible_v<assignment_only_nontrivial>);
static_assert(!::std::is_trivially_copyable_v<assignment_only_nontrivial>);

// Assignment never occurs at a call boundary. The common language policy follows the constructors and destructor that
// the helpers actually execute, avoiding the stronger and unrelated `is_trivially_copyable` approximation. Directional
// admission is then checked independently against the argument and aggregate-result envelopes.
static_assert(::fast_io::details::abi_small_trivial_value_language_object<
	assignment_only_nontrivial>());
static_assert(
	::fast_io::details::abi_small_trivial_argument_object<assignment_only_nontrivial>() ==
	::fast_io::details::abi_small_trivial_argument_layout_envelope(
		::fast_io::details::native_abi_small_aggregate_model, sizeof(assignment_only_nontrivial),
		alignof(assignment_only_nontrivial), false,
		::fast_io::details::abi_small_trivial_argument_max_size,
		::fast_io::details::abi_small_trivial_scalar_alignment));
static_assert(
	::fast_io::details::abi_small_trivial_result_object<assignment_only_nontrivial>() ==
	(::fast_io::details::native_abi_small_aggregate_model ==
			 ::fast_io::details::abi_small_aggregate_model::microsoft_x64 ||
		 ::fast_io::details::native_abi_small_aggregate_model ==
			 ::fast_io::details::abi_small_aggregate_model::windows_arm64
		 ? false
		 : ::fast_io::details::abi_small_trivial_result_layout_envelope(
			   ::fast_io::details::native_abi_small_aggregate_model,
			   sizeof(assignment_only_nontrivial), alignof(assignment_only_nontrivial), false,
			   ::fast_io::details::abi_small_trivial_argument_max_size,
			   ::fast_io::details::abi_small_trivial_scalar_alignment)));

static_assert(!::fast_io::details::abi_small_trivial_argument_object<nontrivial_destructor>());
static_assert(!::fast_io::details::abi_small_trivial_result_object<nontrivial_destructor>());
static_assert(!::fast_io::details::abi_small_trivial_argument_object<forced_reference>());
static_assert(!::fast_io::details::abi_small_trivial_result_object<forced_reference>());
static_assert(::fast_io::details::abi_small_trivial_argument_object<forced_direct>());
static_assert(::fast_io::details::abi_small_trivial_result_object<forced_direct>());
static_assert(
	::fast_io::details::print_forward_result_copied_from_named_value<one_byte &>() ==
	::fast_io::details::abi_small_trivial_result_object<one_byte>());

// The Microsoft result layout admits an exact one-byte shape, but generic C++20 traits cannot prove the ABI's recursive
// C++03-POD-like restrictions. Model-level testing therefore verifies the fail-closed object policy locally rather than
// depending on a Windows runner; only a scalar or an explicit type-author proof can recover direct result transport.
static_assert(!::fast_io::details::abi_small_trivial_result_object_for_model<one_byte>(
	::fast_io::details::abi_small_aggregate_model::microsoft_x64, 8u, 8u));
static_assert(!::fast_io::details::abi_small_trivial_result_object_for_model<
	assignment_only_nontrivial>(
	::fast_io::details::abi_small_aggregate_model::microsoft_x64, 8u, 8u));
static_assert(::fast_io::details::abi_small_trivial_result_object_for_model<unsigned>(
	::fast_io::details::abi_small_aggregate_model::microsoft_x64, 8u, 8u));
static_assert(::fast_io::details::abi_small_trivial_result_object_for_model<forced_direct>(
	::fast_io::details::abi_small_aggregate_model::microsoft_x64, 8u, 8u));

// Windows ARM64/ARM64EC retain AAPCS64's two-register argument opportunity but impose similarly unreflectable class
// conditions on results. The distinct model must therefore reject an unmarked class without penalizing scalar results
// or a type carrying the strong explicit direct-transport contract.
static_assert(!::fast_io::details::abi_small_trivial_result_object_for_model<two_words>(
	::fast_io::details::abi_small_aggregate_model::windows_arm64, 16u, 8u));
static_assert(::fast_io::details::abi_small_trivial_result_object_for_model<::std::uintptr_t>(
	::fast_io::details::abi_small_aggregate_model::windows_arm64, 16u, 8u));
static_assert(::fast_io::details::abi_small_trivial_result_object_for_model<forced_direct>(
	::fast_io::details::abi_small_aggregate_model::windows_arm64, 16u, 8u));

inline consteval bool every_abi_argument_layout_envelope_matches_bounded_copy_policy()
{
	using enum ::fast_io::details::abi_small_aggregate_model;
	using ::fast_io::details::abi_small_trivial_argument_layout_envelope;

	// Microsoft x64 and s390 have integer-equivalent aggregate classes with exact widths. In particular, accepting an
	// odd-sized aggregate merely because it is below one word would incorrectly predict register transport.
	if (!abi_small_trivial_argument_layout_envelope(microsoft_x64, 1u, 1u, false, 8u, 8u) ||
		abi_small_trivial_argument_layout_envelope(microsoft_x64, 3u, 1u, false, 8u, 8u) ||
		!abi_small_trivial_argument_layout_envelope(
			s390_integer_equivalent, 8u, 8u, false, 8u, 8u) ||
		abi_small_trivial_argument_layout_envelope(
			s390_integer_equivalent, 3u, 1u, false, 8u, 8u))
	{
		return false;
	}

	// These psABIs all have a bounded direct aggregate envelope. Their field-level register classes differ and remain
	// compiler decisions, so each entry verifies only the 32- or 64-bit two-word boundary supplied to the policy.
	struct model_limit
	{
		::fast_io::details::abi_small_aggregate_model model;
		::std::size_t maximum_size;
	};
	constexpr model_limit direct_models[]{
		{sysv_amd64, 16u}, {aapcs64, 16u}, {windows_arm64, 16u}, {aapcs32, 8u},
		{riscv_integer, 8u}, {riscv_integer, 16u}, {loongarch, 8u}, {loongarch, 16u},
		{powerpc64_elfv1, 16u}, {powerpc64_elfv2, 16u}, {mips_o32, 8u},
		{mips_n32_n64, 16u}, {sparc_v9, 16u}};
	for (auto const entry : direct_models)
	{
		if (!abi_small_trivial_argument_layout_envelope(
				entry.model, entry.maximum_size, entry.maximum_size, false, entry.maximum_size, 8u) ||
			abi_small_trivial_argument_layout_envelope(
				entry.model, entry.maximum_size + 1u, 1u, false, entry.maximum_size, 8u) ||
			abi_small_trivial_argument_layout_envelope(
				entry.model, 1u, entry.maximum_size * 2u, false, entry.maximum_size, 8u))
		{
			return false;
		}
	}

	// AAPCS64 HFA/HVA classes may remain direct beyond 16 bytes, but neither byte layout nor C++20 traits can prove
	// homogeneity. The generic policy deliberately rejects that false-negative shape; an exact target-specific type may
	// opt in through the strong direct-transport contract instead of broadening every ordinary aggregate.
	if (abi_small_trivial_argument_layout_envelope(aapcs64, 32u, 8u, false, 16u, 8u))
	{
		return false;
	}

	// CHERI, caller-storage aggregate ABIs (PowerPC32/SPARC V8/Wasm), and unknown targets never infer an aggregate
	// register class from byte size. Scalar admission remains bounded so a type-specific direct marker is required for
	// any aggregate exception the compiler can see but portable C++20 reflection cannot describe.
	constexpr ::fast_io::details::abi_small_aggregate_model scalar_only_models[]{
		capability_scalar, aggregate_indirect_or_stack, unmodelled};
	for (auto model : scalar_only_models)
	{
		if (abi_small_trivial_argument_layout_envelope(
				model, 8u, 8u, false, 16u, 16u) ||
			!abi_small_trivial_argument_layout_envelope(
				model, 8u, 8u, true, 16u, 16u))
		{
			return false;
		}
	}
	return true;
}

static_assert(every_abi_argument_layout_envelope_matches_bounded_copy_policy());

inline consteval bool asymmetric_abi_result_envelopes_are_conservative()
{
	using enum ::fast_io::details::abi_small_aggregate_model;
	using ::fast_io::details::abi_small_trivial_result_layout_envelope;

	// AAPCS32 returns at most four bytes of composite data in r0. An eight-byte scalar still has a direct scalar result
	// class, which proves that the aggregate result restriction cannot be represented by shrinking the argument budget.
	if (!abi_small_trivial_result_layout_envelope(aapcs32, 4u, 4u, false, 8u, 4u) ||
		abi_small_trivial_result_layout_envelope(aapcs32, 8u, 4u, false, 8u, 4u) ||
		abi_small_trivial_result_layout_envelope(aapcs32, 4u, 4u, false, 3u, 4u) ||
		!abi_small_trivial_result_layout_envelope(aapcs32, 8u, 8u, true, 8u, 8u))
	{
		return false;
	}

	// These three psABI families admit useful aggregate argument classes but return every aggregate through hidden
	// caller storage. Scalars remain direct and therefore test the intentional direction split rather than a blanket
	// model rejection.
	constexpr ::fast_io::details::abi_small_aggregate_model indirect_result_models[]{
		mips_o32, s390_integer_equivalent, powerpc64_elfv1};
	for (auto model : indirect_result_models)
	{
		if (abi_small_trivial_result_layout_envelope(model, 1u, 1u, false, 16u, 16u) ||
			!abi_small_trivial_result_layout_envelope(model, 8u, 8u, true, 16u, 16u))
		{
			return false;
		}
	}

	// ELFv2 is deliberately distinct from ELFv1: it returns an aggregate of at most two doublewords directly.
	if (!abi_small_trivial_result_layout_envelope(
			powerpc64_elfv2, 16u, 16u, false, 16u, 8u) ||
		abi_small_trivial_result_layout_envelope(
			powerpc64_elfv2, 17u, 1u, false, 16u, 8u))
	{
		return false;
	}

	// Unknown ABI models may retain bounded scalar transport, but aggregate copies never become optional merely because
	// the object is small. This is the fail-closed behavior required for a target without an explicit result proof.
	return !abi_small_trivial_result_layout_envelope(unmodelled, 1u, 1u, false, 8u, 8u) &&
		   abi_small_trivial_result_layout_envelope(unmodelled, 8u, 8u, true, 8u, 8u);
}

static_assert(asymmetric_abi_result_envelopes_are_conservative());

inline consteval bool native_size_classes_are_consistent()
{
	using enum ::fast_io::details::abi_small_aggregate_model;
	constexpr auto model{::fast_io::details::native_abi_small_aggregate_model};
	if constexpr (model == microsoft_x64)
	{
		// Microsoft x64 carries only exact 8/16/32/64-bit aggregates as integer arguments. An odd-sized class is
		// indirect even though its byte count is below the nominal eight-byte ceiling. Results add recursive C++03-POD-like
		// conditions which generic C++20 traits cannot prove, so an unmarked class is fail-closed while an explicit direct
		// marker recovers the result path.
		return ::fast_io::details::abi_small_trivial_argument_object<one_byte>() &&
			   !::fast_io::details::abi_small_trivial_result_object<one_byte>() &&
			   !::fast_io::details::abi_small_trivial_argument_object<three_bytes>() &&
			   !::fast_io::details::abi_small_trivial_result_object<three_bytes>() &&
			   !::fast_io::details::abi_small_trivial_argument_object<two_words>() &&
			   ::fast_io::details::abi_small_trivial_result_object<forced_direct>();
	}
	else if constexpr (model == windows_arm64)
	{
		// The Windows model keeps ordinary one- and two-word aggregate arguments, but generic class results require the
		// explicit direct marker because their recursive shape restrictions are unavailable to C++20 traits.
		return ::fast_io::details::abi_small_trivial_argument_object<one_byte>() &&
			   ::fast_io::details::abi_small_trivial_argument_object<two_words>() &&
			   !::fast_io::details::abi_small_trivial_result_object<one_byte>() &&
			   !::fast_io::details::abi_small_trivial_result_object<two_words>() &&
			   ::fast_io::details::abi_small_trivial_result_object<forced_direct>();
	}
	else if constexpr (model == capability_scalar)
	{
		// CHERI targets have direct scalar classes for ordinary and capability pointers. An arbitrary wrapper may carry
		// capability tags or mixed integer fields, so aggregate admission requires a type-specific proof.
		return ::fast_io::details::abi_small_trivial_argument_object<void *>() &&
			   ::fast_io::details::abi_small_trivial_result_object<void *>() &&
			   !::fast_io::details::abi_small_trivial_argument_object<one_byte>() &&
			   !::fast_io::details::abi_small_trivial_result_object<one_byte>() &&
			   !::fast_io::details::abi_small_trivial_argument_object<two_words>();
	}
	else if constexpr (model == aggregate_indirect_or_stack || model == unmodelled)
	{
		// PowerPC32, SPARC V8, i386, and the basic Wasm C ABI provide no generic class-register guarantee. The common
		// policy therefore admits only scalars. Unknown targets make the same conservative choice until their psABI
		// receives an explicit model.
		return !::fast_io::details::abi_small_trivial_argument_object<one_byte>() &&
			   !::fast_io::details::abi_small_trivial_result_object<one_byte>() &&
			   !::fast_io::details::abi_small_trivial_argument_object<two_words>();
	}
	else if constexpr (model == s390_integer_equivalent)
	{
		return ::fast_io::details::abi_small_trivial_argument_object<one_byte>() &&
			   !::fast_io::details::abi_small_trivial_argument_object<three_bytes>() &&
			   !::fast_io::details::abi_small_trivial_result_object<one_byte>();
	}
	else if constexpr (model == aapcs32 || model == mips_o32 || model == powerpc64_elfv1)
	{
		constexpr bool one_byte_result_expected{model == aapcs32};
		return ::fast_io::details::abi_small_trivial_argument_object<one_byte>() &&
			   (::fast_io::details::abi_small_trivial_result_object<one_byte>() ==
				one_byte_result_expected) &&
			   (sizeof(two_words) <= ::fast_io::details::abi_small_trivial_argument_max_size
					? (::fast_io::details::abi_small_trivial_argument_object<two_words>() &&
					   !::fast_io::details::abi_small_trivial_result_object<two_words>())
					: !::fast_io::details::abi_small_trivial_argument_object<two_words>());
	}
	else
	{
		return ::fast_io::details::abi_small_trivial_argument_object<one_byte>() &&
			   ::fast_io::details::abi_small_trivial_result_object<one_byte>() &&
			   (sizeof(two_words) <= ::fast_io::details::abi_small_trivial_argument_max_size
					? (::fast_io::details::abi_small_trivial_argument_object<two_words>() &&
					   ::fast_io::details::abi_small_trivial_result_object<two_words>())
					: (!::fast_io::details::abi_small_trivial_argument_object<two_words>() &&
					   !::fast_io::details::abi_small_trivial_result_object<two_words>()));
	}
}

static_assert(native_size_classes_are_consistent());

#if defined(__CHERI__) || defined(__CHERI_PURE_CAPABILITY__)
static_assert(::fast_io::details::native_abi_small_aggregate_model ==
			  ::fast_io::details::abi_small_aggregate_model::capability_scalar);
#elif defined(__arm64ec__) || defined(_M_ARM64EC)
static_assert(::fast_io::details::native_abi_small_aggregate_model ==
			  ::fast_io::details::abi_small_aggregate_model::windows_arm64);
static_assert(::fast_io::details::abi_small_trivial_argument_max_size == 16u);
#elif (defined(_WIN32) || defined(__CYGWIN__)) && \
	(defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__))
static_assert(::fast_io::details::native_abi_small_aggregate_model ==
			  ::fast_io::details::abi_small_aggregate_model::microsoft_x64);
static_assert(::fast_io::details::abi_small_trivial_argument_max_size == 8u);
#elif defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
static_assert(::fast_io::details::native_abi_small_aggregate_model ==
			  ::fast_io::details::abi_small_aggregate_model::sysv_amd64);
static_assert(::fast_io::details::abi_small_trivial_argument_max_size == 16u);
#elif defined(_WIN32) && (defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64))
static_assert(::fast_io::details::native_abi_small_aggregate_model ==
			  ::fast_io::details::abi_small_aggregate_model::windows_arm64);
static_assert(::fast_io::details::abi_small_trivial_argument_max_size == 16u);
#elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
static_assert(::fast_io::details::native_abi_small_aggregate_model ==
			  ::fast_io::details::abi_small_aggregate_model::aapcs64);
static_assert(::fast_io::details::abi_small_trivial_argument_max_size == 16u);
#elif defined(_M_ARM) || (defined(__arm__) && \
	(defined(__ARM_EABI__) || defined(__ARM_PCS) || defined(__APPLE__)))
static_assert(::fast_io::details::native_abi_small_aggregate_model ==
			  ::fast_io::details::abi_small_aggregate_model::aapcs32);
static_assert(::fast_io::details::abi_small_trivial_argument_max_size == 8u);
#elif defined(__riscv)
static_assert(::fast_io::details::native_abi_small_aggregate_model ==
			  ::fast_io::details::abi_small_aggregate_model::riscv_integer);
#if defined(__riscv_xlen)
static_assert(::fast_io::details::abi_small_trivial_argument_max_size ==
			  2u * (static_cast<::std::size_t>(__riscv_xlen) / 8u));
#endif
#elif defined(__loongarch__)
static_assert(::fast_io::details::native_abi_small_aggregate_model ==
			  ::fast_io::details::abi_small_aggregate_model::loongarch);
#if defined(__loongarch_grlen)
static_assert(::fast_io::details::abi_small_trivial_argument_max_size ==
			  2u * (static_cast<::std::size_t>(__loongarch_grlen) / 8u));
#endif
#elif (defined(__powerpc64__) || defined(__ppc64__) || defined(_ARCH_PPC64)) && \
	defined(_CALL_ELF) && _CALL_ELF == 1
static_assert(::fast_io::details::native_abi_small_aggregate_model ==
			  ::fast_io::details::abi_small_aggregate_model::powerpc64_elfv1);
static_assert(::fast_io::details::abi_small_trivial_argument_max_size == 16u);
#elif (defined(__powerpc64__) || defined(__ppc64__) || defined(_ARCH_PPC64)) && \
	defined(_CALL_ELF) && _CALL_ELF == 2
static_assert(::fast_io::details::native_abi_small_aggregate_model ==
			  ::fast_io::details::abi_small_aggregate_model::powerpc64_elfv2);
static_assert(::fast_io::details::abi_small_trivial_argument_max_size == 16u);
#elif (defined(__s390__) || defined(__s390x__)) && defined(__ELF__) && !defined(__MVS__)
static_assert(::fast_io::details::native_abi_small_aggregate_model ==
			  ::fast_io::details::abi_small_aggregate_model::s390_integer_equivalent);
static_assert(::fast_io::details::abi_small_trivial_argument_max_size == 8u);
#elif defined(__s390__) || defined(__s390x__)
static_assert(::fast_io::details::native_abi_small_aggregate_model ==
			  ::fast_io::details::abi_small_aggregate_model::unmodelled);
#elif defined(__mips_o32) || \
	(defined(_MIPS_SIM) && defined(_ABIO32) && _MIPS_SIM == _ABIO32) || \
	(defined(_MIPS_SIM) && defined(_MIPS_SIM_ABI32) && _MIPS_SIM == _MIPS_SIM_ABI32)
static_assert(::fast_io::details::native_abi_small_aggregate_model ==
			  ::fast_io::details::abi_small_aggregate_model::mips_o32);
static_assert(::fast_io::details::abi_small_trivial_argument_max_size == 8u);
#elif defined(__mips_n32) || defined(__mips_n64) || \
	(defined(_MIPS_SIM) && defined(_ABIN32) && _MIPS_SIM == _ABIN32) || \
	(defined(_MIPS_SIM) && defined(_ABI64) && _MIPS_SIM == _ABI64) || \
	(defined(_MIPS_SIM) && defined(_MIPS_SIM_NABI32) && _MIPS_SIM == _MIPS_SIM_NABI32) || \
	(defined(_MIPS_SIM) && defined(_MIPS_SIM_ABI64) && _MIPS_SIM == _MIPS_SIM_ABI64)
static_assert(::fast_io::details::native_abi_small_aggregate_model ==
			  ::fast_io::details::abi_small_aggregate_model::mips_n32_n64);
static_assert(::fast_io::details::abi_small_trivial_argument_max_size == 16u);
#elif (defined(__sparc__) || defined(__sparc)) && \
	(defined(__arch64__) || defined(__sparcv9) || defined(__sparcv9__) || defined(__sparc64__))
static_assert(::fast_io::details::native_abi_small_aggregate_model ==
			  ::fast_io::details::abi_small_aggregate_model::sparc_v9);
static_assert(::fast_io::details::abi_small_trivial_argument_max_size == 16u);
#elif defined(__wasm__) || defined(__wasm32__) || defined(__wasm64__)
static_assert(::fast_io::details::native_abi_small_aggregate_model ==
			  ::fast_io::details::abi_small_aggregate_model::aggregate_indirect_or_stack);
#elif defined(__arm__) || defined(_M_ARM) || defined(__powerpc__) || defined(__ppc__) || \
	defined(_M_PPC) || defined(__mips__) || defined(__mips) || defined(__sparc__) || \
	defined(__sparc) || defined(__i386__) || defined(_M_IX86)
static_assert(::fast_io::details::native_abi_small_aggregate_model ==
			  ::fast_io::details::abi_small_aggregate_model::aggregate_indirect_or_stack);
#else
static_assert(::fast_io::details::native_abi_small_aggregate_model ==
			  ::fast_io::details::abi_small_aggregate_model::unmodelled);
#endif

} // namespace abi_value_transport_policy

int main() {}
