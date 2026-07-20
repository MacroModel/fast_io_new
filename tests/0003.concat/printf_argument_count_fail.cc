#include <fast_io.h>
#include <fast_io_format.h>

int main(int, char **argv)
{
	::fast_io::fmt::printf<"a%sc%s">(
		::fast_io::out(), "d", "b", argv);
}
