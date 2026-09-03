#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

#include <fast_io_dsal/string.h>
#include <fast_io_dsal/impl/misc/push_warnings.h>
#include <fast_io_dsal/impl/misc/push_macros.h>
#include <fast_io_unit/string.h>
#include <fast_io_dsal/impl/misc/pop_macros.h>
#include <fast_io_dsal/impl/misc/pop_warnings.h>

#ifndef FAST_IO_OLD_NEW_NUMERIC_BASE
#define FAST_IO_OLD_NEW_NUMERIC_BASE 10
#endif

#ifndef FAST_IO_OLD_NEW_NUMERIC_PACK
#define FAST_IO_OLD_NEW_NUMERIC_PACK 1
#endif

#ifndef FAST_IO_OLD_NEW_NUMERIC_BITS
#define FAST_IO_OLD_NEW_NUMERIC_BITS 64
#endif

namespace
{

inline constexpr ::std::size_t selected_base{FAST_IO_OLD_NEW_NUMERIC_BASE};
inline constexpr ::std::size_t selected_pack{FAST_IO_OLD_NEW_NUMERIC_PACK};
inline constexpr ::std::size_t selected_bits{FAST_IO_OLD_NEW_NUMERIC_BITS};
inline constexpr ::std::size_t corpus_size{4096u};

static_assert(2u <= selected_base && selected_base <= 36u);
static_assert(selected_pack != 0u && selected_pack <= 32u);
static_assert(
	selected_bits == 8u || selected_bits == 16u || selected_bits == 32u ||
	selected_bits == 64u);

using selected_integer = ::std::conditional_t<
	selected_bits == 8u, ::std::uint_least8_t,
	::std::conditional_t<
		selected_bits == 16u, ::std::uint_least16_t,
		::std::conditional_t<selected_bits == 32u, ::std::uint_least32_t,
							 ::std::uint_least64_t>>>;

/*
An eight-bit unsigned integer is normally a character code unit in the public
print/scan CPOs.  Raw decimal syntax therefore denotes character semantics,
not the numeric grammar represented by this benchmark's radix corpus.  Every
format and scan path shares this discriminator: ordinary wider decimal
carriers retain their default-scalar path, while an eight-bit decimal carrier
uses an explicit base<10>/base_get<10> record just like every non-decimal
radix.  Keeping the decision centralized prevents timed and preflight paths
from silently measuring different operation types.
*/
inline constexpr bool selected_uses_raw_decimal_scalar{
	selected_base == 10u && selected_bits != 8u};

#if __has_cpp_attribute(__gnu__::__noinline__)
#define FAST_IO_OLD_NEW_NOINLINE [[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
#define FAST_IO_OLD_NEW_NOINLINE [[msvc::noinline]]
#else
#define FAST_IO_OLD_NEW_NOINLINE
#endif

struct numeric_record
{
	::std::array<char, 64u> text{};
	::std::size_t size{};
	selected_integer value{};
};

inline ::std::uint_least64_t volatile benchmark_sink{};

[[nodiscard]] constexpr char digit_character(unsigned digit) noexcept
{
	return static_cast<char>(digit < 10u ? '0' + digit : 'a' + (digit - 10u));
}

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
	// Strict decimal parsing rejects signs and base prefixes, preventing `-1` or an octal-looking count from
	// accidentally creating a multi-second run contrary to the harness's per-process timing contract.
	return value;
}

[[nodiscard]] constexpr ::std::uint_least64_t xorshift64(
	::std::uint_least64_t value) noexcept
{
	value ^= value << 7u;
	value ^= value >> 9u;
	value ^= value << 8u;
	return value;
}

template <::std::size_t index = 0u>
[[nodiscard]] constexpr auto numeric_print_arg(
	selected_integer value) noexcept
{
	auto const adjusted{static_cast<selected_integer>(
		static_cast<::std::uint_least64_t>(value) + index)};
	if constexpr (selected_uses_raw_decimal_scalar)
	{
		return adjusted;
	}
	else
	{
		return ::fast_io::mnp::base<selected_base>(adjusted);
	}
}

