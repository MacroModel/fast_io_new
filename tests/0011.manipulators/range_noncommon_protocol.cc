#include <cassert>
#include <concepts>
#include <string>
#include <type_traits>

#define FAST_IO_DISABLE_FLOATING_POINT
#include <fast_io.h>

namespace
{

struct pointer_sentinel
{
	int const *last{};

	friend inline constexpr bool operator==(int const *iterator, pointer_sentinel sentinel) noexcept
	{
		return iterator == sentinel.last;
	}

	friend inline constexpr bool operator==(pointer_sentinel sentinel, int const *iterator) noexcept
	{
		return iterator == sentinel;
	}
};

struct noncommon_integer_range
{
	int const *first{};
	int const *last{};

	inline constexpr int const *begin() const noexcept
	{
		return first;
	}

	inline constexpr pointer_sentinel end() const noexcept
	{
		return {last};
	}
};

static_assert(::std::ranges::range<noncommon_integer_range &>);
static_assert(!::std::ranges::common_range<noncommon_integer_range &>);
static_assert(::fast_io::mnp::range_element_print_forwardable<
	char, noncommon_integer_range &>);

using noncommon_view = decltype(::fast_io::mnp::rgvw(
	::std::declval<noncommon_integer_range &>(), ::std::declval<char const (&)[2u]>()));
static_assert(::std::same_as<
	noncommon_view,
	::fast_io::range_view_t<char, int const *, pointer_sentinel>>);
static_assert(::fast_io::printable<char, noncommon_view>);

} // namespace

int main()
{
	int const values[]{1, 22, 333};
	noncommon_integer_range range{values, values + 3u};
	auto view{::fast_io::mnp::rgvw(range, "|")};
	assert(::fast_io::concat_std(view) == "1|22|333");
}
