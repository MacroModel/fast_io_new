#include <fast_io.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <vector>

namespace
{

#if __has_cpp_attribute(__gnu__::__noinline__)
#define FAST_IO_PADDING_ATOF_NOINLINE [[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
#define FAST_IO_PADDING_ATOF_NOINLINE [[msvc::noinline]]
#else
#define FAST_IO_PADDING_ATOF_NOINLINE
#endif

template <typename floating_type>
FAST_IO_PADDING_ATOF_NOINLINE
::std::uint_least64_t parse_ordinary(
	char const *first, ::std::size_t semantic_size) noexcept
{
	floating_type value{};
	auto manipulator{
		::fast_io::scan_alias_define(::fast_io::io_alias, value)};
	using manipulator_type = decltype(manipulator);
	auto const result{scan_contiguous_define(
		::fast_io::io_reserve_type<char, manipulator_type>, first,
		first + semantic_size, manipulator)};
	auto const fields{::fast_io::details::get_punned_result(value)};
	return static_cast<::std::uint_least64_t>(fields.mantissa) ^
		   (static_cast<::std::uint_least64_t>(fields.exponent) << 32u) ^
		   (static_cast<::std::uint_least64_t>(fields.sign) << 63u) ^
		   (static_cast<::std::uint_least64_t>(result.iter - first) << 40u) ^
		   (static_cast<::std::uint_least64_t>(result.code) << 56u);
}

template <typename floating_type>
FAST_IO_PADDING_ATOF_NOINLINE
::std::uint_least64_t parse_padded(
	char const *first, ::std::size_t semantic_size) noexcept
{
	floating_type value{};
	auto manipulator{
		::fast_io::scan_alias_define(::fast_io::io_alias, value)};
	using manipulator_type = decltype(manipulator);
	auto const result{scan_contiguous_padding_define(
		::fast_io::io_reserve_type<char, manipulator_type>, first,
		first + semantic_size, 64u, manipulator)};
	auto const fields{::fast_io::details::get_punned_result(value)};
	return static_cast<::std::uint_least64_t>(fields.mantissa) ^
		   (static_cast<::std::uint_least64_t>(fields.exponent) << 32u) ^
		   (static_cast<::std::uint_least64_t>(fields.sign) << 63u) ^
		   (static_cast<::std::uint_least64_t>(result.iter - first) << 40u) ^
		   (static_cast<::std::uint_least64_t>(result.code) << 56u);
}

template <typename floating_type>
FAST_IO_PADDING_ATOF_NOINLINE
::std::uint_least64_t parse_public_ordinary(
	char const *first, ::std::size_t semantic_size)
{
	floating_type value{};
	::fast_io::basic_ibuffer_view<char> input{
		first, first + semantic_size};
	auto const success{::fast_io::io::scan<true>(input, value)};
	auto const fields{::fast_io::details::get_punned_result(value)};
	return static_cast<::std::uint_least64_t>(fields.mantissa) ^
		   (static_cast<::std::uint_least64_t>(fields.exponent) << 32u) ^
		   (static_cast<::std::uint_least64_t>(fields.sign) << 63u) ^
		   (static_cast<::std::uint_least64_t>(
				input.curr_ptr - first)
			<< 40u) ^
		   (static_cast<::std::uint_least64_t>(success) << 62u);
}

template <typename floating_type>
FAST_IO_PADDING_ATOF_NOINLINE
::std::uint_least64_t parse_public_padded(
	char const *first, ::std::size_t semantic_size)
{
	floating_type value{};
	::fast_io::basic_padded_ibuffer_view<char> input{
		first, first + semantic_size, 64u};
	auto const success{::fast_io::io::scan<true>(input, value)};
	auto const fields{::fast_io::details::get_punned_result(value)};
	return static_cast<::std::uint_least64_t>(fields.mantissa) ^
		   (static_cast<::std::uint_least64_t>(fields.exponent) << 32u) ^
		   (static_cast<::std::uint_least64_t>(fields.sign) << 63u) ^
		   (static_cast<::std::uint_least64_t>(
				input.curr_ptr - first)
			<< 40u) ^
		   (static_cast<::std::uint_least64_t>(success) << 62u);
}

inline volatile ::std::uint_least64_t benchmark_sink{};

[[nodiscard]] ::std::size_t parse_size(char const *text) noexcept
{
	char *end{};
	auto const value{
		::std::strtoull(text, __builtin_addressof(end), 0)};
	if (end == text || *end != '\0')
	{
		return 0u;
	}
	return static_cast<::std::size_t>(value);
}

template <typename floating_type, unsigned mode>
[[nodiscard]] double run(
	::std::vector<char> const &storage,
	::std::size_t stride, ::std::size_t semantic_size,
	::std::size_t iterations)
{
	auto accumulator{static_cast<::std::uint_least64_t>(0)};
	auto const start{::std::chrono::steady_clock::now()};
	for (::std::size_t iteration{}; iteration != iterations; ++iteration)
	{
		auto const *const first{
			storage.data() + (iteration & 255u) * stride};
		if constexpr (mode == 0u)
		{
			accumulator ^=
				parse_ordinary<floating_type>(first, semantic_size);
		}
		else if constexpr (mode == 1u)
		{
			accumulator ^=
				parse_padded<floating_type>(first, semantic_size);
		}
		else if constexpr (mode == 2u)
		{
			accumulator ^=
				parse_public_ordinary<floating_type>(
					first, semantic_size);
		}
		else
		{
			accumulator ^=
				parse_public_padded<floating_type>(
					first, semantic_size);
		}
	}
	auto const finish{::std::chrono::steady_clock::now()};
	benchmark_sink = benchmark_sink ^ accumulator;
	return static_cast<double>(
			   ::std::chrono::duration_cast<::std::chrono::nanoseconds>(
				   finish - start)
				   .count()) /
		   static_cast<double>(iterations);
}

} // namespace

