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
void fast_io_compiler_constant_default_fmt_float64()
{
	::fast_io::fmt::print<"i = {}">(3.14);
}

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void fast_io_compiler_constant_posix_fmt_float64()
{
	::fast_io::fmt::print<"i = {}">(::fast_io::out(), 3.14);
}

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void fast_io_static_argument_posix_fmt_int32()
{
	::fast_io::fmt::print<"i = {}">(
		::fast_io::out(), ::fast_io::mnp::static_arg<32>);
}

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void fast_io_static_argument_posix_raw_int32()
{
	::fast_io::io::print(
		::fast_io::out(), ::fast_io::mnp::static_arg<32>);
}

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void fast_io_static_argument_posix_raw_two_texts()
{
	::fast_io::io::print(
		::fast_io::out(), ::fast_io::mnp::static_arg<"hello">,
		::fast_io::mnp::static_arg<"world">);
}

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void fast_io_static_argument_posix_raw_two_named_texts()
{
	::fast_io::io::print(
		::fast_io::out(), ::fast_io::mnp::static_arg<"left", "hello">,
		::fast_io::mnp::static_arg<"right", "world">);
}

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void fast_io_static_argument_posix_raw_mixed(int value)
{
	::fast_io::io::print(
		::fast_io::out(), ::fast_io::mnp::static_arg<"value=">, value);
}

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void fast_io_compiler_constant_posix_timestamp()
{
	::fast_io::print(
		::fast_io::out(), ::fast_io::unix_timestamp{1700000000, 0});
}

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void fast_io_compiler_constant_posix_fmt_runtime_int(int value)
{
	::fast_io::fmt::print<"i = {}">(::fast_io::out(), value);
}

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void fast_io_compiler_constant_posix_fmt_runtime_float(double value)
{
	::fast_io::fmt::print<"i = {}">(::fast_io::out(), value);
}

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void fast_io_compiler_constant_posix_runtime_timestamp(
	::fast_io::unix_timestamp value)
{
	::fast_io::print(::fast_io::out(), value);
}

} // extern "C"
