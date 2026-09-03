#include <fast_io.h>

#include <cstdint>

#if __has_cpp_attribute(__gnu__::__noinline__)
#define FAST_IO_PADDING_ABI_NOINLINE [[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
#define FAST_IO_PADDING_ABI_NOINLINE [[msvc::noinline]]
#else
#define FAST_IO_PADDING_ABI_NOINLINE
#endif

#ifndef FAST_IO_PADDING_ABI_PROBE_MASK
#define FAST_IO_PADDING_ABI_PROBE_MASK 15
#endif

#if FAST_IO_PADDING_ABI_PROBE_MASK & 1
extern "C" FAST_IO_PADDING_ABI_NOINLINE
::std::uint_least64_t padding_probe_u64(
	char const *first, char const *last,
	::std::uint_least64_t &value) noexcept
{
	auto manipulator{
		::fast_io::scan_alias_define(::fast_io::io_alias, value)};
	using manipulator_type = decltype(manipulator);
	auto const result{scan_contiguous_define(
		::fast_io::io_reserve_type<char, manipulator_type>, first, last,
		manipulator)};
	return value ^
		   (static_cast<::std::uint_least64_t>(result.iter - first)
			<< 48u) ^
		   (static_cast<::std::uint_least64_t>(result.code) << 56u);
}
#endif

#if FAST_IO_PADDING_ABI_PROBE_MASK & 2
extern "C" FAST_IO_PADDING_ABI_NOINLINE
::std::uint_least64_t padding_probe_f64(
	char const *first, char const *last, double &value) noexcept
{
	auto manipulator{
		::fast_io::scan_alias_define(::fast_io::io_alias, value)};
	using manipulator_type = decltype(manipulator);
	auto const result{scan_contiguous_define(
		::fast_io::io_reserve_type<char, manipulator_type>, first, last,
		manipulator)};
	auto const fields{::fast_io::details::get_punned_result(value)};
	return static_cast<::std::uint_least64_t>(fields.mantissa) ^
		   (static_cast<::std::uint_least64_t>(fields.exponent) << 32u) ^
		   (static_cast<::std::uint_least64_t>(fields.sign) << 63u) ^
		   (static_cast<::std::uint_least64_t>(result.iter - first)
			<< 48u) ^
		   (static_cast<::std::uint_least64_t>(result.code) << 56u);
}
#endif

#if FAST_IO_PADDING_ABI_PROBE_MASK & 4
extern "C" FAST_IO_PADDING_ABI_NOINLINE
::std::uint_least64_t padding_probe_public_u64(
	char const *first, char const *last,
	::std::uint_least64_t &value)
{
	::fast_io::basic_ibuffer_view<char> input{first, last};
	auto const success{::fast_io::io::scan<true>(input, value)};
	return value ^
		   (static_cast<::std::uint_least64_t>(
				input.curr_ptr - first)
			<< 48u) ^
		   (static_cast<::std::uint_least64_t>(success) << 63u);
}
#endif

#if FAST_IO_PADDING_ABI_PROBE_MASK & 8
extern "C" FAST_IO_PADDING_ABI_NOINLINE
::std::uint_least64_t padding_probe_public_f64(
	char const *first, char const *last, double &value)
{
	::fast_io::basic_ibuffer_view<char> input{first, last};
	auto const success{::fast_io::io::scan<true>(input, value)};
	auto const fields{::fast_io::details::get_punned_result(value)};
	return static_cast<::std::uint_least64_t>(fields.mantissa) ^
		   (static_cast<::std::uint_least64_t>(fields.exponent) << 32u) ^
		   (static_cast<::std::uint_least64_t>(fields.sign) << 63u) ^
		   (static_cast<::std::uint_least64_t>(
				input.curr_ptr - first)
			<< 48u) ^
		   (static_cast<::std::uint_least64_t>(success) << 62u);
}
#endif
