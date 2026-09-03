#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <span>
#include <string_view>

#include <fast_io_freestanding.h>

namespace
{

#if __has_cpp_attribute(__gnu__::__noinline__)
#define FAST_IO_OLD_NEW_FLOAT_NOINLINE [[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
#define FAST_IO_OLD_NEW_FLOAT_NOINLINE [[msvc::noinline]]
#else
#define FAST_IO_OLD_NEW_FLOAT_NOINLINE
#endif

struct floating_record
{
	char const *first;
	char const *last;
};

template <::std::size_t extent>
[[nodiscard]] constexpr floating_record record(char const (&text)[extent]) noexcept
{
	return {text, text + extent - 1u};
}

/*
The corpus crosses sign, radix-point, exponent, shortest, and long-significand
paths without introducing overflow exceptions into the timed `to` modes.  Its
static storage also proves that destination allocation and source lifetime are
not hidden benchmark variables.
*/
inline constexpr ::std::array short_corpus{
	record("0"),
	record("1"),
	record("2"),
	record("9"),
	record("10"),
	record("42"),
	record("99"),
	record("100"),
	record("-1"),
	record("-9"),
	record("1.5"),
	record("2.5"),
	record("3.25"),
	record("9e1"),
	record("1e-1"),
	record("-2e2")};

inline constexpr ::std::array long_corpus{
	record("0"),
	record("-0"),
	record("1"),
	record("-1"),
	record("1.25"),
	record("-1.25"),
	record("3.141592653589793"),
	record("2.718281828459045"),
	record("6.02214076e23"),
	record("9.1093837139e-31"),
	record("123456789012345.5"),
	record("-0.000000000000000001"),
	record("1e100"),
	record("-1e-100"),
	record("2.2250738585072014e-200"),
	record("1.7976931348623157e200")};

inline ::std::uint_least64_t volatile benchmark_sink{};

[[nodiscard]] ::std::size_t parse_size(char const *text) noexcept
{
	if (text == nullptr || *text == '\0')
	{
		return 0u;
	}
	::std::size_t value{};
	for (; *text != '\0'; ++text)
	{
		unsigned const digit{static_cast<unsigned char>(*text) -
							 static_cast<unsigned>('0')};
		if (9u < digit ||
			value > (::std::numeric_limits<::std::size_t>::max() - digit) / 10u)
		{
			return 0u;
		}
		value = value * 10u + digit;
	}
	// Iteration counts are decimal-only and unsigned so malformed input cannot silently request an unbounded run.
	return value;
}

[[nodiscard]] constexpr ::std::uint_least64_t bits(double value) noexcept
{
	return ::std::bit_cast<::std::uint_least64_t>(value);
}

FAST_IO_OLD_NEW_FLOAT_NOINLINE ::std::uint_least64_t to_once(
	floating_record source)
{
	auto const value{::fast_io::to<double>(
		::fast_io::mnp::strvw(source.first, source.last))};
	return bits(value);
}

FAST_IO_OLD_NEW_FLOAT_NOINLINE ::std::uint_least64_t inplace_to_once(
	floating_record source)
{
	double value{};
	::fast_io::inplace_to(
		value, ::fast_io::mnp::strvw(source.first, source.last));
	return bits(value);
}

FAST_IO_OLD_NEW_FLOAT_NOINLINE ::std::uint_least64_t parse_once(
	floating_record source)
{
	double value{};
	auto const result{
		::fast_io::parse_by_scan(source.first, source.last, value)};
	return bits(value) ^
		   (static_cast<::std::uint_least64_t>(result.iter - source.first)
			<< 48u) ^
		   (static_cast<::std::uint_least64_t>(result.code) << 56u);
}

FAST_IO_OLD_NEW_FLOAT_NOINLINE ::std::uint_least64_t stream_scan_once(
	floating_record source)
{
	double value{};
	::fast_io::basic_ibuffer_view<char> input{source.first, source.last};
	auto input_ref{::fast_io::operations::input_stream_ref(input)};
	auto normalized{::fast_io::io_scan_forward<char>(
		::fast_io::io_scan_alias(value))};
	auto const success{
		::fast_io::operations::decay::scan_freestanding_decay(
			input_ref, normalized)};
	return bits(value) ^
		   (static_cast<::std::uint_least64_t>(
				input.curr_ptr - source.first)
			<< 48u) ^
		   (static_cast<::std::uint_least64_t>(success) << 63u);
}

[[nodiscard]] constexpr ::std::span<floating_record const> select_corpus(
	::std::string_view profile) noexcept
{
	if (profile == "short")
	{
		return short_corpus;
	}
	if (profile == "long")
	{
		return long_corpus;
	}
	return {};
}

[[nodiscard]] bool validate_corpus(
	::std::span<floating_record const> selected_corpus)
{
	for (auto const source : selected_corpus)
	{
		double parsed{};
		auto const result{
			::fast_io::parse_by_scan(source.first, source.last, parsed)};
		if (result.iter != source.last ||
			result.code != ::fast_io::parse_code::ok)
		{
			return false;
		}
		auto const converted{::fast_io::to<double>(
			::fast_io::mnp::strvw(source.first, source.last))};
		if (bits(parsed) != bits(converted))
		{
			return false;
		}
		double inplaced{};
		::fast_io::inplace_to(
			inplaced, ::fast_io::mnp::strvw(source.first, source.last));
		if (bits(parsed) != bits(inplaced))
		{
			return false;
		}
		double streamed{};
		::fast_io::basic_ibuffer_view<char> input{source.first, source.last};
		auto input_ref{::fast_io::operations::input_stream_ref(input)};
		auto normalized{::fast_io::io_scan_forward<char>(
			::fast_io::io_scan_alias(streamed))};
		auto const success{
			::fast_io::operations::decay::scan_freestanding_decay(
				input_ref, normalized)};
		if (!success || input.curr_ptr != source.last ||
			bits(parsed) != bits(streamed))
		{
			return false;
		}
	}
	/*
	All four timed front doors must agree on the complete corpus before the
	clock starts.  This separates protocol correctness (value, status, and
	cursor) from the compact bit checksum used solely to keep timed results live.
	*/
	return true;
}

template <typename operation_type>
[[nodiscard]] double measure(
	operation_type operation,
	::std::span<floating_record const> selected_corpus,
	::std::size_t iterations,
	::std::uint_least64_t &checksum)
{
	checksum = UINT64_C(0xcbf29ce484222325);
	auto const start{::std::chrono::steady_clock::now()};
	for (::std::size_t iteration{}; iteration != iterations; ++iteration)
	{
		auto const result{operation(
			selected_corpus[iteration & (selected_corpus.size() - 1u)])};
		checksum = (checksum ^ result) * UINT64_C(0x100000001b3);
	}
	auto const finish{::std::chrono::steady_clock::now()};
	benchmark_sink = checksum;
	return static_cast<double>(
			   ::std::chrono::duration_cast<::std::chrono::nanoseconds>(
				   finish - start)
				   .count()) /
		   static_cast<double>(iterations);
}

} // namespace

