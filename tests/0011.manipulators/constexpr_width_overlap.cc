#include <fast_io_format.h>

#include <array>
#include <cstddef>

template <typename char_type>
consteval auto render_direct_width_overlap()
{
	::std::array<char_type, 41u> result{};
	::fast_io::basic_obuffer_view<char_type> buffer{result};
	constexpr char_type source[]{
		static_cast<char_type>('a'), static_cast<char_type>('b'),
		static_cast<char_type>('c'), char_type{}};
	auto const separator{
		::fast_io::mnp::chvw(static_cast<char_type>('|'))};
	::fast_io::io::print(
		buffer,
		::fast_io::mnp::right(
			source, 4u, static_cast<char_type>('.')),
		separator, ::fast_io::mnp::right(123, 8u, static_cast<char_type>('.')),
		separator, ::fast_io::mnp::left(123, 6u, static_cast<char_type>('.')),
		separator, ::fast_io::mnp::middle(123, 8u, static_cast<char_type>('.')),
		separator, ::fast_io::mnp::internal(-12, 4u, static_cast<char_type>('0')),
		separator, ::fast_io::mnp::right(-1.25, 6u, static_cast<char_type>('.')));
	return result;
}

template <::fast_io::fmt::basic_fixed_string expected>
consteval bool direct_width_overlap_matches()
{
	using char_type = typename decltype(expected)::value_type;
	constexpr auto value{render_direct_width_overlap<char_type>()};
	if (value.size() != expected.size())
	{
		return false;
	}
	for (::std::size_t index{}; index != value.size(); ++index)
	{
		if (value[index] != expected[index])
		{
			return false;
		}
	}
	return true;
}

static_assert(direct_width_overlap_matches<
			  ".abc|.....123|123...|..123...|-012|.-1.25">());
static_assert(direct_width_overlap_matches<
			  L".abc|.....123|123...|..123...|-012|.-1.25">());
static_assert(direct_width_overlap_matches<
			  u8".abc|.....123|123...|..123...|-012|.-1.25">());
static_assert(direct_width_overlap_matches<
			  u".abc|.....123|123...|..123...|-012|.-1.25">());
static_assert(direct_width_overlap_matches<
			  U".abc|.....123|123...|..123...|-012|.-1.25">());

int main()
{}