[[nodiscard]] bool make_exact_width_bounds(
	::std::size_t digits, ::std::uint_least64_t &lower,
	::std::uint_least64_t &upper) noexcept
{
	if (digits == 0u)
	{
		return false;
	}
	using wide_type = unsigned __int128;
	wide_type power{1u};
	for (::std::size_t index{1u}; index != digits; ++index)
	{
		power *= selected_base;
		if (power > ::std::numeric_limits<::std::uint_least64_t>::max())
		{
			return false;
		}
	}
	lower = static_cast<::std::uint_least64_t>(power);
	auto const next{power * selected_base};
	auto const maximum{
		static_cast<wide_type>(::std::numeric_limits<selected_integer>::max())};
	upper = next - 1u > maximum
				? static_cast<::std::uint_least64_t>(maximum)
				: static_cast<::std::uint_least64_t>(next - 1u);
	/*
	Every pack element is `value + index`.  Reserving the largest index here
	proves that all elements remain in the requested digit-width interval and
	that the timed addition cannot wrap.  This keeps pack-size comparisons on an
	equal grammar rather than silently mixing adjacent width kernels.
	*/
	if (upper < selected_pack - 1u)
	{
		return false;
	}
	upper -= selected_pack - 1u;
	return lower <= upper;
}

void format_record(numeric_record &record) noexcept
{
	auto value{record.value};
	char reverse[64u];
	::std::size_t size{};
	do
	{
		auto const digit{static_cast<unsigned>(value % selected_base)};
		reverse[size++] = digit_character(digit);
		value /= selected_base;
	} while (value != 0u);
	record.size = size;
	for (::std::size_t index{}; index != size; ++index)
	{
		record.text[index] = reverse[size - index - 1u];
	}
}

[[nodiscard]] bool build_corpus(
	::std::array<numeric_record, corpus_size> &records,
	::std::size_t digits) noexcept
{
	::std::uint_least64_t lower{};
	::std::uint_least64_t upper{};
	if (!make_exact_width_bounds(digits, lower, upper))
	{
		return false;
	}
	auto random{UINT64_C(0x9e3779b97f4a7c15) ^ digits ^ selected_base};
	auto const span{upper - lower};
	for (auto &record : records)
	{
		random = xorshift64(random);
		record.value = static_cast<selected_integer>(
			lower + (span == ::std::numeric_limits<::std::uint_least64_t>::max()
						 ? random
						 : random % (span + 1u)));
		format_record(record);
		if (record.size != digits)
		{
			return false;
		}
	}
	return true;
}

template <::std::size_t... indices>
inline void print_pack_to(
	::fast_io::basic_obuffer_view<char> &output, selected_integer value,
	::std::index_sequence<indices...>)
{
	::fast_io::operations::print_freestanding<false>(
		output, numeric_print_arg<indices>(value)...);
}

template <::std::size_t... indices>
FAST_IO_OLD_NEW_NOINLINE ::std::uint_least64_t print_once_impl(
	selected_integer value, ::std::index_sequence<indices...>)
{
	char storage[2048u];
	::fast_io::basic_obuffer_view<char> output{storage, storage + sizeof(storage)};
	print_pack_to(output, value, ::std::index_sequence<indices...>{});
#if defined(__GNUC__) || defined(__clang__)
	/*
	The memory operand makes the complete produced record observable without
	adding a checksum loop to the timed work.  The barrier performs no copy and
	does not broaden the library operation being compared.
	*/
	__asm__ __volatile__("" : : "m"(storage) : "memory");
#endif
	auto const size{output.size()};
	return static_cast<unsigned char>(storage[0]) |
		   (static_cast<::std::uint_least64_t>(
				static_cast<unsigned char>(storage[size - 1u]))
			<< 8u) |
		   (static_cast<::std::uint_least64_t>(size) << 16u);
}

