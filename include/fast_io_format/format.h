#pragma once

// Include the standard string declaration before the fast_io string adapter.  This ordering
// makes the umbrella reliable even when a consumer has not included <string> previously.
#include <string>

#include "types.h"
#include "print.h"
#include "concat_std.h"
#include "concat_fast_io.h"

