// IEEE 754 decimal interchange types and their freestanding text I/O.
#pragma once

#include "fast_io_freestanding.h"

#if !defined(FAST_IO_DISABLE_FLOATING_POINT) && !defined(FAST_IO_DETAIL_DEC_F_D_FLOAT)
#include "fast_io_unit/floating/interchange_cast.h"
#include "fast_io_unit/decimal/print.h"
#include "fast_io_unit/decimal/scan.h"
#endif
