#include <fast_io_format.h>

consteval bool default_output_cannot_be_constant_evaluated()
{
	::fast_io::fmt::print<"{}">(1);
	return true;
}

static_assert(default_output_cannot_be_constant_evaluated());

int main()
{}
