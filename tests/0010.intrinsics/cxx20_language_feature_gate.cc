#include <type_traits>

#include "../../include/fast_io_dsal/impl/misc/push_macros.h"

constexpr bool constant_selection_probe() noexcept
{
	FAST_IO_IF_CONSTEVAL
	{
		return true;
	}
	else
	{
		return false;
	}
}

inline bool runtime_selection_probe() noexcept
{
	FAST_IO_IF_NOT_CONSTEVAL
	{
		return true;
	}
	else
	{
		return false;
	}
}

inline int assumption_probe(int value) noexcept
{
	FAST_IO_ASSUME(value >= 0);
	return value;
}

static_assert(constant_selection_probe());

#if ((defined(_MSVC_LANG) && _MSVC_LANG == 202002L) || \
	 (!defined(_MSVC_LANG) && __cplusplus == 202002L))
static_assert(!FAST_IO_HAS_STATIC_CALL_OPERATOR_IN_LANGUAGE_MODE);
#endif

#include "../../include/fast_io_dsal/impl/misc/pop_macros.h"

int main()
{
	return runtime_selection_probe() && assumption_probe(1) == 1 ? 0 : 1;
}
