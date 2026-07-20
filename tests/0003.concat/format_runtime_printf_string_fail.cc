#include <fast_io_format.h>

#include <string_view>

int main()
{
	::std::string_view format{"%d"};
	::fast_io::fmt::printf(format, 1);
}
