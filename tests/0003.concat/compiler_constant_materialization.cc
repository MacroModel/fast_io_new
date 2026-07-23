#include <fast_io_format.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

namespace
{

using decimal_scalar = decltype(::fast_io::mnp::base<10>(1));
using decimal_materialized =
	::fast_io::details::compiler_constant_materialized_t<char, decimal_scalar>;
using timestamp_materialized =
	::fast_io::details::compiler_constant_materialized_t<
		char, ::fast_io::unix_timestamp>;
using oversized_static_text =
	::fast_io::manipulators::static_scatter_t<char, 257u>;

struct opaque_query_value
{
	unsigned digit;
};

struct opaque_query_proxy
{
	unsigned digit;
};

inline unsigned opaque_query_call_count{};

template <::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type, opaque_query_value>) noexcept
{
	return 1u;
}

template <::std::integral char_type>
inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type, opaque_query_value>,
	char_type *iter, opaque_query_value value) noexcept
{
	*iter++ = ::fast_io::char_literal_add<char_type>(value.digit);
	return iter;
}

template <::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type, opaque_query_proxy>) noexcept
{
	return 1u;
}

template <::std::integral char_type>
inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type, opaque_query_proxy>,
	char_type *iter, opaque_query_proxy value) noexcept
{
	*iter++ = ::fast_io::char_literal_add<char_type>(value.digit);
	return iter;
}

// Deliberately adversarial: this graph-unclassified query is opaque and
// violates the side-effect-free semantic protocol. Consumers must reject the
// provider before the query exists, even though the function lies by returning
// true.
template <::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline bool print_compiler_constant_materialization_eligible(
	::fast_io::io_reserve_type_t<char_type, opaque_query_value>,
	opaque_query_value const &) noexcept
{
	++opaque_query_call_count;
	return true;
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr auto
print_compiler_constant_materialize(
	::fast_io::io_reserve_type_t<char_type, opaque_query_value>,
	opaque_query_value const &value) noexcept
{
	return opaque_query_proxy{value.digit};
}

struct oversized_proxy_source
{
};

struct oversized_proxy
{
	unsigned char state[257u]{};
};

template <::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type, oversized_proxy_source>) noexcept
{
	return 1u;
}

template <::std::integral char_type>
inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type, oversized_proxy_source>,
	char_type *iter, oversized_proxy_source) noexcept
{
	*iter++ = ::fast_io::char_literal_v<u8'x', char_type>;
	return iter;
}

template <::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type, oversized_proxy>) noexcept
{
	return 1u;
}

