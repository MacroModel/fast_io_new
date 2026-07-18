#pragma once

namespace fast_io
{

/// @brief Broad instruction-set families relevant to prefetch lowering and strategy selection.
/// @details This is deliberately not an ABI enumeration. Several ABIs may share one instruction set, while a single
///          ISA may cover cores with very different memory systems. Higher-level policies must therefore combine this
///          value with a source-lifetime proof and measured cost policy; ISA membership alone never authorizes a hint.
enum class prfch_isa
{
	unknown,
	x86,
	arm,
	aarch64,
	riscv,
	powerpc,
	mips,
	s390,
	loongarch,
	sparc,
	hexagon,
	wasm,
	avr,
	bpf,
	other
};

/// @brief Conservative compiler-tuning families used only to choose measured prefetch policies.
/// @details GCC exposes selected x86 `-mtune` choices as `__tune_*` macros, often canonicalizing multiple CPU names to
///          one macro. Clang and MSVC generally do not provide an equivalent preprocessor contract. `generic` therefore
///          means "no usable compile-time tune proof", not "a slow CPU". The categories intentionally remain broad: a
///          header-only library cannot safely choose a microarchitecture-maximal distance for every processor that can
///          execute the same binary.
enum class prfch_tune
{
	generic,
	x86_intel_core,
	x86_intel_atom,
	x86_intel_hybrid,
	x86_amd_zen,
	x86_amd_legacy,
	arm_application,
	arm_server,
	other_known
};

namespace details
{

inline constexpr prfch_isa native_prfch_isa =
#if defined(__aarch64__) || defined(__arm64__) || defined(__arm64) || defined(_M_ARM64) || \
	defined(__arm64ec__) || defined(_M_ARM64EC)
	prfch_isa::aarch64;
#elif (defined(__x86_64__) || defined(__i386__) || defined(__x86__) || defined(_M_X64) || \
	   defined(_M_AMD64) || defined(_M_IX86)) &&                                          \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	prfch_isa::x86;
#elif defined(__arm__) || defined(_M_ARM)
	prfch_isa::arm;
#elif defined(__riscv)
	prfch_isa::riscv;
#elif defined(__powerpc__) || defined(__powerpc64__) || defined(__ppc__) || defined(__PPC__) || defined(_M_PPC)
	prfch_isa::powerpc;
#elif defined(__mips__) || defined(__mips)
	prfch_isa::mips;
#elif defined(__s390__) || defined(__s390x__)
	prfch_isa::s390;
#elif defined(__loongarch__)
	prfch_isa::loongarch;
#elif defined(__sparc__) || defined(__sparc)
	prfch_isa::sparc;
#elif defined(__hexagon__)
	prfch_isa::hexagon;
#elif defined(__wasm__)
	prfch_isa::wasm;
#elif defined(__AVR__)
	prfch_isa::avr;
#elif defined(__bpf__)
	prfch_isa::bpf;
#else
	prfch_isa::unknown;
#endif

inline constexpr prfch_tune native_prfch_tune =
// GCC's target backend defines `__tune_*` according to the selected x86 tuning model. Clang intentionally exposes
// compatibility macros such as `__tune_k8__` even when `-mtune` names an unrelated Intel or AMD core; treating those
// macros as evidence misclassifies every Clang x86-64 translation unit. Unknown compiler families therefore remain
// generic unless they acquire a documented, tested contract of their own. Genuine MSVC x86 is one narrow exception:
// `/favor:ATOM` has the documented `__ATOM__ == 1` preprocessor contract. Other `/favor` modes remain unclassified,
// and ARM64EC is explicitly excluded because x64 compatibility macros do not prove that its native ISA is x86.
#if defined(_MSC_VER) && !defined(__clang__) && defined(__ATOM__) && __ATOM__ == 1 && \
	(defined(_M_X64) || defined(_M_AMD64) || defined(_M_IX86)) &&                     \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	prfch_tune::x86_intel_atom;
#elif defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) && \
	!defined(__INTEL_LLVM_COMPILER)
#if defined(__tune_alderlake__) || defined(__tune_arrowlake__) || defined(__tune_arrowlake_s__) || \
	defined(__tune_pantherlake__) || defined(__tune_novalake__)
	prfch_tune::x86_intel_hybrid;
#elif defined(__tune_bonnell__) || defined(__tune_silvermont__) || defined(__tune_goldmont__) ||      \
	defined(__tune_goldmont_plus__) || defined(__tune_tremont__) || defined(__tune_sierraforest__) || \
	defined(__tune_grandridge__) || defined(__tune_clearwaterforest__)
	prfch_tune::x86_intel_atom;
#elif defined(__tune_core2__) || defined(__tune_nehalem__) || defined(__tune_westmere__) ||                  \
	defined(__tune_sandybridge__) || defined(__tune_ivybridge__) || defined(__tune_haswell__) ||             \
	defined(__tune_broadwell__) || defined(__tune_skylake__) || defined(__tune_cannonlake__) ||              \
	defined(__tune_skylake_avx512__) || defined(__tune_cooperlake__) ||                                      \
	defined(__tune_icelake_client__) || defined(__tune_icelake_server__) || defined(__tune_cascadelake__) || \
	defined(__tune_tigerlake__) || defined(__tune_rocketlake__) || defined(__tune_sapphirerapids__) ||       \
	defined(__tune_emeraldrapids__) || defined(__tune_graniterapids__) ||                                    \
	defined(__tune_graniterapids_d__) || defined(__tune_diamondrapids__)
	prfch_tune::x86_intel_core;
#elif defined(__tune_znver1__) || defined(__tune_znver2__) || defined(__tune_znver3__) || \
	defined(__tune_znver4__) || defined(__tune_znver5__) || defined(__tune_znver6__)
	prfch_tune::x86_amd_zen;
#elif defined(__tune_k8__) || defined(__tune_amdfam10__) || defined(__tune_bdver1__) || \
	defined(__tune_bdver2__) || defined(__tune_bdver3__) || defined(__tune_bdver4__) || \
	defined(__tune_btver1__) || defined(__tune_btver2__)
	// GCC also canonicalizes a few non-AMD CPU names to its K8 scheduling model. This enum records the selected cost
	// family, not a run-time vendor assertion, and the conservative policy keeps the whole class disabled initially.
	prfch_tune::x86_amd_legacy;
#elif defined(__tune_neoverse_n1__) || defined(__tune_neoverse_n2__) || defined(__tune_neoverse_v1__) || \
	defined(__tune_neoverse_v2__)
	prfch_tune::arm_server;
#elif defined(__tune_cortex_a53__) || defined(__tune_cortex_a55__) || defined(__tune_cortex_a57__) || \
	defined(__tune_cortex_a72__) || defined(__tune_cortex_a76__) || defined(__tune_cortex_a77__) ||   \
	defined(__tune_cortex_a78__) || defined(__tune_cortex_a510__) || defined(__tune_cortex_a710__) || \
	defined(__tune_cortex_x1__)
	prfch_tune::arm_application;
#else
	prfch_tune::generic;
#endif
#else
	prfch_tune::generic;
#endif

inline constexpr bool native_data_prfch_available =
#if FAST_IO_HAS_BUILTIN(__builtin_prefetch) || FAST_IO_HAS_BUILTIN(__builtin_arm_prefetch)
	true;
#elif defined(_MSC_VER) && !defined(__clang__) &&                                                  \
	(defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC) ||                                \
	 ((defined(_M_X64) || defined(_M_AMD64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))) || \
	 (defined(_M_IX86_FP) && _M_IX86_FP >= 1))
	true;
