#include <fast_io.h>
#include <fast_io_format.h>

extern "C" [[gnu::noinline]] void brace_static_fixed()
{
	::fast_io::fmt::print<"i = {:020.6f}">(
		::fast_io::out(), ::fast_io::mnp::static_arg<12.44>);
}

extern "C" [[gnu::noinline]] void printf_static_fixed()
{
	::fast_io::fmt::printf<"i = %+020.6f">(
		::fast_io::out(), ::fast_io::mnp::static_arg<12.44>);
}

extern "C" [[gnu::noinline]] void brace_static_middle()
{
	::fast_io::fmt::print<"i = {:*^20.6f}">(
		::fast_io::out(), ::fast_io::mnp::static_arg<12.44>);
}

extern "C" [[gnu::noinline]] void brace_runtime_fixed(double value)
{
	::fast_io::fmt::print<"i = {:020.6f}">(::fast_io::out(), value);
}

int main(int argc, char **argv)
{
	if (argc == 1)
	{
		brace_static_fixed();
		printf_static_fixed();
		brace_static_middle();
	}
	else
	{
		brace_runtime_fixed(static_cast<double>(argv[0][0]));
	}
}
