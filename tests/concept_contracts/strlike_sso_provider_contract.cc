#include <fast_io_concept.h>

namespace test
{

struct constant_sso_string
{};

struct runtime_sso_string
{};

struct boundary_sso_string
{};

struct unrepresentable_sso_string
{};

template <typename T>
concept test_string = ::std::same_as<T, constant_sso_string> || ::std::same_as<T, runtime_sso_string> ||
					  ::std::same_as<T, boundary_sso_string> ||
					  ::std::same_as<T, unrepresentable_sso_string>;

template <test_string T>
char *strlike_begin(::fast_io::io_strlike_type_t<char, T>, T &) noexcept;

template <test_string T>
char *strlike_curr(::fast_io::io_strlike_type_t<char, T>, T &) noexcept;

template <test_string T>
char *strlike_end(::fast_io::io_strlike_type_t<char, T>, T &) noexcept;

template <test_string T>
void strlike_set_curr(::fast_io::io_strlike_type_t<char, T>, T &, char *) noexcept;

template <test_string T>
void strlike_reserve(::fast_io::io_strlike_type_t<char, T>, T &, ::std::size_t);

inline constexpr ::std::size_t
	strlike_sso_size(::fast_io::io_strlike_type_t<char, constant_sso_string>) noexcept
{
	return 31u;
}

inline ::std::size_t
	strlike_sso_size(::fast_io::io_strlike_type_t<char, runtime_sso_string>) noexcept
{
	return 31u;
}

inline constexpr ::std::size_t
	strlike_sso_size(::fast_io::io_strlike_type_t<char, boundary_sso_string>) noexcept
{
	return static_cast<::std::size_t>(PTRDIFF_MAX);
}

inline constexpr ::std::size_t
	strlike_sso_size(::fast_io::io_strlike_type_t<char, unrepresentable_sso_string>) noexcept
{
	return SIZE_MAX;
}

static_assert(::fast_io::buffer_strlike<char, constant_sso_string>);
static_assert(::fast_io::buffer_strlike<char, runtime_sso_string>);
static_assert(::fast_io::buffer_strlike<char, boundary_sso_string>);
static_assert(::fast_io::buffer_strlike<char, unrepresentable_sso_string>);
static_assert(::fast_io::sso_buffer_strlike<char, constant_sso_string>);
static_assert(::fast_io::sso_buffer_strlike<char, boundary_sso_string>);

// A run-time size has the right return type but cannot initialize the constexpr capacity used by concat consumers.
static_assert(!::fast_io::sso_buffer_strlike<char, runtime_sso_string>);
// A type-level value outside the pointer-difference domain cannot describe one live contiguous initial buffer.
static_assert(!::fast_io::sso_buffer_strlike<char, unrepresentable_sso_string>);

} // namespace test

int main()
{}