#else
	false;
#endif

inline constexpr bool native_instruction_prfch_available =
#if (defined(__aarch64__) || defined(__arm64__) || defined(__arm64) || defined(__arm64ec__) || \
	 defined(_M_ARM64) || defined(_M_ARM64EC)) &&                                              \
	FAST_IO_HAS_BUILTIN(__builtin_arm_prefetch)
	true;
#elif (defined(__aarch64__) || defined(__arm64__) || defined(__arm64)) && \
	FAST_IO_HAS_BUILTIN(__builtin_aarch64_plix)
	true;
#elif defined(__aarch64__) && defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 16
	true;
#elif defined(__arm__) && FAST_IO_HAS_BUILTIN(__builtin_arm_prefetch)
			true;
#elif defined(_MSC_VER) && !defined(__clang__) && (defined(_M_ARM64) || defined(_M_ARM64EC))
			true;
#elif defined(__x86_64__) && defined(__PREFETCHI__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_prefetchi) && \
	defined(__clang__) && !defined(__INTEL_LLVM_COMPILER) &&                                            \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
			// Clang exposes the pointer-shaped intrinsic through llvm.prefetch.  Only a final RIP-relative operand is
			// architecturally effective; a non-RIP-relative form is ignored.  The dynamic API preserves Clang's documented
			// syntax, while `prfch_instruction_local` expresses the stronger caller-owned effectiveness precondition.
	true;
#else
			false;
#endif

/// @brief Whether the direct function-address convenience API has a compiler lowering.
/// @details AArch64 and ARM can use the same general-address PLI operation as the dynamic API. Clang x86 preserves its
///          pointer-shaped PREFETCHI builtin for a statically named target, but only a locally bound/hidden target is
///          guaranteed to become an effective RIP-relative operand. Genuine GCC is deliberately excluded from this
///          convenience: its target builtin diagnoses operands which cannot be materialized RIP-relatively, so GCC's
///          support belongs only to the explicit local-binding API below.
inline constexpr bool native_direct_instruction_prfch_available =
	native_instruction_prfch_available;

