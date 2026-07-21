#include <fast_io_format.h>

// A character address must not bypass the structural basic_static_string route.
inline constexpr char character_value{'x'};
auto const rejected{::fast_io::mnp::static_arg<&character_value>};

int main() {}
