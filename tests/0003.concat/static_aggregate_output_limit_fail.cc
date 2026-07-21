#include <fast_io_format.h>

#include <array>

inline constexpr ::std::array<int, 256u> values{};

int main()
{
	// Each of the 256 elements contributes a 256-code-unit field plus two
	// structural code units, so the replacement is known to require 66,048
	// code units. Core must reject that shape before provider materialization;
	// it must not silently select a dynamic path above the 65,536-unit budget.
	(void)::fast_io::fmt::concat_std<"{::0256}">(
		::fast_io::mnp::static_arg<values>);
}
