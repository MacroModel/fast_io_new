#include <fast_io.h>

#include <concepts>

#if defined(FAST_IO_HAS_FLOAT64_TYPE)
static_assert(::std::same_as<
	::fast_io::details::float_alias_type<double>, _Float64>);
static_assert(::fast_io::details::my_floating_point<_Float64>);
static_assert(::fast_io::details::compiler_constant_floating_type_supported<
	_Float64>);
static_assert(sizeof(_Float64) == sizeof(double));
static_assert(__FLT64_MANT_DIG__ == 53);
static_assert(__FLT64_MAX_EXP__ == 1024);
static_assert(::fast_io::details::iec559_traits<_Float64>::mbits == 52u);
static_assert(::fast_io::details::iec559_traits<_Float64>::ebits == 11u);
#endif

int main() {}
