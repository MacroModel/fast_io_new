#include <fast_io_concept.h>

#include <filesystem>
#include <string>
#include <string_view>

namespace test
{

struct const_c_str_source
{
	char const *c_str() const noexcept;
};

struct mutable_only_c_str_source
{
	char const *c_str() noexcept;
};

struct wrong_c_str_element_source
{
	float const *c_str() const noexcept;
};

struct const_view_source
{
	char16_t const *data() const noexcept;
	::std::size_t length() const noexcept;
	const_view_source substr() noexcept;
};

struct mutable_only_view_source
{
	char const *data() noexcept;
	::std::size_t length() noexcept;
	mutable_only_view_source substr() noexcept;
};

struct floating_view_source
{
	double const *data() const noexcept;
	::std::size_t length() const noexcept;
	floating_view_source substr() const noexcept;
};

struct pointer_reference_view_source
{
	char const *const &data() const noexcept;
	::std::size_t length() const noexcept;
	pointer_reference_view_source substr() const noexcept;
};

struct non_size_length
{};

struct non_size_view_source
{
	char const *data() const noexcept;
	non_size_length length() const noexcept;
	non_size_view_source substr() const noexcept;
};

struct const_nullable_source
{
	bool is_nullptr() const noexcept;
};

struct mutable_only_nullable_source
{
	bool is_nullptr() noexcept;
};

static_assert(::fast_io::type_has_c_str_method<const_c_str_source>);
static_assert(::fast_io::constructible_to_os_c_str<const_c_str_source>);
static_assert(::fast_io::constructible_to_os_c_str<const_view_source>);
static_assert(::fast_io::constructible_to_os_c_str<::std::filesystem::path>);
static_assert(::fast_io::constructible_to_os_c_str<::std::string>);
static_assert(::fast_io::constructible_to_os_c_str<::std::u16string>);
static_assert(::fast_io::constructible_to_os_c_str<::std::string_view>);
static_assert(::fast_io::constructible_to_os_c_str<::fast_io::manipulators::basic_os_c_str<char>>);
static_assert(::fast_io::constructible_to_os_c_str<char[4]>);
static_assert(::fast_io::constructible_to_os_c_str<char const[4]>);
static_assert(::fast_io::constructible_to_os_c_str_or_nullptr<const_nullable_source>);

// Each rejected type used to satisfy at least one expression-only branch even though the const consumer expression or
// the encoding converter selected by that branch was ill-formed.
static_assert(!::fast_io::type_has_c_str_method<mutable_only_c_str_source>);
static_assert(!::fast_io::constructible_to_os_c_str<mutable_only_c_str_source>);
static_assert(!::fast_io::constructible_to_os_c_str<wrong_c_str_element_source>);
static_assert(!::fast_io::constructible_to_os_c_str<mutable_only_view_source>);
static_assert(!::fast_io::constructible_to_os_c_str<floating_view_source>);
static_assert(!::fast_io::constructible_to_os_c_str<pointer_reference_view_source>);
static_assert(!::fast_io::constructible_to_os_c_str<non_size_view_source>);
static_assert(!::fast_io::constructible_to_os_c_str<float[4]>);
static_assert(!::fast_io::constructible_to_os_c_str<char[2][2]>);
static_assert(!::fast_io::constructible_to_os_c_str<char volatile[4]>);
static_assert(!::fast_io::constructible_to_os_c_str_or_nullptr<mutable_only_nullable_source>);

} // namespace test

int main()
{}
