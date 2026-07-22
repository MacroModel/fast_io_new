#include <fast_io.h>
#include <fast_io_device.h>

using namespace fast_io::io;

int main()
{
	char8_t a, b, c, d;
	scan(a, ".", b, ".", c, ".", d);
	println(fast_io::mnp::chvw(a), ".", fast_io::mnp::chvw(b), ".", fast_io::mnp::chvw(c), ".",
			fast_io::mnp::chvw(d));
}
