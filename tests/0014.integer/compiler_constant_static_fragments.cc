#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include <fast_io_core.h>

namespace
{

template <::std::size_t base, bool showbase = false,
		  bool uppercase_showbase = false, bool showpos = false,
		  bool uppercase = false, bool full = false, bool modern_octal = false>
inline constexpr auto integer_flags{[]() constexpr noexcept {
	auto flags{::fast_io::manipulators::integral_default_scalar_flags};
	flags.base = base;
	flags.showbase = showbase;
	flags.uppercase_showbase = uppercase_showbase;
	flags.showpos = showpos;
	flags.uppercase = uppercase;
	flags.full = full;
	flags.modern_octal = modern_octal;
	return flags;
}()};

template <bool uppercase>
inline constexpr auto boolalpha_flags{[]() constexpr noexcept {
	auto flags{::fast_io::manipulators::integral_default_scalar_flags};
	flags.alphabet = true;
	flags.uppercase = uppercase;
	return flags;
}()};

template <::std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags, typename value_type>
[[nodiscard]] constexpr bool check_value(value_type value) noexcept
{
	using source_type =
		::fast_io::manipulators::scalar_manip_t<flags, value_type>;
	using proxy_type =
		::fast_io::manipulators::compiler_constant_scalar_manip_t<
			flags, value_type>;
	using source_tag = ::fast_io::io_reserve_type_t<char_type, source_type>;
	using proxy_tag = ::fast_io::io_reserve_type_t<char_type, proxy_type>;
	static_assert(::fast_io::compiler_constant_static_fragment_printable<
				  char_type, proxy_type>);

	constexpr ::std::size_t text_capacity{
		::fast_io::print_reserve_size(source_tag{})};
	constexpr ::std::size_t fragment_capacity{
		::fast_io::print_compiler_constant_static_fragments_size(proxy_tag{})};
	static_assert(text_capacity != 0u);
	static_assert(fragment_capacity != 0u);

	char_type ordinary_text[text_capacity]{};
	char_type proxy_text[text_capacity]{};
	::fast_io::basic_io_scatter_t<char_type>
		fragments[fragment_capacity]{};

	auto const ordinary_end{::fast_io::print_reserve_define(
		source_tag{}, ordinary_text, source_type{value})};
	auto const proxy_end{::fast_io::print_reserve_define(
		proxy_tag{}, proxy_text, proxy_type{value})};
	auto const fragment_end{
		::fast_io::print_compiler_constant_static_fragments_define(
			proxy_tag{}, fragments, proxy_type{value})};

	auto const ordinary_size{
		static_cast<::std::size_t>(ordinary_end - ordinary_text)};
	auto const text_size{
		static_cast<::std::size_t>(proxy_end - proxy_text)};
	if (fragment_end < fragments ||
		static_cast<::std::size_t>(fragment_end - fragments) >
			fragment_capacity)
	{
		return false;
	}
	// GCC packs a one-byte wide execution charset inside native wide string
	// literals.  The historical formatter still uses those literals for a few
	// spellings, whereas both compiler-constant paths intentionally use semantic
	// code units.  Compare against the historical path everywhere its literal
	// representation is native; an EBCDIC configuration compares the two new
	// paths directly.
	if constexpr (!(::std::same_as<char_type, wchar_t> &&
					::fast_io::details::wide_is_none_execution_endian))
	{
		if (ordinary_size != text_size)
		{
			return false;
		}
		for (::std::size_t index{}; index != text_size; ++index)
		{
			if (ordinary_text[index] != proxy_text[index])
			{
				return false;
			}
		}
	}

	::std::size_t flattened_size{};
	for (auto current{fragments}; current != fragment_end; ++current)
	{
		if (current->base == nullptr || current->len == 0u ||
			flattened_size > text_size ||
			text_size - flattened_size < current->len)
		{
			return false;
		}
		for (::std::size_t index{}; index != current->len; ++index)
		{
			if (current->base[index] !=
				proxy_text[flattened_size + index])
			{
				return false;
			}
		}
		flattened_size += current->len;
	}
	if (flattened_size != text_size)
	{
		return false;
	}

	if constexpr (flags.alphabet)
	{
		return fragment_end == fragments + 1 &&
			   fragments[0].len == text_size;
	}
	else
	{
		using integer_type = ::std::conditional_t<
			::std::same_as<::std::remove_cv_t<value_type>, ::std::byte>,
			char8_t, ::std::remove_cv_t<value_type>>;
		bool negative{};
		if constexpr (::fast_io::details::my_signed_integral<integer_type> &&
					  !::std::same_as<integer_type, bool>)
		{
			negative = value < 0;
		}
		auto const sign_fragments{
			static_cast<::std::size_t>(flags.showpos || negative)};
		constexpr ::std::size_t prefix_fragments{
			flags.showbase && flags.base != 10u};
		constexpr ::std::size_t prefix_size{
			prefix_fragments == 0u
				? 0u
				: ::fast_io::details::print_showbase_length<
					  flags.base, flags.modern_octal>};
		if (text_size < sign_fragments + prefix_size)
		{
			return false;
		}
		auto const digit_size{text_size - sign_fragments - prefix_size};
		auto const expected_fragments{
			sign_fragments + prefix_fragments + (digit_size + 1u) / 2u};
		if (static_cast<::std::size_t>(fragment_end - fragments) !=
			expected_fragments)
		{
			return false;
		}

		auto digit_fragment{fragments + sign_fragments + prefix_fragments};
		if ((digit_size & 1u) != 0u)
		{
			if (digit_fragment == fragment_end || digit_fragment->len != 1u)
			{
				return false;
			}
			++digit_fragment;
		}
		for (; digit_fragment != fragment_end; ++digit_fragment)
		{
			if (digit_fragment->len != 2u)
			{
				return false;
			}
		}
		return true;
	}
}

template <::std::integral char_type>
[[nodiscard]] constexpr bool check_character_domain() noexcept
{
	return check_value<char_type, integer_flags<10u>>(0) &&
		   check_value<char_type, integer_flags<10u>>(7) &&
		   check_value<char_type, integer_flags<10u>>(42) &&
		   check_value<char_type, integer_flags<10u>>(12345) &&
		   check_value<char_type, integer_flags<10u>>(-123456) &&
		   check_value<char_type, integer_flags<10u>>(
			   ::std::numeric_limits<::std::int_least64_t>::min()) &&
		   check_value<char_type, integer_flags<10u>>(
			   ::std::numeric_limits<::std::uint_least64_t>::max()) &&
		   check_value<char_type,
					   integer_flags<2u, true, true>>(
			   ::std::numeric_limits<::std::uint_least64_t>::max()) &&
		   check_value<char_type,
					   integer_flags<3u, true, false>>(59048u) &&
		   check_value<char_type,
					   integer_flags<7u, true, false>>(117648u) &&
		   check_value<char_type,
					   integer_flags<8u, true, false, false, false, false, false>>(
			   01234567u) &&
		   check_value<char_type,
					   integer_flags<8u, true, true, false, false, false, true>>(
			   01234567u) &&
		   check_value<char_type,
					   integer_flags<16u, true, true, false, true>>(
			   ::std::numeric_limits<::std::uint_least64_t>::max()) &&
		   check_value<char_type,
					   integer_flags<36u, true, false, false, false>>(
			   ::std::numeric_limits<::std::uint_least64_t>::max()) &&
		   check_value<char_type,
					   integer_flags<36u, true, true, false, true>>(
			   ::std::numeric_limits<::std::uint_least64_t>::max()) &&
		   check_value<char_type,
					   integer_flags<10u, false, false, true>>(12345) &&
		   check_value<char_type,
					   integer_flags<10u, false, false, true>>(-12345) &&
		   check_value<char_type,
					   integer_flags<16u, false, false, false, false, true>>(
			   static_cast<::std::uint_least16_t>(0x2au)) &&
		   check_value<char_type,
					   integer_flags<16u, true, true, true, true, true>>(
			   static_cast<::std::int_least8_t>(-1)) &&
		   check_value<char_type,
					   ::fast_io::details::compute_bool_scalar_flags_cache(
						   ::fast_io::manipulators::integral_default_scalar_flags)>(false) &&
		   check_value<char_type,
					   ::fast_io::details::compute_bool_scalar_flags_cache(
						   ::fast_io::manipulators::integral_default_scalar_flags)>(true) &&
		   check_value<char_type,
					   integer_flags<16u, true, true, true, true, true>>(true) &&
		   check_value<char_type, boolalpha_flags<false>>(false) &&
		   check_value<char_type, boolalpha_flags<false>>(true) &&
		   check_value<char_type, boolalpha_flags<true>>(false) &&
		   check_value<char_type, boolalpha_flags<true>>(true) &&
		   check_value<char_type, integer_flags<16u, true, true>>(
			   static_cast<::std::byte>(0xabu));
}

template <::std::integral char_type>
[[nodiscard]] constexpr bool check_semantic_fragments() noexcept
{
	using hex_proxy =
		::fast_io::manipulators::compiler_constant_scalar_manip_t<
			integer_flags<16u, true, true, false, true>,
			::std::uint_least8_t>;
	using hex_tag = ::fast_io::io_reserve_type_t<char_type, hex_proxy>;
	::fast_io::basic_io_scatter_t<char_type> hex_fragments[2u]{};
	auto const hex_end{
		::fast_io::print_compiler_constant_static_fragments_define(
			hex_tag{}, hex_fragments,
			hex_proxy{static_cast<::std::uint_least8_t>(0xabu)})};
	if (hex_end != hex_fragments + 2u || hex_fragments[0u].len != 2u ||
		hex_fragments[1u].len != 2u ||
		hex_fragments[0u].base[0u] !=
			::fast_io::char_literal_v<u8'0', char_type> ||
		hex_fragments[0u].base[1u] !=
			::fast_io::char_literal_v<u8'X', char_type> ||
		hex_fragments[1u].base[0u] !=
			::fast_io::char_literal_v<u8'A', char_type> ||
		hex_fragments[1u].base[1u] !=
			::fast_io::char_literal_v<u8'B', char_type>)
	{
		return false;
	}

	using base36_proxy =
		::fast_io::manipulators::compiler_constant_scalar_manip_t<
			integer_flags<36u, true>, ::std::uint_least16_t>;
	using base36_tag = ::fast_io::io_reserve_type_t<char_type, base36_proxy>;
	::fast_io::basic_io_scatter_t<char_type> base36_fragments[3u]{};
	auto const base36_end{
		::fast_io::print_compiler_constant_static_fragments_define(
			base36_tag{}, base36_fragments,
			base36_proxy{static_cast<::std::uint_least16_t>(1295u)})};
	if (base36_end != base36_fragments + 2u ||
		base36_fragments[0u].len != 5u ||
		base36_fragments[1u].len != 2u ||
		base36_fragments[0u].base[0u] !=
			::fast_io::char_literal_v<u8'0', char_type> ||
		base36_fragments[0u].base[1u] !=
			::fast_io::char_literal_v<u8'[', char_type> ||
		base36_fragments[0u].base[2u] !=
			::fast_io::char_literal_v<u8'3', char_type> ||
		base36_fragments[0u].base[3u] !=
			::fast_io::char_literal_v<u8'6', char_type> ||
		base36_fragments[0u].base[4u] !=
			::fast_io::char_literal_v<u8']', char_type> ||
		base36_fragments[1u].base[0u] !=
			::fast_io::char_literal_v<u8'z', char_type> ||
		base36_fragments[1u].base[1u] !=
			::fast_io::char_literal_v<u8'z', char_type>)
	{
		return false;
	}

	using alpha_proxy =
		::fast_io::manipulators::compiler_constant_scalar_manip_t<
			boolalpha_flags<true>, bool>;
	using alpha_tag = ::fast_io::io_reserve_type_t<char_type, alpha_proxy>;
	::fast_io::basic_io_scatter_t<char_type> alpha_fragment[1u]{};
	auto const alpha_end{
		::fast_io::print_compiler_constant_static_fragments_define(
			alpha_tag{}, alpha_fragment, alpha_proxy{true})};
	return alpha_end == alpha_fragment + 1u &&
		   alpha_fragment[0u].len == 4u &&
		   alpha_fragment[0u].base[0u] ==
			   ::fast_io::char_literal_v<u8'T', char_type> &&
		   alpha_fragment[0u].base[1u] ==
			   ::fast_io::char_literal_v<u8'R', char_type> &&
		   alpha_fragment[0u].base[2u] ==
			   ::fast_io::char_literal_v<u8'U', char_type> &&
		   alpha_fragment[0u].base[3u] ==
			   ::fast_io::char_literal_v<u8'E', char_type>;
}

template <::std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags, typename value_type>
[[nodiscard]] constexpr bool check_single_static_fragment(
	value_type value, bool expected_available) noexcept
{
	using proxy_type =
		::fast_io::manipulators::compiler_constant_scalar_manip_t<
			flags, value_type>;
	using proxy_tag = ::fast_io::io_reserve_type_t<char_type, proxy_type>;
	static_assert(::fast_io::compiler_constant_precise_compact_preferred<
		char_type, proxy_type>);
	static_assert(::fast_io::compiler_constant_single_static_fragment_printable<
		char_type, proxy_type>);

	proxy_type proxy{value};
	auto const fragment{
		::fast_io::print_compiler_constant_single_static_fragment(
			proxy_tag{}, proxy)};
	if ((fragment.len != 0u) != expected_available)
	{
		return false;
	}
	if (!expected_available)
	{
		return true;
	}
	constexpr ::std::size_t capacity{
		::fast_io::print_reserve_size(proxy_tag{})};
	char_type text[capacity]{};
	auto const end{
		::fast_io::print_reserve_define(proxy_tag{}, text, proxy)};
	auto const size{static_cast<::std::size_t>(end - text)};
	if (fragment.base == nullptr || fragment.len != size)
	{
		return false;
	}
	for (::std::size_t index{}; index != size; ++index)
	{
		if (fragment.base[index] != text[index])
		{
			return false;
		}
	}
	return true;
}

template <::std::integral char_type>
[[nodiscard]] constexpr bool check_single_static_fragments() noexcept
{
	return
		check_single_static_fragment<char_type, integer_flags<10u>>(7, true) &&
		check_single_static_fragment<char_type, integer_flags<10u>>(32, true) &&
		check_single_static_fragment<char_type, integer_flags<10u>>(123, false) &&
		check_single_static_fragment<char_type, integer_flags<10u>>(-7, false) &&
		check_single_static_fragment<char_type,
			integer_flags<10u, false, false, true>>(7, false) &&
		check_single_static_fragment<char_type,
			integer_flags<16u, true>>(0xabu, false) &&
		check_single_static_fragment<char_type,
			integer_flags<16u, false, false, false, true>>(0xabu, true) &&
		check_single_static_fragment<char_type,
			integer_flags<16u, false, false, false, true, true>>(
				static_cast<::std::uint_least8_t>(0xau), true) &&
		check_single_static_fragment<char_type, boolalpha_flags<false>>(
			false, true) &&
		check_single_static_fragment<char_type, boolalpha_flags<true>>(
			true, true);
}

using decimal_u64_proxy =
	::fast_io::manipulators::compiler_constant_scalar_manip_t<
		integer_flags<10u>, ::std::uint_least64_t>;
using binary_u64_proxy =
	::fast_io::manipulators::compiler_constant_scalar_manip_t<
		integer_flags<2u>, ::std::uint_least64_t>;
using decimal_i64_proxy =
	::fast_io::manipulators::compiler_constant_scalar_manip_t<
		integer_flags<10u>, ::std::int_least64_t>;

// The descriptor bound itself is part of the protocol consumed by the print
// dispatcher.  These checks prevent a regression to one descriptor per digit.
static_assert(::fast_io::print_compiler_constant_static_fragments_size(
				  ::fast_io::io_reserve_type<char, decimal_u64_proxy>) == 10u);
static_assert(::fast_io::print_compiler_constant_static_fragments_size(
				  ::fast_io::io_reserve_type<char, binary_u64_proxy>) == 32u);
static_assert(::fast_io::print_compiler_constant_static_fragments_size(
				  ::fast_io::io_reserve_type<char, decimal_i64_proxy>) == 11u);

static_assert(check_character_domain<char>());
static_assert(check_character_domain<wchar_t>());
static_assert(check_character_domain<char8_t>());
static_assert(check_character_domain<char16_t>());
static_assert(check_character_domain<char32_t>());
static_assert(check_semantic_fragments<char>());
static_assert(check_semantic_fragments<wchar_t>());
static_assert(check_semantic_fragments<char8_t>());
static_assert(check_semantic_fragments<char16_t>());
static_assert(check_semantic_fragments<char32_t>());
static_assert(check_single_static_fragments<char>());
static_assert(check_single_static_fragments<wchar_t>());
static_assert(check_single_static_fragments<char8_t>());
static_assert(check_single_static_fragments<char16_t>());
static_assert(check_single_static_fragments<char32_t>());

#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
static_assert(check_value<char, integer_flags<10u>>(
	static_cast<__uint128_t>(~static_cast<__uint128_t>(0))));
static_assert(check_value<char32_t,
						  integer_flags<16u, true, true, false, true>>(
	static_cast<__uint128_t>(~static_cast<__uint128_t>(0))));
#endif

} // namespace

int main()
{}
