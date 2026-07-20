#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <fast_io_freestanding.h>

namespace
{

#if defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE) && defined(__clang__) && \
	(defined(__x86_64__) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))

using rounding = ::fast_io::manipulators::floating_rounding;
using precision_mode = ::fast_io::manipulators::floating_precision;

template <rounding rounding_value, precision_mode precision_value,
	bool decorated>
inline constexpr auto hex_flags = []() constexpr noexcept {
	auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.floating = ::fast_io::manipulators::floating_format::hexfloat;
	flags.rounding = rounding_value;
	flags.precision = precision_value;
	flags.showbase = decorated;
	flags.showpos = decorated;
	flags.uppercase_showbase = decorated;
	flags.uppercase = decorated;
	flags.uppercase_e = decorated;
	flags.comma = decorated;
	flags.nan_show_type = decorated;
	return flags;
}();

inline constexpr auto probe_flags{
	hex_flags<rounding::nearest_to_even, precision_mode::fractional, true>};
using probe_manipulator =
	::fast_io::manipulators::scalar_manip_precision_t<probe_flags, __bf16>;
using probe_tag = ::fast_io::io_reserve_type_t<char, probe_manipulator>;

static_assert(
	::fast_io::details::print_floating_requires_object_field_capture<__bf16>);
#if defined(__AVX512BF16__)
static_assert(
	!::fast_io::details::print_floating_decimal_requires_integer_transport<
		__bf16>);
#else
static_assert(
	::fast_io::details::print_floating_decimal_requires_integer_transport<
		__bf16>);
#endif

/*
These symbols are ABI probes as well as test helpers.  A default Clang x86
build gives the probe a const-reference parameter.  A `-mavx512bf16` build
gives the same externally visible probe a by-value owning manipulator; the
separate pointer caller must load its raw word into XMM, and the probe must
extract it from that register-backed local object before any nested scalar
copy.  The following commands exercise and inspect the second target on a
machine without AVX512-BF16:

  clang++ -std=c++20 -O3 -mavx512bf16 -Iinclude \
    tests/0004.floating/bfloat16_hex_raw_fields.cc -o /tmp/bf16-fields
  sde64 -spr -- /tmp/bf16-fields
  objdump -drwC -Mintel /tmp/bf16-fields

The caller/probe pair should contain a raw-word `vpinsrw`/`vpextrw` transport
and no `__truncsfbf2` call.
*/
extern "C"
#if __has_cpp_attribute(gnu::noinline)
[[gnu::noinline]]
#endif
char *fast_io_bfloat16_hex_by_value_probe(
	char *iter,
#if defined(__AVX512BF16__)
	probe_manipulator value
#else
	probe_manipulator const &value
#endif
	) noexcept
{
	return ::fast_io::print_reserve_define(probe_tag{}, iter, value);
}

extern "C"
#if __has_cpp_attribute(gnu::noinline)
[[gnu::noinline]]
#endif
char *fast_io_bfloat16_hex_probe_caller(
	char *iter, probe_manipulator const *value) noexcept
{
	return fast_io_bfloat16_hex_by_value_probe(iter, *value);
}

[[nodiscard]] inline constexpr auto fields_from_raw(
	::std::uint_least16_t raw) noexcept
{
	return ::fast_io::details::punning_result<__bf16>{
		static_cast<::std::uint_least16_t>(raw & 0x007fu),
		static_cast<::std::uint_least32_t>((raw >> 7u) & 0x00ffu),
		static_cast<bool>(raw >> 15u)};
}

template <typename manipulator>
inline void store_raw(manipulator &manip,
	::std::uint_least16_t raw) noexcept
{
	static_assert(sizeof(manip.reference) == sizeof(raw));
	::std::memcpy(&manip.reference, &raw, sizeof(raw));
}

