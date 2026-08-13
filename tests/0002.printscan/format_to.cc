#include <cassert>
#include <string>

#include <fast_io_format.h>

int main()
{
	auto const value{::fast_io::fmt::to<"{}", int>(42)};
	assert(value == 42);

	auto const text{::fast_io::fmt::to<"value={}", ::std::string>(42)};
	assert(text == "value=42");

	::std::string inplace_text;
	::fast_io::fmt::inplace_to<"value={}">(inplace_text, 7);
	assert(inplace_text == "value=7");

	auto const percent{::fast_io::fmt::printf_to<int, "%d">(42)};
	assert(percent == 42);
}
