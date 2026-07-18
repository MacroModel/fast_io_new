#include <string>
#include <utility>

#include <fast_io.h>

namespace
{

struct non_strlike_target
{};

struct shaped_but_invalid_ref
{
	using value_type = non_strlike_target;
	using char_type = char;
	non_strlike_target *ptr{};
};

struct invalid_source
{
	non_strlike_target target;
};

inline constexpr shaped_but_invalid_ref io_strlike_ref(
	::fast_io::io_alias_t, invalid_source &source) noexcept
{
	return {__builtin_addressof(source.target)};
}

using invalid_token = decltype(
	::fast_io::manipulators::strlike_get(::std::declval<invalid_source &>()));
using forged_fixed_count = ::fast_io::manipulators::basic_str_get_all<
	::fast_io::io_strlike_reference_wrapper<char, non_strlike_target>>;

// A custom reference can reproduce the historical member names without supplying any target construction protocol.
// Scan admission must reject both the public token and a manually forged fixed-count wrapper before a CPO body is used.
static_assert(!::fast_io::context_scannable<char, invalid_token>);
static_assert(!::fast_io::context_scannable<char, forged_fixed_count>);

using string_token = decltype(
	::fast_io::manipulators::strlike_get(::std::declval<::std::string &>()));
using string_line_token = decltype(
	::fast_io::manipulators::strlike_line_get(::std::declval<::std::string &>()));
using string_whole_token = decltype(
	::fast_io::manipulators::strlike_whole_get(::std::declval<::std::string &>()));
using string_fixed_count = decltype(
	::fast_io::manipulators::str_get_all(::std::declval<::std::string &>(), 3u));
using wide_string_token = decltype(
	::fast_io::manipulators::strlike_get(::std::declval<::std::wstring &>()));

static_assert(::fast_io::context_scannable<char, string_token>);
static_assert(::fast_io::context_scannable<char, string_line_token>);
static_assert(::fast_io::context_scannable<char, string_whole_token>);
static_assert(::fast_io::context_scannable<char, string_fixed_count>);
// A different target character domain is valid only through the existing staged code-conversion construction path.
static_assert(::fast_io::context_scannable<char, wide_string_token>);

} // namespace

int main() {}
