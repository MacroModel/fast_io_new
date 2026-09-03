#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include <fast_io.h>

namespace
{

inline constexpr auto flags{
	::fast_io::manipulators::floating_point_default_scalar_flags};

struct measurement
{
	::std::uint_least64_t nanoseconds{};
	::std::uint_least64_t values{};
	::std::uint_least64_t hash{};
};

template <typename floating_type>
[[nodiscard]] constexpr typename ::fast_io::details::iec559_traits<
	floating_type>::mantissa_type
representative_mantissa(unsigned variant) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr auto mask{
		(static_cast<mantissa_type>(1u) << trait::mbits) - 1u};
	if (!variant)
	{
		return static_cast<mantissa_type>(1u);
	}
	if (variant == 1u)
	{
		return static_cast<mantissa_type>(
			(mask / static_cast<mantissa_type>(3u)) |
			static_cast<mantissa_type>(1u));
	}
	return static_cast<mantissa_type>(mask);
}

template <typename floating_type>
#if __has_cpp_attribute(gnu::noinline)
[[gnu::noinline]]
#endif
[[nodiscard]] ::std::uint_least64_t measure_precise_size_once(
	::std::uint_least32_t exponent, unsigned variant) noexcept
{
	auto const mantissa{
		representative_mantissa<floating_type>(variant)};
	auto const size{
		::fast_io::details::floating_precise_exact_decimal_fields_size<
			flags, floating_type>(mantissa, exponent, false)};
	return static_cast<::std::uint_least64_t>(size) *
			   UINT64_C(0x9e3779b97f4a7c15) ^
		   exponent;
}

template <typename floating_type>
#if __has_cpp_attribute(gnu::noinline)
[[gnu::noinline]]
#endif
[[nodiscard]] ::std::uint_least64_t measure_define_once(
	::std::uint_least32_t exponent, unsigned variant) noexcept
{
	auto const mantissa{
		representative_mantissa<floating_type>(variant)};
	constexpr auto reserve_size{
		::fast_io::details::print_floating_exact_decimal_reserve_size<
			floating_type, flags.floating, flags.json_float>()};
	static char buffer[reserve_size];
	auto *const end{
		::fast_io::details::print_floating_exact_decimal_fields_define<
			flags, floating_type>(buffer, mantissa, exponent, false)};
	auto const size{static_cast<::std::size_t>(end - buffer)};
	auto hash{static_cast<::std::uint_least64_t>(size) *
			  UINT64_C(0x9e3779b97f4a7c15)};
	if (size)
	{
		hash ^= static_cast<unsigned char>(buffer[0u]);
		hash ^= static_cast<::std::uint_least64_t>(
					static_cast<unsigned char>(buffer[size - 1u]))
				<< 8u;
	}
	return hash ^ exponent;
}

template <typename floating_type>
[[nodiscard]] floating_type make_value(
	::std::uint_least32_t exponent, unsigned variant) noexcept
{
	::fast_io::details::punning_result<floating_type> fields{
		representative_mantissa<floating_type>(variant), exponent,
		variant == 2u};
	return ::fast_io::details::
		compiler_constant_floating_value_from_fields<floating_type>(fields);
}

template <typename floating_type>
#if __has_cpp_attribute(gnu::noinline)
[[gnu::noinline]]
#endif
[[nodiscard]] ::std::uint_least64_t measure_range_precise_size_once(
	::std::uint_least32_t exponent, unsigned variant) noexcept
{
	using namespace ::fast_io::mnp;
	auto const value{precision_range(
		decimal(make_value<floating_type>(exponent, variant)), 1u, 17u)};
	using value_type = ::std::remove_cvref_t<decltype(value)>;
	using tag = ::fast_io::io_reserve_type_t<char, value_type>;
	auto const size{print_reserve_precise_size(tag{}, value)};
	return static_cast<::std::uint_least64_t>(size) *
			   UINT64_C(0x9e3779b97f4a7c15) ^
		   exponent;
}

