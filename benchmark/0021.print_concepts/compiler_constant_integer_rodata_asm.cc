#include <fast_io.h>
#include <fast_io_format.h>

int main()
{
#if defined(FAST_IO_COMPILER_CONSTANT_INTEGER_STATIC_OUT)
	::fast_io::fmt::print<"i = {}">(
		::fast_io::out(), ::fast_io::mnp::static_arg<32>);
#elif defined(FAST_IO_COMPILER_CONSTANT_INTEGER_EXPLICIT_OUT)
	::fast_io::fmt::print<"i = {}">(::fast_io::out(), 32);
#else
	::fast_io::fmt::print<"i = {}">(32);
#endif
}
