#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include <fast_io_freestanding.h>

#if __has_cpp_attribute(__gnu__::__noinline__)
#define FAST_IO_OLD_NEW_CODEGEN_NOINLINE [[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
#define FAST_IO_OLD_NEW_CODEGEN_NOINLINE [[msvc::noinline]]
#else
#define FAST_IO_OLD_NEW_CODEGEN_NOINLINE
#endif

/*
This translation unit separates the uint64 decimal arithmetic leaf from the
public fixed-output CPO.  Both functions have C linkage and a forced external
boundary, giving assembly diff, symbol-size, and llvm-mca tools stable names
without relying on compiler-specific template manglings.  The caller owns 64
writable bytes, which exceeds the maximum decimal reserve extent and makes all
stores semantically observable through the escaped pointer.

The LLVM-MCA comments bracket only the operation under study.  They are inert
assembler comments on every supported compiler; llvm-mca recognizes them when
present and ignores prologue, epilogue, and the executable smoke-test driver.
*/
extern "C" FAST_IO_OLD_NEW_CODEGEN_NOINLINE char *
fast_io_old_new_reserve_u64(char *destination,
							::std::uint_least64_t value) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("# LLVM-MCA-BEGIN fast_io_old_new_reserve_u64");
#endif
	auto normalized{::fast_io::io_print_forward<char>(
		::fast_io::io_print_alias(value))};
	using normalized_type = ::std::remove_cvref_t<decltype(normalized)>;
	auto *const end{print_reserve_define(
		::fast_io::io_reserve_type<char, normalized_type>, destination,
		normalized)};
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("# LLVM-MCA-END fast_io_old_new_reserve_u64");
#endif
	return end;
}

extern "C" FAST_IO_OLD_NEW_CODEGEN_NOINLINE ::std::size_t
fast_io_old_new_fixed_print_u64(char *destination,
								::std::uint_least64_t value) noexcept
{
	::fast_io::basic_obuffer_view<char> output{destination, destination + 64u};
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("# LLVM-MCA-BEGIN fast_io_old_new_fixed_print_u64");
#endif
	::fast_io::operations::print_freestanding<false>(output, value);
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("# LLVM-MCA-END fast_io_old_new_fixed_print_u64");
#endif
	return output.size();
}

/*
This exact-width reference is not an alternate public formatter. Its caller
must prove 10^19 <= value <= UINT64_MAX, which makes the leading block exactly
four digits and both remainders exactly eight digits with zero padding. The
function exists so llvm-mca can analyze the resolved 20-digit arithmetic/store
kernel without linearly executing mutually exclusive dispatcher blocks. The
whole-call runtime benchmark remains authoritative for branch prediction,
alignment, and instruction-cache effects.
*/
extern "C" FAST_IO_OLD_NEW_CODEGEN_NOINLINE char *
fast_io_old_new_jeaiii_d20_reference(
	char *destination, ::std::uint_least64_t value) noexcept
{
#if defined(__has_builtin)
#if __has_builtin(__builtin_assume)
	__builtin_assume(value >= UINT64_C(10000000000000000000));
#endif
#endif
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("# LLVM-MCA-BEGIN fast_io_old_new_jeaiii_d20_reference");
#endif
	constexpr ::std::uint_least64_t divisor8{UINT64_C(100000000)};
	constexpr ::std::uint_least64_t divisor16{divisor8 * divisor8};
	auto const high{value / divisor8};
	auto const low{static_cast<::std::uint_least32_t>(value - high * divisor8)};
	auto const high_first{
		static_cast<::std::uint_least32_t>(value / divisor16)};
	auto const high_low{static_cast<::std::uint_least32_t>(
		high - static_cast<::std::uint_least64_t>(high_first) * divisor8)};
	auto *iter{
		::fast_io::details::jeaiii::jeaiii_range4(destination, high_first)};
	iter = ::fast_io::details::jeaiii::jeaiii_f<7>(iter, high_low);
	iter = ::fast_io::details::jeaiii::jeaiii_f<7>(iter, low);
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("# LLVM-MCA-END fast_io_old_new_jeaiii_d20_reference");
#endif
	return iter;
}

int main()
{
	char storage[64u];
	auto *const end{fast_io_old_new_reserve_u64(
		storage, ::std::numeric_limits<::std::uint_least64_t>::max())};
	if (static_cast<::std::size_t>(end - storage) != 20u)
	{
		return 1;
	}
	if (fast_io_old_new_fixed_print_u64(
			storage, ::std::numeric_limits<::std::uint_least64_t>::max()) != 20u)
	{
		return 2;
	}
	char reference[64u];
	auto *const reference_end{fast_io_old_new_jeaiii_d20_reference(
		reference, ::std::numeric_limits<::std::uint_least64_t>::max())};
	if (static_cast<::std::size_t>(reference_end - reference) != 20u)
	{
		return 3;
	}
	for (::std::size_t index{}; index != 20u; ++index)
	{
		if (reference[index] != storage[index])
		{
			return 4;
		}
	}
	return 0;
}
