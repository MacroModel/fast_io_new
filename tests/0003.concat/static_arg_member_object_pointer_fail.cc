#include <fast_io_format.h>

struct record
{
	int value;
};

// A member locator is not immutable printable content owned by its type.
auto const rejected{::fast_io::mnp::static_arg<&record::value>};

int main() {}