int main(int argc, char **argv)
{
	if (argc != 5)
	{
		::std::fprintf(
			stderr,
			"usage: %s ordinary|padded|public-ordinary|public-padded "
			"TYPE LENGTH ITERATIONS\n",
			argv[0]);
		return 2;
	}
	unsigned mode{};
	if (::std::strcmp(argv[1], "ordinary") == 0)
	{
		mode = 0u;
	}
	else if (::std::strcmp(argv[1], "padded") == 0)
	{
		mode = 1u;
	}
	else if (::std::strcmp(argv[1], "public-ordinary") == 0)
	{
		mode = 2u;
	}
	else if (::std::strcmp(argv[1], "public-padded") == 0)
	{
		mode = 3u;
	}
	else
	{
		return 2;
	}
	auto const semantic_size{parse_size(argv[3])};
	auto const iterations{parse_size(argv[4])};
	if (semantic_size == 0u || iterations == 0u)
	{
		return 2;
	}
	auto const stride{(semantic_size + 64u + 63u) & ~::std::size_t{63u}};
	::std::vector<char> storage(256u * stride);
	for (::std::size_t block{}; block != 256u; ++block)
	{
		auto *const first{storage.data() + block * stride};
		first[0] = '1';
		if (semantic_size != 1u)
		{
			first[1] = '.';
		}
		for (::std::size_t index{2u}; index != semantic_size; ++index)
		{
			first[index] = static_cast<char>(
				'0' + (index + block) % 10u);
		}
		for (::std::size_t index{semantic_size}; index != stride; ++index)
		{
			first[index] = '9';
		}
	}

	double nanoseconds{};
	::std::string_view const type{argv[2]};
	auto const run_type = [&]<typename floating_type>() {
		switch (mode)
		{
		case 0u:
			return run<floating_type, 0u>(
				storage, stride, semantic_size, iterations);
		case 1u:
			return run<floating_type, 1u>(
				storage, stride, semantic_size, iterations);
		case 2u:
			return run<floating_type, 2u>(
				storage, stride, semantic_size, iterations);
		default:
			return run<floating_type, 3u>(
				storage, stride, semantic_size, iterations);
		}
	};
#if (defined(__GNUC__) && !defined(__clang__) && \
	 defined(__BFLT16_MANT_DIG__)) ||             \
	defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE)
	if (type == "bf16")
	{
		nanoseconds = run_type.template operator()<__bf16>();
	}
	else
#endif
#if defined(__FLT16_MANT_DIG__) && __FLT16_MANT_DIG__ == 11
	if (type == "f16")
	{
		nanoseconds = run_type.template operator()<_Float16>();
	}
	else
#endif
	if (type == "f32")
	{
		nanoseconds = run_type.template operator()<float>();
	}
	else if (type == "f64")
	{
		nanoseconds = run_type.template operator()<double>();
	}
	else if (type == "f80")
	{
		nanoseconds = run_type.template operator()<long double>();
	}
#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	(!defined(__FLT128_MANT_DIG__) || __FLT128_MANT_DIG__ == 113)
	else if (type == "f128")
	{
		nanoseconds = run_type.template operator()<__float128>();
	}
#endif
	else
	{
		return 2;
	}

	::std::printf(
		"{\"mode\":\"%s\",\"type\":\"%s\",\"semantic_size\":%zu,"
		"\"iterations\":%zu,\"ns\":%.6f,\"sink\":%llu}\n",
		argv[1], argv[2], semantic_size, iterations, nanoseconds,
		static_cast<unsigned long long>(benchmark_sink));
}
