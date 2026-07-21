#include <fast_io_format.h>

struct record
{
	constexpr int value() const noexcept
	{
		return 42;
	}
};

// A member-function locator belongs to the same rejected address-bearing domain.
auto const rejected{::fast_io::mnp::static_arg<&record::value>};

int main() {}
