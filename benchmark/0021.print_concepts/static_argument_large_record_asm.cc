#include <fast_io.h>
#include <fast_io_format.h>

template <::std::size_t extent>
[[nodiscard]] consteval auto make_repeated_static_text(char value)
{
	char text[extent + 1u]{};
	for (::std::size_t index{}; index != extent; ++index)
	{
		text[index] = value;
	}
	return ::fast_io::fmt::basic_fixed_string{text};
}

extern "C"
{

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void fast_io_static_argument_record_65(int fd)
{
	::fast_io::fmt::print<"{}">(
		::fast_io::posix_io_observer{fd},
		::fast_io::mnp::static_arg<make_repeated_static_text<65u>('a')>);
}

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void fast_io_static_argument_record_4k(int fd)
{
	::fast_io::fmt::print<"{}">(
		::fast_io::posix_io_observer{fd},
		::fast_io::mnp::static_arg<make_repeated_static_text<4096u>('b')>);
}

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void fast_io_static_argument_record_limit(int fd)
{
	// 64 KiB is the public static-provider code-unit budget. This probe
	// deliberately reaches that semantic limit rather than a transport cutoff.
	::fast_io::fmt::print<"{}">(
		::fast_io::posix_io_observer{fd},
		::fast_io::mnp::static_arg<make_repeated_static_text<65536u>('c')>);
}

} // extern "C"

int main()
{
	return 0;
}
