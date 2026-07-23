#pragma once
// Public protocol vocabulary for user-defined fast_io devices, values, scanners, formatters, and string results.
// Each concept checks the structural part of its customization contract. Provider comments at the declaration record
// the additional bounds, lifetime, ownership, exception, and observational-equivalence obligations which C++ concept
// expressions cannot prove.
#if !defined(__cplusplus)
#error "You must be using a C++ compiler"
#endif
#if !defined(__cpp_concepts)
#error "fast_io requires at least a C++20 standard compiler."
#else

#include <version>
#include <cstddef>
#include <type_traits>
#include <concepts>
#include <cstdint>
// Public protocol concepts form numeric capacity constants without relying on a higher-level umbrella header.
#include <limits>

#include "fast_io_dsal/impl/misc/push_macros.h"
#include "fast_io_dsal/impl/misc/push_warnings.h"

#include "fast_io_freestanding_impl/stack/impl.h"

#include "fast_io_core_impl/freestanding/addressof.h"
#include "fast_io_core_impl/concepts/impl.h"

#include "fast_io_dsal/impl/misc/pop_macros.h"
#include "fast_io_dsal/impl/misc/pop_warnings.h"

#endif
