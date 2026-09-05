#include <cstddef>

#if !defined(__STDC_HOSTED__) || __STDC_HOSTED__ != 0
#error "This contract must be compiled with the compiler's real freestanding mode."
#endif

namespace fast_io
{

// A complete allocator declaration proves that the public umbrella selects
// its documented freestanding customization path without borrowing hosted
// allocation or exception facilities.
class custom_global_allocator
{
public:
	static void *allocate(::std::size_t) noexcept;
	static void deallocate(void *) noexcept;
};

} // namespace fast_io

#include <fast_io_legacy.h>

#include <concepts>

static_assert(::std::same_as<typename ::fast_io::io_type_t<int>::type, int>);
static_assert((::fast_io::chars_format::fixed |
			   ::fast_io::chars_format::scientific) ==
			  ::fast_io::chars_format::general);

int main()
{
	char buffer[64]{};
	auto const converted{::fast_io::to_chars(
		buffer, buffer + sizeof(buffer), 1.25,
		::fast_io::chars_format::general)};
	return converted.ec == ::fast_io::charconv_errc{} ? 0 : 1;
}
