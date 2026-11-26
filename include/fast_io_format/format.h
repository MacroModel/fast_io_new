#pragma once

/*
 * Aggregation boundary for the format frontend (FMT level).
 *
 * This file assembles the structural format vocabulary, print front doors, and
 * string-concat front doors. The parser/compiler/lowering implementation is
 * intentionally reached through those facades. Every successful operation
 * leaves this level as ordinary typed IO arguments: output goes to
 * `fast_io::io::print`, while materialization goes to the IO concat engine.
 */

#if (__STDC_HOSTED__ && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED) && !defined(_LIBCPP_FREESTANDING)) || defined(FAST_IO_ENABLE_HOSTED_FEATURES)
// Only standard-string destinations require the hosted library's complete string definition.
#include <string>
#endif
#include "types.h"
#include "print.h"
#include "to.h"
#if (__STDC_HOSTED__ && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED) && !defined(_LIBCPP_FREESTANDING)) || defined(FAST_IO_ENABLE_HOSTED_FEATURES)
#include "concat_std.h"
#include "concat_fast_io.h"
#endif
