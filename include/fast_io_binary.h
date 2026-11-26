#pragma once

#include "fast_io_freestanding.h"

#if !defined(FAST_IO_DISABLE_FLOATING_POINT) && !defined(FAST_IO_DETAIL_DEC_F_D_FLOAT)
#include "fast_io_unit/floating/interchange_cast.h"
#include "fast_io_unit/floating/binary_interchange_io.h"
#endif
