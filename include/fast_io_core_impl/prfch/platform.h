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
#if (defined(__x86_64__) || defined(__i386__) || defined(__x86__) || defined(_M_X64) || \
	  defined(_M_AMD64) || defined(_M_IX86)) && \
	 !(defined(__arm64ec__) || defined(_M_ARM64EC))
	prfch_isa::x86;
#elif defined(__aarch64__) || defined(__arm64__) || defined(__arm64) || defined(_M_ARM64) || defined(__arm64ec__) || defined(_M_ARM64EC)
	prfch_isa::aarch64;
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
#if defined(__tune_alderlake__)
	prfch_tune::x86_intel_hybrid;
#elif defined(__tune_bonnell__) || defined(__tune_silvermont__) || defined(__tune_goldmont__) || \
	defined(__tune_goldmont_plus__) || defined(__tune_tremont__) || defined(__tune_sierraforest__) || \
	defined(__tune_grandridge__) || defined(__tune_clearwaterforest__)
	prfch_tune::x86_intel_atom;
#elif defined(__tune_core2__) || defined(__tune_nehalem__) || defined(__tune_westmere__) || \
	defined(__tune_sandybridge__) || defined(__tune_ivybridge__) || defined(__tune_haswell__) || \
	defined(__tune_broadwell__) || defined(__tune_skylake__) || defined(__tune_cannonlake__) || \
	defined(__tune_icelake_client__) || defined(__tune_icelake_server__) || defined(__tune_cascadelake__) || \
	defined(__tune_tigerlake__) || defined(__tune_rocketlake__) || defined(__tune_sapphirerapids__) || \
	defined(__tune_emeraldrapids__) || defined(__tune_graniterapids__)
	prfch_tune::x86_intel_core;
#elif defined(__tune_znver1__) || defined(__tune_znver2__) || defined(__tune_znver3__) || \
	defined(__tune_znver4__) || defined(__tune_znver5__)
	prfch_tune::x86_amd_zen;
#elif defined(__tune_k8__) || defined(__tune_amdfam10__) || defined(__tune_bdver1__) || \
	defined(__tune_bdver2__) || defined(__tune_bdver3__) || defined(__tune_bdver4__) || \
	defined(__tune_btver1__) || defined(__tune_btver2__)
	prfch_tune::x86_amd_legacy;
#elif defined(__tune_neoverse_n1__) || defined(__tune_neoverse_n2__) || defined(__tune_neoverse_v1__) || \
	defined(__tune_neoverse_v2__)
	prfch_tune::arm_server;
#elif defined(__tune_cortex_a53__) || defined(__tune_cortex_a55__) || defined(__tune_cortex_a57__) || \
	defined(__tune_cortex_a72__) || defined(__tune_cortex_a76__) || defined(__tune_cortex_a77__) || \
	defined(__tune_cortex_a78__) || defined(__tune_cortex_a510__) || defined(__tune_cortex_a710__) || \
	defined(__tune_cortex_x1__)
	prfch_tune::arm_application;
#else
	prfch_tune::generic;
#endif

inline constexpr bool native_data_prfch_available =
#if FAST_IO_HAS_BUILTIN(__builtin_prefetch) || FAST_IO_HAS_BUILTIN(__builtin_arm_prefetch)
	true;
#elif defined(_MSC_VER) && !defined(__clang__) && \
	(defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC) || \
	 ((defined(_M_X64) || defined(_M_AMD64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))) || \
	 (defined(_M_IX86_FP) && _M_IX86_FP >= 1))
	true;
#else
	false;
#endif

inline constexpr bool native_instruction_prfch_available =
#if (defined(__aarch64__) || defined(__arm64__) || defined(__arm64) || defined(__arm64ec__)) && \
	FAST_IO_HAS_BUILTIN(__builtin_arm_prefetch)
	true;
#elif (defined(__x86_64__) || defined(__i386__)) && defined(__PREFETCHI__) && \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_prefetchi) && \
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
};

} // namespace details

/// @brief Structural contract for a compile-time prefetch platform description.
template <typename platform_type>
concept prfch_platform = requires {
	{ platform_type::isa } -> ::std::convertible_to<prfch_isa>;
	{ platform_type::tune } -> ::std::convertible_to<prfch_tune>;
	{ platform_type::data_available } -> ::std::convertible_to<bool>;
	{ platform_type::instruction_available } -> ::std::convertible_to<bool>;
};

template <typename platform_type>
concept data_prfch_platform = prfch_platform<platform_type> && platform_type::data_available;

template <typename platform_type>
concept instruction_prfch_platform = prfch_platform<platform_type> && platform_type::instruction_available;

template <typename platform_type>
concept x86_prfch_platform = prfch_platform<platform_type> && platform_type::isa == prfch_isa::x86;

template <typename platform_type>
concept arm_prfch_platform =
	prfch_platform<platform_type> &&
	(platform_type::isa == prfch_isa::arm || platform_type::isa == prfch_isa::aarch64);

template <typename platform_type>
concept tuned_prfch_platform = prfch_platform<platform_type> && platform_type::tune != prfch_tune::generic;

} // namespace fast_io
