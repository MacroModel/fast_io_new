#include <fast_io_format.h>

// An address identifies storage; it is not type-owned immutable printable data.
inline constexpr int object_value{42};
auto const rejected{::fast_io::mnp::static_arg<&object_value>};

int main() {}
