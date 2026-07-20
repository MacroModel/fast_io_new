#include <fast_io.h>
#include <fast_io_format.h>

extern "C"
{

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void fast_io_compiler_constant_default_fmt_int32()
{
	::fast_io::fmt::print<"i = {}">(32);
}

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void fast_io_compiler_constant_posix_fmt_int32()
{
	::fast_io::fmt::print<"i = {}">(::fast_io::out(), 32);
}

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void fast_io_compiler_constant_posix_fmt_runtime_int(int value)
{
	::fast_io::fmt::print<"i = {}">(::fast_io::out(), value);
}

} // extern "C"
