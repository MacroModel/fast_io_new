#include <fast_io_format.h>

#include <string_view>

int main()
{
	::std::string_view format{"{}"};
	::fast_io::fmt::print(format, 1);
}
