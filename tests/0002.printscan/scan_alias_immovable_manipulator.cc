#include <cassert>
#include <concepts>
#include <type_traits>
#include <utility>

#include <fast_io_core.h>

namespace scan_alias_immovable_manipulator
{

struct immovable_manipulator
{
	using manip_tag = ::fast_io::manip_tag_t;

	int value{};

	constexpr immovable_manipulator() noexcept = default;
	immovable_manipulator(immovable_manipulator const &) = delete;
	immovable_manipulator(immovable_manipulator &&) = delete;
	immovable_manipulator &operator=(immovable_manipulator const &) = delete;
	immovable_manipulator &operator=(immovable_manipulator &&) = delete;
};

template <typename T>
concept scan_aliasable = requires(T &&value) {
	::fast_io::io_scan_alias(::std::forward<T>(value));
};

static_assert(::fast_io::manipulator<immovable_manipulator>);
static_assert(scan_aliasable<immovable_manipulator &>);
static_assert(!scan_aliasable<immovable_manipulator>);

} // namespace scan_alias_immovable_manipulator

int main()
{
	using namespace scan_alias_immovable_manipulator;

	immovable_manipulator manipulator;
	manipulator.value = 42;
	auto &&aliased{::fast_io::io_scan_alias(manipulator)};
	static_assert(::std::same_as<decltype(aliased), immovable_manipulator &>);
	assert(__builtin_addressof(aliased) == __builtin_addressof(manipulator));
	assert(aliased.value == 42);
}