FAST_IO_OLD_NEW_NOINLINE ::std::uint_least64_t print_once(
	selected_integer value)
{
	return print_once_impl(value, ::std::make_index_sequence<selected_pack>{});
}

FAST_IO_OLD_NEW_NOINLINE ::std::uint_least64_t reserve_once(
	selected_integer value)
{
	char storage[64u];
	auto parameter{numeric_print_arg(value)};
	auto normalized{::fast_io::io_print_forward<char>(
		::fast_io::io_print_alias(parameter))};
	using normalized_type = ::std::remove_cvref_t<decltype(normalized)>;
	auto const *const end{print_reserve_define(
		::fast_io::io_reserve_type<char, normalized_type>, storage, normalized)};
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("" : : "m"(storage) : "memory");
#endif
	auto const size{static_cast<::std::size_t>(end - storage)};
	return static_cast<unsigned char>(storage[0]) |
		   (static_cast<::std::uint_least64_t>(
				static_cast<unsigned char>(storage[size - 1u]))
			<< 8u) |
		   (static_cast<::std::uint_least64_t>(size) << 16u);
}

template <::std::size_t... indices>
FAST_IO_OLD_NEW_NOINLINE ::std::uint_least64_t concat_once_impl(
	selected_integer value, ::std::index_sequence<indices...>)
{
	auto result{
		::fast_io::concat_fast_io(numeric_print_arg<indices>(value)...)};
#if defined(__GNUC__) || defined(__clang__)
	/*
	The opaque boundary receives both the contiguous base and its live extent.
	Together with the memory clobber, that escape makes the complete result range
	observable and prevents an SSO formatter from retaining only the front/back
	stores subsequently sampled by the compact timing checksum.
	*/
	__asm__ __volatile__("" : : "r"(result.data()), "r"(result.size()) : "memory");
#endif
	auto const size{result.size()};
	return static_cast<unsigned char>(result.front()) |
		   (static_cast<::std::uint_least64_t>(
				static_cast<unsigned char>(result.back()))
			<< 8u) |
		   (static_cast<::std::uint_least64_t>(size) << 16u);
}

FAST_IO_OLD_NEW_NOINLINE ::std::uint_least64_t concat_once(
	selected_integer value)
{
	return concat_once_impl(value, ::std::make_index_sequence<selected_pack>{});
}

template <::std::size_t... indices>
FAST_IO_OLD_NEW_NOINLINE ::std::uint_least64_t concat_std_once_impl(
	selected_integer value, ::std::index_sequence<indices...>)
{
	auto result{::fast_io::concat_std(numeric_print_arg<indices>(value)...)};

#if defined(__GNUC__) || defined(__clang__)
	// Match the fast_io-string observation contract above so both destinations time complete materialization.
	__asm__ __volatile__("" : : "r"(result.data()), "r"(result.size()) : "memory");
#endif
	auto const size{result.size()};
	return static_cast<unsigned char>(result.front()) |
		   (static_cast<::std::uint_least64_t>(
				static_cast<unsigned char>(result.back()))
			<< 8u) |
		   (static_cast<::std::uint_least64_t>(size) << 16u);
}

FAST_IO_OLD_NEW_NOINLINE ::std::uint_least64_t concat_std_once(
	selected_integer value)
{
	return concat_std_once_impl(
		value, ::std::make_index_sequence<selected_pack>{});
}

struct expected_pack_record
{
	::std::array<char, 2048u> text{};
	::std::size_t size{};
};

[[nodiscard]] bool build_expected_pack(
	expected_pack_record &expected, selected_integer value) noexcept
{
	for (::std::size_t index{}; index != selected_pack; ++index)
	{
		numeric_record element{};
		element.value = static_cast<selected_integer>(
			static_cast<::std::uint_least64_t>(value) + index);
		format_record(element);
		if (expected.text.size() - expected.size < element.size)
		{
			return false;
		}
		for (::std::size_t offset{}; offset != element.size; ++offset)
		{
			expected.text[expected.size + offset] = element.text[offset];
		}
		expected.size += element.size;
	}
	return expected.size != 0u;
}

