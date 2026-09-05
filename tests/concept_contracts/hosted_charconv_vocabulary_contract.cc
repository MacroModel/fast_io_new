#include <fast_io.h>

#include <charconv>
#include <concepts>

// The local vocabulary is an exact alias in hosted mode. These assertions are
// an ABI contract: existing C++20 callers continue to pass std::chars_format
// and receive the standard narrow-character result and error types.
static_assert(::std::same_as<::fast_io::chars_format, ::std::chars_format>);
static_assert(::std::same_as<::fast_io::charconv_errc, ::std::errc>);
static_assert(
	::std::same_as<::fast_io::to_chars_result, ::std::to_chars_result>);
static_assert(
	::std::same_as<::fast_io::from_chars_result, ::std::from_chars_result>);
static_assert(::std::same_as<
	decltype(::fast_io::basic_to_chars_result<char16_t>{}.ec), ::std::errc>);
static_assert(::std::same_as<
	decltype(::fast_io::basic_from_chars_result<char16_t>{}.ec), ::std::errc>);

static_assert(requires(char *first, char *last, double value,
					   ::std::chars_format format) {
	{
		::fast_io::to_chars(first, last, value, format)
	} -> ::std::same_as<::std::to_chars_result>;
});

static_assert(requires(char const *first, char const *last, double &value,
					   ::std::chars_format format) {
	{
		::fast_io::from_chars(first, last, value, format)
	} -> ::std::same_as<::std::from_chars_result>;
});

int main()
{
	return 0;
}