template <typename floating_type>
#if __has_cpp_attribute(gnu::noinline)
[[gnu::noinline]]
#endif
[[nodiscard]] ::std::uint_least64_t measure_range_define_once(
	::std::uint_least32_t exponent, unsigned variant) noexcept
{
	using namespace ::fast_io::mnp;
	auto const value{precision_range(
		decimal(make_value<floating_type>(exponent, variant)), 1u, 17u)};
	using value_type = ::std::remove_cvref_t<decltype(value)>;
	using tag = ::fast_io::io_reserve_type_t<char, value_type>;
	static char buffer[20000u];
	auto *const end{print_reserve_define(tag{}, buffer, value)};
	auto const size{static_cast<::std::size_t>(end - buffer)};
	auto hash{static_cast<::std::uint_least64_t>(size) *
			  UINT64_C(0x9e3779b97f4a7c15)};
	if (size)
	{
		hash ^= static_cast<unsigned char>(buffer[0u]);
		hash ^= static_cast<::std::uint_least64_t>(
					static_cast<unsigned char>(buffer[size - 1u]))
				<< 8u;
	}
	return hash ^ exponent;
}

template <typename floating_type, typename function_type>
[[nodiscard]] measurement measure_all_exponents(
	function_type function) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	constexpr auto exponent_mask{
		(static_cast<::std::uint_least32_t>(1u) << trait::ebits) - 1u};
	constexpr auto values_per_pass{
		static_cast<::std::size_t>(exponent_mask) * 3u};
	constexpr ::std::size_t target_values{262144u};
	constexpr ::std::size_t repetitions{
		values_per_pass < target_values
			? (target_values + values_per_pass - 1u) / values_per_pass
			: 1u};
	measurement result{};
	auto const begin{::std::chrono::steady_clock::now()};
	for (::std::size_t repetition{}; repetition != repetitions; ++repetition)
	{
		for (::std::uint_least32_t exponent{}; exponent != exponent_mask;
			 ++exponent)
		{
			for (unsigned variant{}; variant != 3u; ++variant)
			{
				result.hash ^= function(exponent, variant);
				++result.values;
			}
		}
	}
	auto const end{::std::chrono::steady_clock::now()};
	result.nanoseconds = static_cast<::std::uint_least64_t>(
		::std::chrono::duration_cast<::std::chrono::nanoseconds>(end - begin)
			.count());
	return result;
}

template <typename floating_type>
void benchmark_type(char const *name)
{
	auto const size{measure_all_exponents<floating_type>(
		measure_precise_size_once<floating_type>)};
	auto const define{measure_all_exponents<floating_type>(
		measure_define_once<floating_type>)};
	auto const range_size{measure_all_exponents<floating_type>(
		measure_range_precise_size_once<floating_type>)};
	auto const range_define{measure_all_exponents<floating_type>(
		measure_range_define_once<floating_type>)};
	::fast_io::io::perr(
		::fast_io::mnp::os_c_str(name), ",values=", size.values,
		",precise_size_ns=", size.nanoseconds / size.values,
		",define_ns=", define.nanoseconds / define.values,
		",range_size_ns=", range_size.nanoseconds / range_size.values,
		",range_define_ns=", range_define.nanoseconds / range_define.values,
		",size_hash=", size.hash, ",define_hash=", define.hash, "\n");
}

[[nodiscard]] bool selected(char const *argument, char const *name) noexcept
{
	return ::std::string_view{argument} == ::std::string_view{name};
}

} // namespace

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		::fast_io::io::perr("usage: exact_decimal_precise_size_bench ",
							"bf16|f16|f32|f64|f80|f128\n");
		return 1;
	}
#if defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE)
	if (selected(argv[1], "bf16"))
	{
		benchmark_type<__bf16>("bf16");
		return 0;
	}
#endif
#if defined(__FLT16_MANT_DIG__) && __FLT16_MANT_DIG__ == 11
	if (selected(argv[1], "f16"))
	{
		benchmark_type<_Float16>("f16");
		return 0;
	}
#endif
	if (selected(argv[1], "f32"))
	{
		benchmark_type<float>("f32");
		return 0;
	}
	if (selected(argv[1], "f64"))
	{
		benchmark_type<double>("f64");
		return 0;
	}
#if defined(__LDBL_MANT_DIG__) && __LDBL_MANT_DIG__ == 64 && \
	defined(__LDBL_MAX_EXP__) && __LDBL_MAX_EXP__ == 16384
	if (selected(argv[1], "f80"))
	{
		benchmark_type<long double>("f80");
		return 0;
	}
#endif
#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
	if (selected(argv[1], "f128"))
	{
		benchmark_type<__float128>("f128");
		return 0;
	}
#endif
	::fast_io::io::perr("unsupported format: ",
						::fast_io::mnp::os_c_str(argv[1]), "\n");
	return 2;
}
