#include <cstddef>
#include <string_view>

#include <fast_io_format.h>

namespace
{

#if !defined(__SIZEOF_INT128__)
static_assert(
	!::fast_io::details::print_floating_decimal_exact_supported<long double>);
#endif

template <::std::size_t extent>
[[nodiscard]] bool equals(
	::fast_io::basic_obuffer_view<char> const &output,
	char const (&expected)[extent]) noexcept
{
	return output.size() == extent - 1u &&
		   ::std::string_view{output.data(), output.size()} ==
			   ::std::string_view{expected, extent - 1u};
}

template <typename flt>
[[nodiscard]] bool check_public_endpoints() noexcept
{
	constexpr flt value{static_cast<flt>(1.25L)};
	constexpr char raw_expected[]{"1.25|1.4p+0"};
	constexpr char format_expected[]{"1.25|0x1.4p+0"};
	char storage[128u]{};

	::fast_io::basic_obuffer_view<char> raw_output{
		storage, storage + sizeof(storage)};
	::fast_io::print(
		raw_output, value, "|", ::fast_io::mnp::hexfloat(value));
	if (!equals(raw_output, raw_expected) ||
		::fast_io::concat_std(
			value, "|", ::fast_io::mnp::hexfloat(value)) != raw_expected)
	{
		return false;
	}

	::fast_io::basic_obuffer_view<char> brace_output{
		storage, storage + sizeof(storage)};
	::fast_io::fmt::print<"{}|{:a}">(
		brace_output, value, value);
	if (!equals(brace_output, format_expected) ||
		::fast_io::fmt::concat_std<"{}|{:a}">(
			value, value) != format_expected)
	{
		return false;
	}

	/*
	The C printf grammar has no binary128 length modifier.  Its `L` conversion
	is deliberately a cast to long double, just as an ellipsis-based C call
	expects.  This endpoint check therefore proves the printf compatibility
	bridge for a value exactly representable in both domains; native binary128
	preservation is covered by the raw and brace calls above.
	*/
	::fast_io::basic_obuffer_view<char> printf_output{
		storage, storage + sizeof(storage)};
	::fast_io::fmt::printf<"%Lg|%La">(
		printf_output, value, value);
	return equals(printf_output, format_expected) &&
		   ::fast_io::fmt::concatf_std<"%Lg|%La">(
			   value, value) == format_expected;
}

template <typename flt>
[[nodiscard]] bool check_binary80_impl() noexcept
{
	if constexpr (::fast_io::details::
					  print_floating_decimal_exact_supported<flt>)
	{
		return check_public_endpoints<flt>();
	}
	return true;
}

[[nodiscard]] bool check_binary80() noexcept
{
	return check_binary80_impl<long double>();
}

[[nodiscard]] bool check_binary128() noexcept
{
#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16 &&     \
	(!defined(__FLT128_MANT_DIG__) || __FLT128_MANT_DIG__ == 113)
	return check_public_endpoints<__float128>();
#else
	return true;
#endif
}

} // namespace

int main()
{
	return !(check_binary80() && check_binary128());
}
