#include <cassert>
#include <cstddef>
#include <string>

#include <fast_io.h>

namespace
{

struct constructed_text
{
	::std::string value;
};

inline constructed_text strlike_construct_define(
	::fast_io::io_strlike_type_t<char, constructed_text>, char const *first, char const *last)
{
	return {::std::string(first, last)};
}

template <::std::size_t extent>
struct fixed_text
{};

template <::std::size_t extent>
inline constexpr ::std::size_t
print_reserve_size(::fast_io::io_reserve_type_t<char, fixed_text<extent>>) noexcept
{
	return extent;
}

template <::std::size_t extent>
inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, fixed_text<extent>>, char *destination, fixed_text<extent>) noexcept
{
	for (::std::size_t index{}; index != extent; ++index)
	{
		destination[index] = static_cast<char>('a' + index % 26u);
	}
	return destination + extent;
}

inline constexpr ::std::size_t inline_character_capacity{
	::fast_io::details::decay::print_stack_buffer_max_element_count<char>()};
inline constexpr ::std::size_t large_text_extent{inline_character_capacity + 1u};
using large_text = fixed_text<large_text_extent>;

static_assert(::fast_io::strlike<char, constructed_text>);
static_assert(!::fast_io::buffer_strlike<char, constructed_text>);
static_assert(inline_character_capacity != SIZE_MAX);
static_assert(::fast_io::reserve_printable<char, large_text>);
static_assert(!::fast_io::details::decay::print_stack_buffer_size_within_limit<
	print_reserve_size(::fast_io::io_reserve_type<char, large_text>), char>);

} // namespace

int main()
{
	// A non-buffer string-like result must first materialize an all-reserve run. The type-level bound is exactly one
	// character beyond this build's shared stack policy, so the test remains a boundary proof when a platform or vendor
	// changes that policy instead of accidentally asserting that one hard-coded capacity is always large.
	auto text{::fast_io::basic_general_concat<false, char, constructed_text>(large_text{})};
	assert(text.value.size() == large_text_extent);
	assert(text.value.front() == 'a');
	assert(text.value.back() == static_cast<char>('a' + (large_text_extent - 1u) % 26u));

	auto line{::fast_io::basic_general_concat<true, char, constructed_text>(large_text{})};
	assert(line.value.size() == large_text_extent + 1u);
	assert(line.value.back() == '\n');
}
