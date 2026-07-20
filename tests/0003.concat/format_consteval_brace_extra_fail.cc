#include <fast_io_format.h>

consteval bool brace_extra_argument_must_be_rejected()
{
	auto const result{::fast_io::fmt::concat_std<"{}">(1, 2)};
	return result == "1";
}

static_assert(brace_extra_argument_must_be_rejected());

int main()
{}
