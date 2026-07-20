#include <fast_io.h>
#include <fast_io_format.h>

extern "C" [[gnu::noinline]] void constant_binary32_static_width()
{
	::fast_io::fmt::print<"i = {:020.6f}">(::fast_io::out(), 12.44f);
}

extern "C" [[gnu::noinline]] void runtime_binary32_static_width(float value)
{
	::fast_io::fmt::print<"i = {:020.6f}">(::fast_io::out(), value);
}

extern "C" [[gnu::noinline]] void runtime_binary32_no_width(float value)
{
	::fast_io::fmt::print<"{:+.6f}">(::fast_io::out(), value);
}

int main(int argc, char **argv)
{
	if (argc == 1)
	{
		constant_binary32_static_width();
	}
	else
	{
		runtime_binary32_static_width(static_cast<float>(argc));
		runtime_binary32_no_width(static_cast<float>(argv[0][0]));
	}
}
