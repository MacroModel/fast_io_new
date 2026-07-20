#include <fast_io.h>
#include <fast_io_format.h>

// Compile this probe in both C++20 and C++23 modes. GCC exposes its native
// `_Float64` extension in both modes, so the optimizer-proven proxy must have
// identical code shape even though `__STDCPP_FLOAT64_T__` is C++23-only.
extern "C" __attribute__((noinline)) void constant_width_float64_alias()
{
	::fast_io::fmt::print<"i = {:020.6f}">(::fast_io::out(), 12.44);
}

extern "C" __attribute__((noinline)) void runtime_width_float64_alias(
	double value)
{
	::fast_io::fmt::print<"i = {:020.6f}">(::fast_io::out(), value);
}
