#pragma once

// The JSON driver is a hosted/freestanding-with-allocation component.  It uses
// fast_io's scalar formatting and scanning engines while keeping the DOM and
// JSON grammar in their own layer.
#include "../fast_io_freestanding.h"

#include "json/concepts.h"
#include "json/error.h"
#include "json/options.h"
#include "json/dom.h"
#include "json/escape.h"
#include "json/simd.h"
#include "json/number.h"
#include "json/parse.h"
#include "json/serialize.h"
#include "json/immutable.h"
#include "json/arena.h"
