#include <fast_io.h>
#include <fast_io_format.h>

int main(int, char **)
{
	::fast_io::fmt::print<"i={}">(::fast_io::out(), 3.2);
}
