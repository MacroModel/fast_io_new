#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>

#include <fast_io.h>

namespace
{

template <typename floating_type, bool anchored>
#if __has_cpp_attribute(gnu::noinline)
[[gnu::noinline]]
#endif
::std::uint_least64_t convert_and_hash(floating_type value) noexcept
{
	auto const fields{::fast_io::details::get_punned_result(value)};
	auto const decimal{[](
						   typename ::fast_io::details::iec559_traits<floating_type>::mantissa_type
							   mantissa,
						   ::std::uint_least32_t exponent) noexcept {
		if constexpr (anchored)
		{
			return ::fast_io::details::exact_decimal_from_binary<floating_type>(
				mantissa, exponent);
		}
		else
		{
			return ::fast_io::details::exact_precision_from_binary<floating_type>(
				mantissa, exponent);
		}
	}(fields.mantissa, fields.exponent)};
	::std::uint_least64_t hash{UINT64_C(1469598103934665603)};
	for (::std::size_t index{}; index != decimal.size; ++index)
	{
		hash = (hash ^ decimal.digits[index]) * UINT64_C(1099511628211);
	}
	return hash ^ static_cast<::std::uint_least64_t>(decimal.exponent);
}

template <typename function_type>
[[nodiscard]] ::std::uint_least64_t measure_ns(
	function_type function, ::std::size_t iterations,
	::std::uint_least64_t &hash) noexcept
{
	auto const begin{::std::chrono::steady_clock::now()};
	for (::std::size_t index{}; index != iterations; ++index)
	{
		hash ^= function();
	}
	auto const end{::std::chrono::steady_clock::now()};
	return static_cast<::std::uint_least64_t>(
		::std::chrono::duration_cast<::std::chrono::nanoseconds>(end - begin)
			.count());
}

template <typename floating_type>
void benchmark_type(char const *name, floating_type minimum)
{
	constexpr ::std::size_t iterations{64u};
	::std::uint_least64_t hash{};
	auto const anchored_cold{measure_ns(
		[&] { return convert_and_hash<floating_type, true>(minimum); }, 1u,
		hash)};
	auto const anchored_warm{measure_ns(
		[&] { return convert_and_hash<floating_type, true>(minimum); },
		iterations, hash)};
	auto const original{measure_ns(
		[&] { return convert_and_hash<floating_type, false>(minimum); },
		iterations, hash)};
	::fast_io::io::perr(::fast_io::mnp::os_c_str(name), ",cold_ns=", anchored_cold,
						",anchored_ns=", anchored_warm / iterations,
						",original_ns=", original / iterations, ",hash=", hash, "\n");
}

} // namespace

int main()
{
	if constexpr (::fast_io::details::
					  print_floating_decimal_exact_supported<long double>)
	{
		benchmark_type("binary80",
					   (::std::numeric_limits<long double>::denorm_min)());
	}
#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16 &&     \
	(!defined(__FLT128_MANT_DIG__) || __FLT128_MANT_DIG__ == 113)
	benchmark_type("binary128", ::std::bit_cast<__float128>(
									static_cast<__uint128_t>(1u)));
#endif
}
