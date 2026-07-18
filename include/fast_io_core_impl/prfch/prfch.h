#pragma once

namespace fast_io
{

enum class prfch_mode
{
	read = 0,
	write = 1,
	instruction = 2
};

/// @brief Portable locality vocabulary for prefetch hints.
/// @details These names describe the intended closest useful cache level. They are not a promise that a target exposes
///          that exact hierarchy: GCC/Clang's generic builtin accepts only a locality value, and hardware is permitted
///          to ignore or reinterpret every hint.
enum class prfch_level
{
	nta = 0,
	L3 = 1,
	L2 = 2,
	L1 = 3
};

enum class prfch_retention
{
	keep = 0,
	strm = 1
};

/// @brief Emits the closest available non-faulting prefetch hint, or a compile-time no-op on unsupported targets.
/// @details The function itself deliberately performs no run-time feature detection. Instruction prefetch on x86 is
///          emitted only when the translation unit explicitly enables PREFETCHI; data prefetch uses the compiler's
///          generic builtin where available, which GCC documents as a no-op when the target has no lowering. Genuine
///          MSVC uses ISA-specific intrinsics and conservatively degrades write intent to an ordinary temporal hint on
///          x86 rather than assuming PREFETCHW support. The address expression must still be valid C++ even though the
///          generated machine hint is non-faulting. Higher-level bounded helpers own that object-range proof.
template <prfch_mode mode = prfch_mode::write, prfch_level level = prfch_level::nta,
		  prfch_retention retention = prfch_retention::keep>
	requires(static_cast<unsigned>(mode) < 3u && static_cast<unsigned>(level) < 4u &&
			 static_cast<unsigned>(retention) < 2u)
FAST_IO_GNU_ALWAYS_INLINE_ARTIFICIAL inline constexpr void prfch(void const *address) noexcept
{
	if (::std::is_constant_evaluated())
	{
		return;
	}
	if constexpr (mode == prfch_mode::instruction)
	{
#if (defined(__aarch64__) || defined(__arm64__) || defined(__arm64) || defined(__arm64ec__)) && \
	FAST_IO_HAS_BUILTIN(__builtin_arm_prefetch)
		if constexpr (level == prfch_level::nta)
		{
			__builtin_arm_prefetch(address, 0, 0, 1, 0);
		}
		else
		{
			__builtin_arm_prefetch(address, 0, 3 - static_cast<int>(level),
							   static_cast<int>(retention), 0);
		}
#elif (defined(__x86_64__) || defined(__i386__)) && defined(__PREFETCHI__) && \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_prefetchi)
		constexpr int actual_level{static_cast<int>(level) < 2 ? 2 : static_cast<int>(level)};
		__builtin_ia32_prefetchi(address, actual_level);
#else
		(void)address;
#endif
	}
	else
	{
#if (defined(__aarch64__) || defined(__arm64__) || defined(__arm64) || defined(__arm64ec__)) && \
	FAST_IO_HAS_BUILTIN(__builtin_arm_prefetch)
		if constexpr (level == prfch_level::nta)
		{
			__builtin_arm_prefetch(address, static_cast<int>(mode), 0, 1, 1);
		}
		else
		{
			__builtin_arm_prefetch(address, static_cast<int>(mode), 3 - static_cast<int>(level),
							   static_cast<int>(retention), 1);
		}
#elif FAST_IO_HAS_BUILTIN(__builtin_prefetch)
		__builtin_prefetch(address, static_cast<int>(mode), static_cast<int>(level));
#elif defined(_MSC_VER) && !defined(__clang__) && \
	(defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC))
		__prefetch(address);
#elif defined(_MSC_VER) && !defined(__clang__) && \
	(defined(_M_X64) || defined(_M_AMD64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1))
		_mm_prefetch(reinterpret_cast<char const *>(address), static_cast<int>(level));
#else
		(void)address;
#endif
	}
}

} // namespace fast_io