template <bool standard_result, ::std::size_t... indices>
[[nodiscard]] bool validate_concat_result_impl(
	selected_integer value, ::std::index_sequence<indices...>)
{
	auto result = [&] {
		if constexpr (standard_result)
		{
			return ::fast_io::concat_std(numeric_print_arg<indices>(value)...);
		}
		else
		{
			return ::fast_io::concat_fast_io(
				numeric_print_arg<indices>(value)...);
		}
	}();
	expected_pack_record expected{};
	if (!build_expected_pack(expected, value) || result.size() != expected.size)
	{
		return false;
	}
	for (::std::size_t index{}; index != expected.size; ++index)
	{
		if (result.data()[index] != expected.text[index])
		{
			return false;
		}
	}
	return true;
}

template <::std::size_t... indices>
[[nodiscard]] bool validate_print_result_impl(
	selected_integer value, ::std::index_sequence<indices...>)
{
	char storage[2048u];
	::fast_io::basic_obuffer_view<char> output{storage, storage + sizeof(storage)};
	print_pack_to(output, value, ::std::index_sequence<indices...>{});
	expected_pack_record expected{};
	if (!build_expected_pack(expected, value) || output.size() != expected.size)
	{
		return false;
	}
	for (::std::size_t index{}; index != expected.size; ++index)
	{
		if (storage[index] != expected.text[index])
		{
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool validate_print_result(selected_integer value)
{
	/*
	The preflight invokes the same argument factory and pack emitter as the timed
	path, then compares every byte with the independent radix formatter.  The
	compact timed checksum and opaque memory barrier can therefore prevent DCE
	without making a hidden spelling error part of the performance result.
	*/
	return validate_print_result_impl(
		value, ::std::make_index_sequence<selected_pack>{});
}

[[nodiscard]] bool validate_reserve_result(selected_integer value)
{
	char storage[64u];
	auto parameter{numeric_print_arg(value)};
	auto normalized{::fast_io::io_print_forward<char>(
		::fast_io::io_print_alias(parameter))};
	using normalized_type = ::std::remove_cvref_t<decltype(normalized)>;
	auto const *const end{print_reserve_define(
		::fast_io::io_reserve_type<char, normalized_type>, storage, normalized)};
	expected_pack_record expected{};
	return build_expected_pack(expected, value) &&
		   static_cast<::std::size_t>(end - storage) == expected.size &&
		   ::std::string_view{storage, expected.size} ==
			   ::std::string_view{expected.text.data(), expected.size};
}

template <bool standard_result>
[[nodiscard]] bool validate_concat_result(selected_integer value)
{
	/*
	This non-timed preflight compares every emitted character with an independent
	radix formatter.  The timing checksum may remain compact because spelling
	errors in an interior pack element are rejected before measurement begins.
	*/
	return validate_concat_result_impl<standard_result>(
		value, ::std::make_index_sequence<selected_pack>{});
}

FAST_IO_OLD_NEW_NOINLINE ::std::uint_least64_t scan_once(
	numeric_record const &record)
{
	selected_integer value{};
	auto const *const first{record.text.data()};
	auto const *const last{first + record.size};
	auto result = [&] {
		if constexpr (selected_uses_raw_decimal_scalar)
		{
			return ::fast_io::parse_by_scan(first, last, value);
		}
		else
		{
			auto target{::fast_io::mnp::base_get<selected_base>(value)};
			return ::fast_io::parse_by_scan(first, last, target);
		}
	}();
	return static_cast<::std::uint_least64_t>(value) ^
		   (static_cast<::std::uint_least64_t>(result.iter - first) << 48u) ^
		   (static_cast<::std::uint_least64_t>(result.code) << 56u);
}

FAST_IO_OLD_NEW_NOINLINE ::std::uint_least64_t stream_scan_once(
	numeric_record const &record)
{
	selected_integer value{};
	auto const *const first{record.text.data()};
	auto const *const last{first + record.size};
	::fast_io::basic_ibuffer_view<char> input{first, last};
	auto const success = [&] {
		auto input_ref{::fast_io::operations::input_stream_ref(input)};
		if constexpr (selected_uses_raw_decimal_scalar)
		{
			return ::fast_io::operations::decay::scan_freestanding_decay(
				input_ref,
				::fast_io::io_scan_forward<char>(
					::fast_io::io_scan_alias(value)));
		}
		else
		{
			auto target{::fast_io::mnp::base_get<selected_base>(value)};
			return ::fast_io::operations::decay::scan_freestanding_decay(
				input_ref,
				::fast_io::io_scan_forward<char>(
					::fast_io::io_scan_alias(target)));
		}
	}();
	return static_cast<::std::uint_least64_t>(value) ^
		   (static_cast<::std::uint_least64_t>(input.curr_ptr - first) << 48u) ^
		   (static_cast<::std::uint_least64_t>(success) << 63u);
}

FAST_IO_OLD_NEW_NOINLINE ::std::uint_least64_t to_once(
	numeric_record const &record)
{
	return ::fast_io::to<selected_integer>(
		::fast_io::mnp::strvw(record.text.data(),
							  record.text.data() + record.size));
}

FAST_IO_OLD_NEW_NOINLINE ::std::uint_least64_t inplace_to_once(
	numeric_record const &record)
{
	selected_integer value{};
	::fast_io::inplace_to(
		value, ::fast_io::mnp::strvw(record.text.data(),
									 record.text.data() + record.size));
	return static_cast<::std::uint_least64_t>(value);
}

template <typename operation_type>
[[nodiscard]] double measure(
	operation_type operation,
	::std::array<numeric_record, corpus_size> const &records,
	::std::size_t iterations, ::std::uint_least64_t &checksum)
{
	checksum = UINT64_C(0xcbf29ce484222325);
	auto const start{::std::chrono::steady_clock::now()};
	for (::std::size_t iteration{}; iteration != iterations; ++iteration)
	{
		auto const result{operation(records[iteration & (corpus_size - 1u)])};
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

[[nodiscard]] bool validate_parse_by_scan(numeric_record const &record)
{
	selected_integer value{};
	auto const *const first{record.text.data()};
	auto const *const last{first + record.size};
	auto result = [&] {
		if constexpr (selected_uses_raw_decimal_scalar)
		{
			return ::fast_io::parse_by_scan(first, last, value);
		}
		else
		{
			auto target{::fast_io::mnp::base_get<selected_base>(value)};
			return ::fast_io::parse_by_scan(first, last, target);
		}
	}();
	return result.iter == last && result.code == ::fast_io::parse_code::ok &&
		   value == record.value;
}

[[nodiscard]] bool validate_stream_scan(numeric_record const &record)
{
	selected_integer value{};
	auto const *const first{record.text.data()};
	auto const *const last{first + record.size};
	::fast_io::basic_ibuffer_view<char> input{first, last};
	auto input_ref{::fast_io::operations::input_stream_ref(input)};
	auto const success = [&] {
		if constexpr (selected_uses_raw_decimal_scalar)
		{
			return ::fast_io::operations::decay::scan_freestanding_decay(
				input_ref,
				::fast_io::io_scan_forward<char>(
					::fast_io::io_scan_alias(value)));
		}
		else
		{
			auto target{::fast_io::mnp::base_get<selected_base>(value)};
			return ::fast_io::operations::decay::scan_freestanding_decay(
				input_ref,
				::fast_io::io_scan_forward<char>(
					::fast_io::io_scan_alias(target)));
		}
	}();
	return success && input.curr_ptr == last && value == record.value;
}

} // namespace

int main(int argc, char **argv)
{
	if (argc != 4)
	{
		::std::fprintf(
			stderr,
			"usage: %s reserve|print|concat-fast|concat-std|parse-by-scan|stream-scan|to|inplace-to DIGITS ITERATIONS\n",
			argv[0]);
		return 2;
	}
	auto const digits{parse_size(argv[2])};
	auto const iterations{parse_size(argv[3])};
	if (digits == 0u || iterations == 0u)
	{
		return 2;
	}
	::std::array<numeric_record, corpus_size> records{};
	if (!build_corpus(records, digits))
	{
		::std::fprintf(stderr, "digit width is not representable for the selected radix and pack\n");
		return 2;
	}

	::std::string_view const mode{argv[1]};
	::std::uint_least64_t checksum{};
	double nanoseconds{};
	if (mode == "reserve")
	{
		if constexpr (selected_pack != 1u)
		{
			::std::fprintf(stderr, "reserve requires FAST_IO_OLD_NEW_NUMERIC_PACK=1\n");
			return 2;
		}
		for (auto const &record : records)
		{
			if (!validate_reserve_result(record.value))
			{
				return 3;
			}
		}
		nanoseconds = measure(
			[](numeric_record const &record) { return reserve_once(record.value); },
			records, iterations, checksum);
	}
	else if (mode == "print")
	{
		for (auto const &record : records)
		{
			if (!validate_print_result(record.value))
			{
				return 3;
			}
		}
		nanoseconds = measure(
			[](numeric_record const &record) { return print_once(record.value); },
			records, iterations, checksum);
	}
	else if (mode == "concat-fast")
	{
		for (auto const &record : records)
		{
			if (!validate_concat_result<false>(record.value))
			{
				return 3;
			}
		}
		nanoseconds = measure(
			[](numeric_record const &record) { return concat_once(record.value); },
			records, iterations, checksum);
	}
	else if (mode == "concat-std")
	{
		for (auto const &record : records)
		{
			if (!validate_concat_result<true>(record.value))
			{
				return 3;
			}
		}
		nanoseconds = measure(
			[](numeric_record const &record) {
				return concat_std_once(record.value);
			},
			records, iterations, checksum);
	}
	else if (mode == "parse-by-scan" || mode == "stream-scan")
	{
		if constexpr (selected_pack != 1u)
		{
			::std::fprintf(stderr, "scan modes require FAST_IO_OLD_NEW_NUMERIC_PACK=1\n");
			return 2;
		}
		for (auto const &record : records)
		{
			if ((mode == "parse-by-scan" && !validate_parse_by_scan(record)) ||
				(mode == "stream-scan" && !validate_stream_scan(record)))
			{
				return 3;
			}
		}
		nanoseconds = mode == "parse-by-scan"
						  ? measure(scan_once, records, iterations, checksum)
						  : measure(stream_scan_once, records, iterations, checksum);
	}
	else if (mode == "to" || mode == "inplace-to")
	{
		if constexpr (
			selected_base != 10u || selected_pack != 1u || selected_bits == 8u)
		{
			::std::fprintf(
				stderr,
				"to modes require decimal base, pack=1, and a non-code-unit carrier\n");
			return 2;
		}
		for (auto const &record : records)
		{
			auto const preflight{
				mode == "to" ? to_once(record) : inplace_to_once(record)};
			if (preflight != record.value)
			{
				return 3;
			}
		}
		if (mode == "to")
		{
			nanoseconds = measure(to_once, records, iterations, checksum);
		}
		else
		{
			nanoseconds = measure(inplace_to_once, records, iterations, checksum);
		}
	}
	else
	{
		return 2;
	}

	::std::printf(
		"mode=%s bits=%zu base=%zu digits=%zu pack=%zu iterations=%zu ns_per_op=%.3f checksum=%llu\n",
		argv[1], selected_bits, selected_base, digits, selected_pack, iterations,
		nanoseconds, static_cast<unsigned long long>(checksum));
}
