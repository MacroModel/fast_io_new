#include <fast_io.h>
#include <fast_io_format.h>

// A field-only entry catches constantness lost specifically while fmt builds
// and transports its semantic scalar.  The prefixed main probe additionally
// covers the final contiguous-record merge, so neither can substitute for the
// other in the assembly gate.
extern "C" void fast_io_fmt_constant_float_only()
{
	::fast_io::fmt::print<"{}">(::fast_io::out(), 3.14);
}

int main(int, char **)
{
	::fast_io::fmt::print<"i={}">(::fast_io::out(), 3.2);
}
