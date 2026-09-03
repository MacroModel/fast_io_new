#include <type_traits>

#include <fast_io_core.h>

namespace
{

/*
The probe intentionally models libc declarations such as Darwin `shm_open`.
Its observable result depends only on the fixed argument, so the ellipsis does
not introduce a platform ABI dependency into the test.
*/
int c_variadic_identity(int value, ...)
{
	return value;
}

using throwing_variadic_type = int(int, ...);
using nonthrowing_variadic_type = int(int, ...) noexcept;
using throwing_ellipsis_only_type = int(...);
using nonthrowing_ellipsis_only_type = int(...) noexcept;

static_assert(::std::is_same_v<
			  ::fast_io::freestanding::make_noexcept_t<throwing_variadic_type>,
			  nonthrowing_variadic_type>);
static_assert(::std::is_same_v<
			  ::fast_io::freestanding::make_noexcept_t<nonthrowing_variadic_type>,
			  nonthrowing_variadic_type>);
static_assert(::std::is_same_v<
			  ::fast_io::freestanding::make_noexcept_t<
				  throwing_ellipsis_only_type>,
			  nonthrowing_ellipsis_only_type>);

} // namespace

int main()
{
	return ::fast_io::noexcept_call(c_variadic_identity, 42) == 42 ? 0 : 1;
}
