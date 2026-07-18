#include <cassert>
#include <type_traits>
#include <utility>

#define FAST_IO_DISABLE_FLOATING_POINT
#include <fast_io_core.h>

namespace
{

struct stable_proxy
{
	int *target{};

	stable_proxy() = default;
	inline explicit constexpr stable_proxy(int *pointer) noexcept : target(pointer) {}
	stable_proxy(stable_proxy const &) = delete;
	stable_proxy &operator=(stable_proxy const &) = delete;
};

struct stable_source
{
	stable_proxy proxy;
};

inline constexpr stable_proxy &status_io_scan_forward(
	::fast_io::io_alias_type_t<char>, stable_source &source) noexcept
{
	return source.proxy;
}

struct owned_proxy
{
	int *target{};

	owned_proxy() = default;
	inline explicit constexpr owned_proxy(int *pointer) noexcept : target(pointer) {}
	owned_proxy(owned_proxy const &) = delete;
	owned_proxy &operator=(owned_proxy const &) = delete;
	inline constexpr owned_proxy(owned_proxy &&other) noexcept : target(other.target)
	{
		other.target = nullptr;
	}
	owned_proxy &operator=(owned_proxy &&) = delete;
};

struct xvalue_source
{
	owned_proxy proxy;
};

inline constexpr owned_proxy &&status_io_scan_forward(
	::fast_io::io_alias_type_t<char>, xvalue_source &&source) noexcept
{
	return ::std::move(source.proxy);
}

struct immovable_proxy
{
	immovable_proxy() = default;
	immovable_proxy(immovable_proxy const &) = delete;
	immovable_proxy &operator=(immovable_proxy const &) = delete;
	immovable_proxy(immovable_proxy &&) = delete;
	immovable_proxy &operator=(immovable_proxy &&) = delete;
};

struct immovable_source
{
	immovable_proxy proxy;
};

inline constexpr immovable_proxy &&status_io_scan_forward(
	::fast_io::io_alias_type_t<char>, immovable_source &&source) noexcept
{
	return ::std::move(source.proxy);
}

struct incomplete_proxy;
struct incomplete_source
{};

inline incomplete_proxy &&status_io_scan_forward(
	::fast_io::io_alias_type_t<char>, incomplete_source &&) noexcept
{
	__builtin_unreachable();
}

struct void_source
{};

inline constexpr void status_io_scan_forward(
	::fast_io::io_alias_type_t<char>, void_source &&) noexcept
{}

static_assert(::fast_io::status_io_scan_forwardable<char, stable_source &>);
static_assert(::std::same_as<
	decltype(::fast_io::io_scan_forward<char>(::std::declval<stable_source &>())),
	stable_proxy &>);

static_assert(::fast_io::status_io_scan_forwardable<char, xvalue_source>);
static_assert(::std::same_as<
	decltype(::fast_io::io_scan_forward<char>(::std::declval<xvalue_source>())),
	owned_proxy>);

// A non-lvalue status result has to become owned storage. These malformed customizations are concept-negative before
// constructibility traits see an incomplete type or forwarding reaches the deleted move constructor.
static_assert(!::fast_io::status_io_scan_forwardable<char, immovable_source>);
static_assert(!::fast_io::status_io_scan_forwardable<char, incomplete_source>);
static_assert(!::fast_io::status_io_scan_forwardable<char, void_source>);

} // namespace

int main()
{
	int value{7};
	stable_source stable{stable_proxy{__builtin_addressof(value)}};
	auto &borrowed{::fast_io::io_scan_forward<char>(stable)};
	assert(__builtin_addressof(borrowed) == __builtin_addressof(stable.proxy));
	assert(borrowed.target == __builtin_addressof(value));

	xvalue_source source{owned_proxy{__builtin_addressof(value)}};
	auto owned{::fast_io::io_scan_forward<char>(::std::move(source))};
	assert(owned.target == __builtin_addressof(value));
	assert(source.proxy.target == nullptr);
}
