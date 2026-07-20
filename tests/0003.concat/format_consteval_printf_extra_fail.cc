#include <fast_io_format.h>

consteval bool printf_extra_argument_must_be_rejected()
{
	auto const result{::fast_io::fmt::concatf_std<"%d">(1, 2)};
	return result == "1";
}

static_assert(printf_extra_argument_must_be_rejected());

int main()
{}