int main(int argc, char **argv)
{
	if (argc != 4)
	{
		::std::fprintf(
			stderr,
			"usage: %s to|inplace-to|parse-by-scan|stream-scan short|long ITERATIONS\n",
			argv[0]);
		return 2;
	}
	::std::string_view const profile{argv[2]};
	auto const selected_corpus{select_corpus(profile)};
	auto const iterations{parse_size(argv[3])};
	if (selected_corpus.empty() || iterations == 0u ||
		!validate_corpus(selected_corpus))
	{
		return 3;
	}

	::std::uint_least64_t checksum{};
	double nanoseconds{};
	::std::string_view const mode{argv[1]};
	if (mode == "to")
	{
		nanoseconds = measure(
			to_once, selected_corpus, iterations, checksum);
	}
	else if (mode == "inplace-to")
	{
		nanoseconds = measure(
			inplace_to_once, selected_corpus, iterations, checksum);
	}
	else if (mode == "parse-by-scan")
	{
		nanoseconds = measure(
			parse_once, selected_corpus, iterations, checksum);
	}
	else if (mode == "stream-scan")
	{
		nanoseconds = measure(
			stream_scan_once, selected_corpus, iterations, checksum);
	}
	else
	{
		return 2;
	}

	::std::printf(
		"mode=%s profile=%s iterations=%zu ns_per_op=%.3f checksum=%llu\n",
		argv[1], argv[2], iterations, nanoseconds,
		static_cast<unsigned long long>(checksum));
}
