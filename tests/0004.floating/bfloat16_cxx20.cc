#include <bit>
#include <cstddef>
#include <cstdint>

#include <fast_io_freestanding.h>

int main()
{
	// GCC exposes the __bf16 type in C++20 before the standard bf16 literal
	// suffix becomes available.  This guard tests precisely that vendor domain;
	// other frontends and targets cannot name the type and therefore have no
	// corresponding compile-time contract to exercise here.
#if defined(__GNUC__) && !defined(__clang__) && defined(__BFLT16_MANT_DIG__)
	static_assert(::fast_io::details::my_floating_point<__bf16>);
	using namespace ::fast_io::mnp;
	__bf16 value{static_cast<__bf16>(0.10009765625f)};
	char buffer[128u];
	auto decimal_manip{fixed<
		::fast_io::manipulators::floating_precision::fractional,
		::fast_io::manipulators::floating_rounding::toward_zero>(value, 1u)};
	auto decimal_end{::fast_io::print_reserve_define(
		::fast_io::io_reserve_type_t<char, decltype(decimal_manip)>{},
		buffer, decimal_manip)};
	if (decimal_end - buffer != 3 || buffer[0] != '0' ||
		buffer[1] != '.' || buffer[2] != '1')
	{
		return 1;
	}

	// Hexadecimal formatting reaches iec559_traits<__bf16>.  A nonempty result
	// proves that the C++20 vendor-type specialization is visible without using
	// the C++23-only literal suffix in the library headers.
	auto hex_manip{hexfloat(value)};
	auto hex_end{::fast_io::print_reserve_define(
		::fast_io::io_reserve_type_t<char, decltype(hex_manip)>{},
		buffer, hex_manip)};
	if (hex_end == buffer)
	{
		return 2;
	}
#endif

	/*
	Clang x86 can lower a by-value __bf16 aggregate copy through
	VCVTNEPS2BF16.  For raw bit pattern one, that instruction may produce zero
	before a callee can recover the IEC 60559 fields.  The precise CPO therefore
	borrows this one compiler/type domain while every other floating type keeps
	the established by-value ABI.  Exercise the scalar boundary, not merely the
	runtime-precision backend: the latter was already field-normalized in the
	original regression and did not expose the entry-copy loss.
	*/
#if defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE) && defined(__clang__) && \
	(defined(__x86_64__) || defined(_M_X64))
	static_assert(sizeof(__bf16) == sizeof(::std::uint_least16_t));
	auto const subnormal{::std::bit_cast<__bf16>(::std::uint_least16_t{1u})};
	constexpr auto scalar_flags{[]() constexpr noexcept {
		auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
		flags.floating = ::fast_io::manipulators::floating_format::decimal;
		return flags;
	}()};
	using scalar_type =
		::fast_io::manipulators::scalar_manip_t<scalar_flags, __bf16>;
	static_assert(::fast_io::precise_reserve_printable<char, scalar_type>);
	scalar_type scalar{subnormal};
	char ordinary[64u];
	char storage[96u];
	for (auto &element : storage)
	{
		element = static_cast<char>(0x5a);
	}
	constexpr ::std::size_t prefix{16u};
	auto *const precise{storage + prefix};
	auto const size{::fast_io::print_reserve_precise_size(
		::fast_io::io_reserve_type_t<char, scalar_type>{}, scalar)};
	auto const ordinary_end{::fast_io::print_reserve_define(
		::fast_io::io_reserve_type_t<char, scalar_type>{}, ordinary, scalar)};
	auto const precise_end{::fast_io::print_reserve_precise_define(
		::fast_io::io_reserve_type_t<char, scalar_type>{}, precise, size, scalar)};
	if (ordinary_end != ordinary + size || precise_end != precise + size ||
		!size || ordinary[0] == '0')
	{
		return 3;
	}
	for (::std::size_t index{}; index != size; ++index)
	{
		if (ordinary[index] != precise[index])
		{
			return 4;
		}
	}
	for (::std::size_t index{}; index != prefix; ++index)
	{
		if (storage[index] != static_cast<char>(0x5a))
		{
			return 5;
		}
	}
	for (auto index{prefix + size}; index != sizeof(storage); ++index)
	{
		if (storage[index] != static_cast<char>(0x5a))
		{
			return 6;
		}
	}
#endif
}
