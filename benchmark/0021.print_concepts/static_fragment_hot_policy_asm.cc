#include <fast_io.h>
#include <fast_io_format.h>

extern "C"
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void fast_io_static_fragment_policy_probe(int fd)
{
	::fast_io::fmt::print<"i = {}">(
		::fast_io::posix_io_observer{fd}, 32);
}