template <typename char_type>
bool equal(char_type const *left, char_type const *left_end,
	char_type const *right, char_type const *right_end) noexcept
{
	if (left_end - left != right_end - right)
	{
		return false;
	}
	for (; left != left_end; ++left, ++right)
	{
		if (*left != *right)
		{
			return false;
		}
	}
	return true;
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags>
bool check_scalar(::std::uint_least16_t raw) noexcept
{
	using manipulator =
		::fast_io::manipulators::scalar_manip_t<flags, __bf16>;
	using tag = ::fast_io::io_reserve_type_t<char_type, manipulator>;
	manipulator manip{};
	store_raw(manip, raw);
	char_type actual[256u]{};
	char_type oracle[256u]{};
	auto *const actual_end{
		::fast_io::print_reserve_define(tag{}, actual, manip)};
	auto *const oracle_end{
		::fast_io::details::compiler_constant_hex_scalar_fields_define<flags>(
			oracle, fields_from_raw(raw))};
	if (!equal(actual, actual_end, oracle, oracle_end))
	{
		return false;
	}
	auto const precise_size{
		::fast_io::print_reserve_precise_size(tag{}, manip)};
	char_type precise[256u]{};
	auto *const precise_end{::fast_io::print_reserve_precise_define(
		tag{}, precise, precise_size, manip)};
	return precise_size == static_cast<::std::size_t>(oracle_end - oracle) &&
		equal(precise, precise_end, oracle, oracle_end);
}

template <::std::integral char_type, rounding rounding_value,
	precision_mode precision_value,
	bool decorated>
bool check_precision(::std::uint_least16_t raw,
	::std::size_t precision) noexcept
{
	constexpr auto flags{hex_flags<rounding_value, precision_value, decorated>};
	using manipulator =
		::fast_io::manipulators::scalar_manip_precision_t<flags, __bf16>;
	using tag = ::fast_io::io_reserve_type_t<char_type, manipulator>;
	manipulator manip{};
	store_raw(manip, raw);
	manip.precision = precision;
	char_type actual[512u]{};
	char_type oracle[512u]{};
	auto *const actual_end{
		::fast_io::print_reserve_define(tag{}, actual, manip)};
	auto *const oracle_end{::fast_io::details::
		compiler_constant_hex_precision_fields_runtime_define<flags>(
			oracle, fields_from_raw(raw), precision)};
	if (!equal(actual, actual_end, oracle, oracle_end))
	{
		return false;
	}
	auto const precise_size{
		::fast_io::print_reserve_precise_size(tag{}, manip)};
	char_type precise[512u]{};
	auto *const precise_end{::fast_io::print_reserve_precise_define(
		tag{}, precise, precise_size, manip)};
	return precise_size == static_cast<::std::size_t>(oracle_end - oracle) &&
		equal(precise, precise_end, oracle, oracle_end);
}

template <::std::integral char_type, rounding rounding_value,
	precision_mode precision_value>
bool check_decorations(::std::uint_least16_t raw,
	::std::size_t precision) noexcept
{
	return check_precision<char_type, rounding_value, precision_value, false>(
			raw, precision) &&
		check_precision<char_type, rounding_value, precision_value, true>(
			raw, precision);
}

template <::std::integral char_type, precision_mode precision_value>
bool check_roundings(::std::uint_least16_t raw,
	::std::size_t precision) noexcept
{
	return
		check_decorations<char_type, rounding::nearest_to_even, precision_value>(raw, precision) &&
		check_decorations<char_type, rounding::nearest_to_odd, precision_value>(raw, precision) &&
		check_decorations<char_type, rounding::nearest_toward_plus_infinity, precision_value>(raw, precision) &&
		check_decorations<char_type, rounding::nearest_toward_minus_infinity, precision_value>(raw, precision) &&
		check_decorations<char_type, rounding::nearest_toward_zero, precision_value>(raw, precision) &&
		check_decorations<char_type, rounding::nearest_away_from_zero, precision_value>(raw, precision) &&
		check_decorations<char_type, rounding::toward_plus_infinity, precision_value>(raw, precision) &&
		check_decorations<char_type, rounding::toward_minus_infinity, precision_value>(raw, precision) &&
		check_decorations<char_type, rounding::toward_zero, precision_value>(raw, precision) &&
		check_decorations<char_type, rounding::away_from_zero, precision_value>(raw, precision);
}

template <::std::integral char_type>
bool check_key_matrix(::std::uint_least16_t raw) noexcept
{
	constexpr ::std::size_t precisions[]{0u, 1u, 3u, 7u, 40u, 128u};
	for (auto const precision : precisions)
	{
		if (!check_roundings<char_type, precision_mode::significant>(raw, precision) ||
			!check_roundings<char_type, precision_mode::fractional>(raw, precision) ||
			!check_roundings<char_type,
				precision_mode::significant_preserve_trailing_zero>(raw, precision) ||
			!check_roundings<char_type,
				precision_mode::fractional_preserve_trailing_zero>(raw, precision))
		{
			return false;
		}
	}
	return true;
}

bool run_bfloat16_raw_fields_test() noexcept
{
	constexpr auto scalar_flags{
		hex_flags<rounding::nearest_to_even, precision_mode::significant, true>};
	for (::std::uint_least32_t raw{}; raw != 0x10000u; ++raw)
	{
		auto const bits{static_cast<::std::uint_least16_t>(raw)};
		if (!check_scalar<char, scalar_flags>(bits) ||
			!check_precision<char, rounding::nearest_to_even,
				precision_mode::significant, true>(bits, 3u) ||
			!check_precision<char, rounding::toward_minus_infinity,
				precision_mode::fractional_preserve_trailing_zero, true>(
					bits, 7u))
		{
			::std::fprintf(stderr, "bf16 raw mismatch: 0x%04x\n",
				static_cast<unsigned>(bits));
			return false;
		}
	}
	constexpr ::std::uint_least16_t key_bits[]{
		0x0000u, 0x8000u, 0x0001u, 0x007fu, 0x0080u, 0x3f7fu,
		0x3f80u, 0x7f7fu, 0xff7fu, 0x7f80u, 0xff80u, 0x7f81u,
		0x7fbfu, 0x7fc0u, 0x7fffu, 0xff81u, 0xffc0u, 0xffffu};
	for (auto const bits : key_bits)
	{
		if (!check_key_matrix<char>(bits) ||
			!check_key_matrix<wchar_t>(bits) ||
			!check_key_matrix<char8_t>(bits) ||
			!check_key_matrix<char16_t>(bits) ||
			!check_key_matrix<char32_t>(bits))
		{
			::std::fprintf(stderr, "bf16 key mismatch: 0x%04x\n",
				static_cast<unsigned>(bits));
			return false;
		}
	}
	return true;
}

#endif

} // namespace

int main()
{
#if defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE) && defined(__clang__) && \
	(defined(__x86_64__) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	return !run_bfloat16_raw_fields_test();
#else
	return 0;
#endif
}
