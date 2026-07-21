#include <fast_io_format.h>

#include <cstddef>

namespace
{

template <typename result_type, typename char_type, ::std::size_t extent>
[[nodiscard]] consteval bool equals_literal(
	result_type const &result, char_type const (&expected)[extent]) noexcept
{
	if (result.size() != extent - 1u)
	{
		return false;
	}
	for (::std::size_t index{}; index != extent - 1u; ++index)
	{
		if (result.data()[index] != expected[index])
		{
			return false;
		}
	}
	return true;
}

/** Proves that every fast_io-owned string family starts its reserved
 *  character lifetimes before concat writes through the exposed capacity.
 */
[[nodiscard]] consteval bool fast_io_destinations_are_constant_evaluated()
{
	return equals_literal(::fast_io::concat_fast_io(2, "|", 3.2), "2|3.2") &&
		   equals_literal(::fast_io::wconcat_fast_io(2, L"|", 3.2), L"2|3.2") &&
		   equals_literal(::fast_io::u8concat_fast_io(2, u8"|", 3.2), u8"2|3.2") &&
		   equals_literal(::fast_io::u16concat_fast_io(2, u"|", 3.2), u"2|3.2") &&
		   equals_literal(::fast_io::u32concat_fast_io(2, U"|", 3.2), U"2|3.2") &&
		   equals_literal(::fast_io::concatln_fast_io(42), "42\n") &&
		   equals_literal(::fast_io::tlc::concat_fast_io_tlc(2, "|", 3.2), "2|3.2") &&
		   equals_literal(::fast_io::tlc::wconcat_fast_io_tlc(2, L"|", 3.2), L"2|3.2") &&
		   equals_literal(::fast_io::tlc::u8concat_fast_io_tlc(2, u8"|", 3.2), u8"2|3.2") &&
		   equals_literal(::fast_io::tlc::u16concat_fast_io_tlc(2, u"|", 3.2), u"2|3.2") &&
		   equals_literal(::fast_io::tlc::u32concat_fast_io_tlc(2, U"|", 3.2), U"2|3.2") &&
		   equals_literal(::fast_io::tlc::concatln_fast_io_tlc(42), "42\n");
}

[[nodiscard]] consteval bool std_destinations_are_constant_evaluated()
{
	return equals_literal(::fast_io::concat_std(2, "|", 3.2), "2|3.2") &&
		   equals_literal(::fast_io::wconcat_std(2, L"|", 3.2), L"2|3.2") &&
		   equals_literal(::fast_io::u8concat_std(2, u8"|", 3.2), u8"2|3.2") &&
		   equals_literal(::fast_io::u16concat_std(2, u"|", 3.2), u"2|3.2") &&
		   equals_literal(::fast_io::u32concat_std(2, U"|", 3.2), U"2|3.2") &&
		   equals_literal(::fast_io::concatln_std(42), "42\n");
}

static_assert(fast_io_destinations_are_constant_evaluated());
static_assert(std_destinations_are_constant_evaluated());

} // namespace

int main()
{}
