#pragma once

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#if defined(_M_X64) || defined(_M_AMD64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
#include <xmmintrin.h>
#endif
#endif

#include "platform.h"
#include "prfch.h"
