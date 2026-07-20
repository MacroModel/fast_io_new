#include <fast_io.h>
#include <fast_io_format.h>

extern "C" [[gnu::noinline]] void fixed_width_constant_int()
{
	::fast_io::fmt::print<"i={:08}">(::fast_io::out(), 42);
}

extern "C" [[gnu::noinline]] void fixed_width_runtime_int(int value)
{
	::fast_io::fmt::print<"i={:08}">(::fast_io::out(), value);
}

extern "C" [[gnu::noinline]] void dynamic_width_constant_int()
{
	::fast_io::fmt::print<"i={0:0{1}}">(::fast_io::out(), 42, 8u);
}

extern "C" [[gnu::noinline]] void dynamic_width_runtime_int(
	int value, unsigned width)
{
	::fast_io::fmt::print<"i={0:0{1}}">(
		::fast_io::out(), value, width);
}