template <::std::integral char_type>
inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type, oversized_proxy>,
	char_type *iter, oversized_proxy const &) noexcept
{
	*iter++ = ::fast_io::char_literal_v<u8'x', char_type>;
	return iter;
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr bool
print_compiler_constant_materialization_eligible(
	::fast_io::io_reserve_type_t<char_type, oversized_proxy_source>,
	oversized_proxy_source const &) noexcept
{
	return true;
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr auto print_compiler_constant_materialize(
	::fast_io::io_reserve_type_t<char_type, oversized_proxy_source>,
	oversized_proxy_source const &) noexcept
{
	return oversized_proxy{};
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_graph_proven(
	::fast_io::io_reserve_type_t<char_type, oversized_proxy_source>) noexcept
{
	// Provider admission is intentional here: the consumer must reject the
	// 257-byte proxy at its independent aggregate-state bound.
	return {};
}

static_assert(::fast_io::compiler_constant_printable<char, decimal_scalar>);
static_assert(::fast_io::compiler_constant_query_inline_safe<
	char, decimal_scalar>);
static_assert(!::std::same_as<decimal_scalar, decimal_materialized>);
static_assert(::fast_io::reserve_printable<char, decimal_materialized>);
static_assert(::fast_io::compiler_constant_printable<
	char, ::fast_io::unix_timestamp>);
static_assert(!::std::same_as<::fast_io::unix_timestamp, timestamp_materialized>);
static_assert(::fast_io::reserve_printable<char, timestamp_materialized>);
static_assert(
	print_reserve_size(::fast_io::io_reserve_type<char, decimal_materialized>) <=
	::fast_io::details::compiler_constant_materialization_max_bytes);
static_assert(!::fast_io::details::decay::
				   basic_general_concat_compiler_constant_materialization_available<
					   false, char, ::std::string, oversized_static_text,
					   decimal_scalar>());
static_assert(::fast_io::compiler_constant_printable<
	char, opaque_query_value>);
static_assert(!::fast_io::compiler_constant_query_inline_safe<
	char, opaque_query_value>);
static_assert(
	!::fast_io::compiler_constant_materialization_graph_proven_source_shape<
		char, opaque_query_value>);
static_assert(::fast_io::compiler_constant_printable<
	char, oversized_proxy_source>);
static_assert(
	::fast_io::operations::decay::
		print_compiler_constant_materialization_proxy_bytes<
			char, oversized_proxy_source>() == SIZE_MAX);
static_assert(!::fast_io::details::decay::
				   basic_general_concat_compiler_constant_materialization_available<
					   false, char, ::std::string,
					   oversized_proxy_source>());

[[nodiscard]] bool unclassified_query_is_never_called()
{
	opaque_query_call_count = 0u;
	volatile unsigned runtime_digit{7u};
	auto const concat_result{::fast_io::concat_std(opaque_query_value{
		static_cast<unsigned>(runtime_digit)})};
	char storage[8u]{};
	::fast_io::basic_obuffer_view<char> output{storage, storage + 8u};
	::fast_io::print(output, opaque_query_value{
		static_cast<unsigned>(runtime_digit)});
	return opaque_query_call_count == 0u && concat_result == "7" &&
		output.size() == 1u && storage[0] == '7';
}

template <::std::size_t base, bool showbase, bool full, bool modern_octal,
		  auto value>
[[nodiscard]] bool constant_base_matches_runtime()
{
	using value_type = decltype(value);
	volatile value_type runtime_source{value};
	return ::fast_io::concat_std(
			   ::fast_io::mnp::base<base, showbase, full, modern_octal>(value)) ==
		   ::fast_io::concat_std(::fast_io::mnp::base<
			base, showbase, full, modern_octal>(
			static_cast<value_type>(runtime_source)));
}

template <::std::size_t base, bool showbase, bool full, bool modern_octal,
		  auto value>
[[nodiscard]] bool constant_upper_base_matches_runtime()
{
	using value_type = decltype(value);
	volatile value_type runtime_source{value};
	return ::fast_io::concat_std(::fast_io::mnp::baseupper<
			   base, showbase, full, modern_octal>(value)) ==
		   ::fast_io::concat_std(::fast_io::mnp::baseupper<
			base, showbase, full, modern_octal>(
			static_cast<value_type>(runtime_source)));
}

template <typename char_type>
[[nodiscard]] bool direct_format_output_is_correct()
{
	char_type storage[128u]{};
	::fast_io::basic_obuffer_view<char_type> output{
		storage, storage + 128u};
	if constexpr (::std::same_as<char_type, char>)
	{
		::fast_io::fmt::print<"i = {0}|a{2}c{1}">(
			output, 1, "d", "b");
	}
	else if constexpr (::std::same_as<char_type, wchar_t>)
	{
		::fast_io::fmt::wprint<L"i = {0}|a{2}c{1}">(
			output, 1, L"d", L"b");
	}
	else if constexpr (::std::same_as<char_type, char8_t>)
	{
		::fast_io::fmt::u8print<u8"i = {0}|a{2}c{1}">(
			output, 1, u8"d", u8"b");
	}
	else if constexpr (::std::same_as<char_type, char16_t>)
	{
		::fast_io::fmt::u16print<u"i = {0}|a{2}c{1}">(
			output, 1, u"d", u"b");
	}
	else
	{
		::fast_io::fmt::u32print<U"i = {0}|a{2}c{1}">(
			output, 1, U"d", U"b");
	}
	constexpr char expected[]{"i = 1|abcd"};
	if (output.size() != sizeof(expected) - 1u)
	{
		return false;
	}
	for (::std::size_t index{}; index != sizeof(expected) - 1u; ++index)
	{
		if (storage[index] != static_cast<char_type>(expected[index]))
		{
			return false;
		}
	}
	return true;
}

} // namespace

int main()
{
	using namespace ::std::literals;
	char mutable_text[8u]{"xy"};
	volatile int runtime_signed{-42};
	volatile bool runtime_boolean{true};
	volatile ::std::int_least64_t runtime_seconds{1700000000};
	volatile ::std::uint_least64_t runtime_subseconds{};
	volatile ::std::uint_least64_t runtime_fractional_subseconds{
		1230000000000000000ULL};
	return ::fast_io::fmt::concat_std<"{}|{}|{:x}|{:X}|{:b}|{:o}|{}">(
			   ::std::numeric_limits<::std::int64_t>::min(),
			   ::std::numeric_limits<::std::uint64_t>::max(), 0x2au, 0x2au,
			   5u, 9u, true) ==
			   "-9223372036854775808|18446744073709551615|2a|2A|101|11|1"sv &&
		   ::fast_io::concat_std(::fast_io::mnp::base<36>(35u)) == "z"sv &&
		   ::fast_io::concat_std(::fast_io::mnp::hex0x(0x2au)) == "0x2a"sv &&
		   ::fast_io::concat_std(::fast_io::mnp::boolalpha(false)) == "false"sv &&
		   ::fast_io::fmt::concat_std<"{:+}">(-42) ==
			   ::fast_io::fmt::concat_std<"{:+}">(
				   static_cast<int>(runtime_signed)) &&
		   ::fast_io::concat_std(::fast_io::mnp::boolalpha(true)) ==
			   ::fast_io::concat_std(::fast_io::mnp::boolalpha(
				   static_cast<bool>(runtime_boolean))) &&
		   ::fast_io::fmt::concat_std<"t={}">(
			   ::fast_io::unix_timestamp{1700000000, 0}) ==
			   "t=1700000000"sv &&
		   ::fast_io::concat_std(::fast_io::unix_timestamp{1700000000, 0}) ==
			   ::fast_io::concat_std(::fast_io::unix_timestamp{
				   static_cast<::std::int_least64_t>(runtime_seconds),
				   static_cast<::std::uint_least64_t>(runtime_subseconds)}) &&
		   ::fast_io::concat_std(::fast_io::unix_timestamp{
			   -42, 1230000000000000000ULL}) ==
			   ::fast_io::concat_std(::fast_io::unix_timestamp{
				   -42,
				   static_cast<::std::uint_least64_t>(
					   runtime_fractional_subseconds)}) &&
		   constant_base_matches_runtime<10u, false, false, false,
			   ::std::numeric_limits<::std::int64_t>::min()>() &&
		   constant_base_matches_runtime<10u, false, false, false,
			   ::std::numeric_limits<::std::uint64_t>::max()>() &&
		   constant_base_matches_runtime<2u, true, false, false, 5u>() &&
		   constant_base_matches_runtime<8u, true, false, false, 9u>() &&
		   constant_base_matches_runtime<8u, true, false, true, 9u>() &&
		   constant_base_matches_runtime<16u, true, true, false, 0x2au>() &&
		   constant_upper_base_matches_runtime<36u, true, false, false,
			   123456789u>() &&
		   ::fast_io::fmt::concatf_std<"%s">(mutable_text) == "xy"sv &&
		   direct_format_output_is_correct<char>() &&
		   direct_format_output_is_correct<wchar_t>() &&
		   direct_format_output_is_correct<char8_t>() &&
		   direct_format_output_is_correct<char16_t>() &&
		   direct_format_output_is_correct<char32_t>() &&
		   unclassified_query_is_never_called()
			? 0
			: 1;
}
