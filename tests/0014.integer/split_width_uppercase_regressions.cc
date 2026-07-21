#include <cstddef>
#include <cstdint>

#include <fast_io_core.h>

namespace
{

#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16

template <std::size_t base, bool showbase = false,
		  bool uppercase_showbase = false, bool showpos = false,
		  bool uppercase = false, bool full = false,
		  bool modern_octal = false>
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

template <std::integral char_type>
[[nodiscard]] constexpr char_type semantic_character(char8_t value) noexcept
{
	if (u8'0' <= value && value <= u8'9')
	{
		return ::fast_io::char_literal_add<char_type>(value - u8'0');
	}
	switch (value)
	{
	case u8'+':
		return ::fast_io::char_literal_v<u8'+', char_type>;
	case u8'-':
		return ::fast_io::char_literal_v<u8'-', char_type>;
	case u8'[':
		return ::fast_io::char_literal_v<u8'[', char_type>;
	case u8']':
		return ::fast_io::char_literal_v<u8']', char_type>;
	case u8'b':
		return ::fast_io::char_literal_v<u8'b', char_type>;
	case u8'B':
		return ::fast_io::char_literal_v<u8'B', char_type>;
	case u8'o':
		return ::fast_io::char_literal_v<u8'o', char_type>;
	case u8'O':
		return ::fast_io::char_literal_v<u8'O', char_type>;
	case u8'x':
		return ::fast_io::char_literal_v<u8'x', char_type>;
	case u8'X':
		return ::fast_io::char_literal_v<u8'X', char_type>;
	default:
		if (u8'a' <= value && value <= u8'z')
		{
			switch (value)
			{
			case u8'a':
				return ::fast_io::char_literal_v<u8'a', char_type>;
			case u8'b':
				return ::fast_io::char_literal_v<u8'b', char_type>;
			case u8'c':
				return ::fast_io::char_literal_v<u8'c', char_type>;
			case u8'd':
				return ::fast_io::char_literal_v<u8'd', char_type>;
			case u8'e':
				return ::fast_io::char_literal_v<u8'e', char_type>;
			case u8'f':
				return ::fast_io::char_literal_v<u8'f', char_type>;
			case u8'g':
				return ::fast_io::char_literal_v<u8'g', char_type>;
			case u8'h':
				return ::fast_io::char_literal_v<u8'h', char_type>;
			case u8'i':
				return ::fast_io::char_literal_v<u8'i', char_type>;
			case u8'j':
				return ::fast_io::char_literal_v<u8'j', char_type>;
			case u8'k':
				return ::fast_io::char_literal_v<u8'k', char_type>;
			case u8'l':
				return ::fast_io::char_literal_v<u8'l', char_type>;
			case u8'm':
				return ::fast_io::char_literal_v<u8'm', char_type>;
			case u8'n':
				return ::fast_io::char_literal_v<u8'n', char_type>;
			case u8'o':
				return ::fast_io::char_literal_v<u8'o', char_type>;
			case u8'p':
				return ::fast_io::char_literal_v<u8'p', char_type>;
			case u8'q':
				return ::fast_io::char_literal_v<u8'q', char_type>;
			case u8'r':
				return ::fast_io::char_literal_v<u8'r', char_type>;
			case u8's':
				return ::fast_io::char_literal_v<u8's', char_type>;
			case u8't':
				return ::fast_io::char_literal_v<u8't', char_type>;
			case u8'u':
				return ::fast_io::char_literal_v<u8'u', char_type>;
			case u8'v':
				return ::fast_io::char_literal_v<u8'v', char_type>;
			case u8'w':
				return ::fast_io::char_literal_v<u8'w', char_type>;
			case u8'x':
				return ::fast_io::char_literal_v<u8'x', char_type>;
			case u8'y':
				return ::fast_io::char_literal_v<u8'y', char_type>;
			default:
				return ::fast_io::char_literal_v<u8'z', char_type>;
			}
		}
		switch (value)
		{
		case u8'A':
			return ::fast_io::char_literal_v<u8'A', char_type>;
		case u8'B':
			return ::fast_io::char_literal_v<u8'B', char_type>;
		case u8'C':
			return ::fast_io::char_literal_v<u8'C', char_type>;
		case u8'D':
			return ::fast_io::char_literal_v<u8'D', char_type>;
		case u8'E':
			return ::fast_io::char_literal_v<u8'E', char_type>;
		case u8'F':
			return ::fast_io::char_literal_v<u8'F', char_type>;
		case u8'G':
			return ::fast_io::char_literal_v<u8'G', char_type>;
		case u8'H':
			return ::fast_io::char_literal_v<u8'H', char_type>;
		case u8'I':
			return ::fast_io::char_literal_v<u8'I', char_type>;
		case u8'J':
			return ::fast_io::char_literal_v<u8'J', char_type>;
		case u8'K':
			return ::fast_io::char_literal_v<u8'K', char_type>;
		case u8'L':
			return ::fast_io::char_literal_v<u8'L', char_type>;
		case u8'M':
			return ::fast_io::char_literal_v<u8'M', char_type>;
		case u8'N':
			return ::fast_io::char_literal_v<u8'N', char_type>;
		case u8'O':
			return ::fast_io::char_literal_v<u8'O', char_type>;
		case u8'P':
			return ::fast_io::char_literal_v<u8'P', char_type>;
		case u8'Q':
			return ::fast_io::char_literal_v<u8'Q', char_type>;
		case u8'R':
			return ::fast_io::char_literal_v<u8'R', char_type>;
		case u8'S':
			return ::fast_io::char_literal_v<u8'S', char_type>;
		case u8'T':
			return ::fast_io::char_literal_v<u8'T', char_type>;
		case u8'U':
			return ::fast_io::char_literal_v<u8'U', char_type>;
		case u8'V':
			return ::fast_io::char_literal_v<u8'V', char_type>;
		case u8'W':
			return ::fast_io::char_literal_v<u8'W', char_type>;
		case u8'X':
			return ::fast_io::char_literal_v<u8'X', char_type>;
		case u8'Y':
			return ::fast_io::char_literal_v<u8'Y', char_type>;
		default:
			return ::fast_io::char_literal_v<u8'Z', char_type>;
		}
	}
}

template <typename unsigned_type>
[[nodiscard]] constexpr std::size_t maximum_digits(std::size_t base) noexcept
{
	auto value{static_cast<unsigned_type>(~static_cast<unsigned_type>(0))};
	std::size_t count{1u};
	while (value >= static_cast<unsigned_type>(base))
	{
		value = static_cast<unsigned_type>(value / static_cast<unsigned_type>(base));
		++count;
	}
	return count;
}

template <std::integral char_type>
struct reference_text
{
	char_type storage[256u]{};
	std::size_t size{};
};

template <std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags, typename integer_type>
[[nodiscard]] constexpr reference_text<char_type>
make_reference(integer_type input) noexcept
{
	using unsigned_type = ::fast_io::details::my_make_unsigned_t<integer_type>;
	unsigned_type magnitude{static_cast<unsigned_type>(input)};
	bool negative{};
	if constexpr (::fast_io::details::my_signed_integral<integer_type>)
	{
		negative = input < 0;
		if (negative)
		{
			magnitude = static_cast<unsigned_type>(0) - magnitude;
		}
	}
	reference_text<char_type> result{};
	auto append{[&](char8_t value) constexpr noexcept {
		result.storage[result.size++] = semantic_character<char_type>(value);
	}};
	if (negative)
	{
		append(u8'-');
	}
	else if constexpr (flags.showpos)
	{
		append(u8'+');
	}
	if constexpr (flags.showbase && flags.base != 10u)
	{
		append(u8'0');
		if constexpr (flags.base == 2u)
		{
			append(flags.uppercase_showbase ? u8'B' : u8'b');
		}
		else if constexpr (flags.base == 8u)
		{
			if constexpr (flags.modern_octal)
			{
				append(flags.uppercase_showbase ? u8'O' : u8'o');
			}
		}
		else if constexpr (flags.base == 16u)
		{
			append(flags.uppercase_showbase ? u8'X' : u8'x');
		}
		else
		{
			append(u8'[');
			if constexpr (flags.base >= 10u)
			{
				append(static_cast<char8_t>(u8'0' + flags.base / 10u));
			}
			append(static_cast<char8_t>(u8'0' + flags.base % 10u));
			append(u8']');
		}
	}
	char8_t reverse_digits[160u]{};
	std::size_t digit_count{};
	auto const alphabet{flags.uppercase ? u8"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
										: u8"0123456789abcdefghijklmnopqrstuvwxyz"};
	do
	{
		auto const digit{static_cast<std::size_t>(
			magnitude % static_cast<unsigned_type>(flags.base))};
		reverse_digits[digit_count++] = alphabet[digit];
		magnitude = static_cast<unsigned_type>(
			magnitude / static_cast<unsigned_type>(flags.base));
	} while (magnitude != 0u);
	if constexpr (flags.full)
	{
		auto const target{maximum_digits<unsigned_type>(flags.base)};
		while (digit_count != target)
		{
			reverse_digits[digit_count++] = u8'0';
		}
	}
	while (digit_count != 0u)
	{
		append(reverse_digits[--digit_count]);
	}
	return result;
}

template <std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags, typename integer_type>
[[nodiscard]] constexpr bool check_value(integer_type value) noexcept
{
	using source_type =
		::fast_io::manipulators::scalar_manip_t<flags, integer_type>;
	using proxy_type =
		::fast_io::manipulators::compiler_constant_scalar_manip_t<
			flags, integer_type>;
	using source_tag = ::fast_io::io_reserve_type_t<char_type, source_type>;
	using proxy_tag = ::fast_io::io_reserve_type_t<char_type, proxy_type>;
	constexpr std::size_t capacity{
		::fast_io::print_reserve_size(source_tag{})};
	char_type ordinary[capacity]{};
	char_type proxy[capacity]{};
	auto const ordinary_end{::fast_io::print_reserve_define(
		source_tag{}, ordinary, source_type{value})};
	auto const proxy_end{::fast_io::print_reserve_define(
		proxy_tag{}, proxy, proxy_type{value})};
	auto const ordinary_size{
		static_cast<std::size_t>(ordinary_end - ordinary)};
	auto const proxy_size{static_cast<std::size_t>(proxy_end - proxy)};
	auto const expected{make_reference<char_type, flags>(value)};
	if (ordinary_size != expected.size || proxy_size != expected.size)
	{
		return false;
	}
	for (std::size_t index{}; index != expected.size; ++index)
	{
		if (ordinary[index] != expected.storage[index] ||
			proxy[index] != expected.storage[index])
		{
			return false;
		}
	}
	// GCC 15 does not accept a null comparison on a pointer into an inline
	// variable during constant evaluation.  The contiguous spellings remain
	// compile-time checked above; descriptor ownership is checked by main.
	if (::std::is_constant_evaluated())
	{
		return true;
	}
	constexpr std::size_t fragment_capacity{
		::fast_io::print_compiler_constant_static_fragments_size(proxy_tag{})};
	::fast_io::basic_io_scatter_t<char_type> fragments[fragment_capacity]{};
	auto const fragment_end{
		::fast_io::print_compiler_constant_static_fragments_define(
			proxy_tag{}, fragments, proxy_type{value})};
	std::size_t flattened{};
	for (auto current{fragments}; current != fragment_end; ++current)
	{
		if (current->base == nullptr || current->len == 0u ||
			current->len > expected.size - flattened)
		{
			return false;
		}
		for (std::size_t index{}; index != current->len; ++index)
		{
			if (current->base[index] !=
				expected.storage[flattened + index])
			{
				return false;
			}
		}
		flattened += current->len;
	}
	return flattened == expected.size;
}

template <std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags, typename integer_type,
		  std::size_t extent>
[[nodiscard]] constexpr bool check_table(
	integer_type const (&values)[extent]) noexcept
{
	for (auto value : values)
	{
		if (!check_value<char_type, flags>(value))
		{
			return false;
		}
	}
	return true;
}

template <std::integral char_type>
[[nodiscard]] constexpr bool check_character_domain() noexcept
{
	constexpr __uint128_t unsigned_values[]{
		0u,
		1u,
		(static_cast<__uint128_t>(1u) << 64u) - 1u,
		static_cast<__uint128_t>(1u) << 64u,
		(static_cast<__uint128_t>(1u) << 127u) +
			(static_cast<__uint128_t>(0xabcdefu) << 68u) + 0x123456789abcdefu,
		~static_cast<__uint128_t>(0u)};
	constexpr __int128_t signed_min{
		-static_cast<__int128_t>(
			(static_cast<__uint128_t>(1u) << 127u) - 1u) -
		1};
	constexpr __int128_t signed_values[]{
		signed_min, -static_cast<__int128_t>(1), 0,
		static_cast<__int128_t>(1),
		static_cast<__int128_t>((static_cast<__uint128_t>(1u) << 126u) +
								0xabcdefu)};
	return check_table<char_type, integer_flags<2u, true, true, true>>(
			   unsigned_values) &&
		   check_table<char_type, integer_flags<8u, true>>(
			   unsigned_values) &&
		   check_table<char_type,
					   integer_flags<8u, true, true, false, false, false, true>>(
			   unsigned_values) &&
		   check_table<char_type,
					   integer_flags<16u, true, true, true, true>>(
			   unsigned_values) &&
		   check_table<char_type,
					   integer_flags<16u, true, false, false, false, true>>(
			   unsigned_values) &&
		   check_table<char_type,
					   integer_flags<36u, true, false, true, true>>(
			   unsigned_values) &&
		   check_table<char_type,
					   integer_flags<36u, false, false, false, false, true>>(
			   unsigned_values) &&
		   check_table<char_type,
					   integer_flags<16u, true, true, true, true>>(
			   signed_values) &&
		   check_table<char_type,
					   integer_flags<36u, true, false, true, true>>(
			   signed_values);
}

static_assert(check_character_domain<char>());
static_assert(check_character_domain<wchar_t>());
static_assert(check_character_domain<char8_t>());
static_assert(check_character_domain<char16_t>());
static_assert(check_character_domain<char32_t>());

#endif

} // namespace

int main()
{
#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
	return check_character_domain<char>() &&
				   check_character_domain<wchar_t>() &&
				   check_character_domain<char8_t>() &&
				   check_character_domain<char16_t>() &&
				   check_character_domain<char32_t>()
			   ? 0
			   : 1;
#endif
}
