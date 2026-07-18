#define FAST_IO_DISABLE_FLOATING_POINT

#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <string_view>

#include <fast_io_dsal/string.h>
#include <fast_io.h>

int main()
{
	// Integer conversion is intentionally common to every destination below. This test exercises only the surrounding
	// semantic graph, destination normalization, and scan publication contracts; it is not an integer-algorithm
	// benchmark. Signed extrema and mixed widths make accidental proxy decay or character-type drift observable.
	::std::array<::std::int64_t, 4u> values{-7, 0, 42, 9223372036854775807LL};
	auto range{::fast_io::mnp::rgvw(values, "|")};
	auto selected{::fast_io::mnp::cond(true, ::std::int64_t{-42}, ::std::int64_t{17})};
	auto record{::fast_io::mnp::pack(
		"id=", ::fast_io::mnp::right(::std::uint32_t{73}, 5u, '0'), ";signed=", selected,
		";values=", range)};
	constexpr ::std::string_view expected{
		"id=00073;signed=-42;values=-7|0|42|9223372036854775807"};

	auto portable{::fast_io::concat_std(record)};
	assert(portable == expected);
	auto native{::fast_io::concat_fast_io(record)};
	assert((::std::string_view{native.data(), native.size()} == expected));

	::std::string printed;
	::fast_io::ostring_ref_std output{__builtin_addressof(printed)};
	::fast_io::print(output, record);
	assert(printed == expected);

	// The public multi-target scan path owns/borrows every normalized target exactly once and publishes each parsed
	// integer to the original object. Whitespace delimiters deliberately keep tokenization independent of formatting.
	::std::string_view input_text{"-7 0 42 4294967295"};
	::fast_io::ibuffer_view input{input_text};
	::std::int32_t first{};
	::std::int64_t second{};
	::std::uint16_t third{};
	::std::uint32_t fourth{};
	assert(::fast_io::io::scan<true>(input, first, second, third, fourth));
	assert(first == -7 && second == 0 && third == 42 && fourth == 4294967295u);
}
