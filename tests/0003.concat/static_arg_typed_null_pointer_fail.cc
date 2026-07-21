#include <fast_io_format.h>

// A null pointer value is still a pointer and has no static printable payload.
auto const rejected{
	::fast_io::mnp::static_arg<static_cast<int const *>(nullptr)>};

int main() {}