/// @brief Whether an explicitly asserted locally-bound function can use x86 PREFETCHI.
/// @details Clang and genuine GCC intentionally share only this explicit proof surface, not their compiler behavior.
///          GCC diagnoses a non-RIP-relative operand; Clang accepts a register-indirect spelling which the architecture
///          ignores. Standard C++ cannot inspect ELF/COFF symbol binding, so the token remains a caller assertion in
///          both branches. This flag reports compiler/ISA support, not that a particular function satisfies it.
inline constexpr bool native_local_instruction_prfch_available =
#if (defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)) && defined(__PREFETCHI__) && \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_prefetchi) &&                                           \
	((defined(__clang__) && !defined(__INTEL_LLVM_COMPILER)) ||                                \
	 (defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) &&                \
	  !defined(__INTEL_LLVM_COMPILER))) &&                                                     \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	true;
#else
	false;
#endif

struct native_prfch_platform
{
	inline static constexpr prfch_isa isa{native_prfch_isa};
	inline static constexpr prfch_tune tune{native_prfch_tune};
	inline static constexpr bool data_available{native_data_prfch_available};
	inline static constexpr bool instruction_available{native_instruction_prfch_available};
	inline static constexpr bool direct_instruction_available{native_direct_instruction_prfch_available};
	inline static constexpr bool local_instruction_available{native_local_instruction_prfch_available};
};

} // namespace details

/// @brief Structural contract for a compile-time prefetch platform description.
template <typename platform_type>
concept prfch_platform = requires {
	{ platform_type::isa } -> ::std::convertible_to<prfch_isa>;
	{ platform_type::tune } -> ::std::convertible_to<prfch_tune>;
	{ platform_type::data_available } -> ::std::convertible_to<bool>;
	{ platform_type::instruction_available } -> ::std::convertible_to<bool>;
	typename ::std::integral_constant<prfch_isa, static_cast<prfch_isa>(platform_type::isa)>;
	typename ::std::integral_constant<prfch_tune, static_cast<prfch_tune>(platform_type::tune)>;
	typename ::std::bool_constant<static_cast<bool>(platform_type::data_available)>;
	typename ::std::bool_constant<static_cast<bool>(platform_type::instruction_available)>;
};

template <typename platform_type>
concept data_prfch_platform = prfch_platform<platform_type> && platform_type::data_available;

/// @brief Identifies a platform where the generic instruction-prefetch syntax has a compiler lowering.
/// @details This is not an address-binding proof. In particular, Clang x86 accepts a pointer operand although only a
///          final RIP-relative instruction is architecturally effective. Site code which requires that stronger fact
///          must use `local_instruction_prfch_platform` together with `prfch_local_function_t`.
template <typename platform_type>
concept instruction_prfch_platform = prfch_platform<platform_type> && platform_type::instruction_available;

/// @brief Identifies a platform where the direct function-address convenience has a compiler lowering.
/// @details Custom descriptions opt in with a constant `direct_instruction_available` member. Keeping this optional
///          field outside `prfch_platform` preserves source compatibility for existing four-field platform models.
///          x86's stricter RIP-relative local-binding contract belongs to `local_instruction_prfch_platform` instead.
template <typename platform_type>
concept direct_instruction_prfch_platform = instruction_prfch_platform<platform_type> && requires {
	{ platform_type::direct_instruction_available } -> ::std::convertible_to<bool>;
	typename ::std::bool_constant<static_cast<bool>(platform_type::direct_instruction_available)>;
} && platform_type::direct_instruction_available;

/// @brief Identifies a platform which can lower an explicit caller assertion of local function binding.
/// @details The optional member is kept outside `prfch_platform`, as with the direct capability, so existing custom
///          four-field descriptions remain valid. A true value does not prove the binding of a concrete symbol; the
///          `prfch_local_function_t` token carries that separate caller-owned precondition.
template <typename platform_type>
concept local_instruction_prfch_platform = prfch_platform<platform_type> && requires {
	{ platform_type::local_instruction_available } -> ::std::convertible_to<bool>;
	typename ::std::bool_constant<static_cast<bool>(platform_type::local_instruction_available)>;
} && platform_type::local_instruction_available;

template <typename platform_type>
concept x86_prfch_platform = prfch_platform<platform_type> && platform_type::isa == prfch_isa::x86;

template <typename platform_type>
concept arm_prfch_platform =
	prfch_platform<platform_type> &&
	(platform_type::isa == prfch_isa::arm || platform_type::isa == prfch_isa::aarch64);

template <typename platform_type>
concept tuned_prfch_platform = prfch_platform<platform_type> && platform_type::tune != prfch_tune::generic;

} // namespace fast_io
