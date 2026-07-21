#include <fast_io_format.h>

// Naming a pointer does not make its addressed object part of the static argument.
inline constexpr int object_value{42};
auto const rejected{
	::fast_io::mnp::static_arg<"answer", &object_value>};

int main() {}
