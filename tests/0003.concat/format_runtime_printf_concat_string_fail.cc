#include <fast_io_format.h>

#include <string_view>

int main()
{
	::std::string_view format{"%d"};
	(void)::fast_io::fmt::concatf_fast_io(format, 1);
}
