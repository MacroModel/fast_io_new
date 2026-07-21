#include <fast_io_format.h>

// Function identity has no type-owned contiguous spelling for static_arg.
constexpr int function_value() noexcept
{
	return 42;
}

auto const rejected{::fast_io::mnp::static_arg<&function_value>};

int main() {}
