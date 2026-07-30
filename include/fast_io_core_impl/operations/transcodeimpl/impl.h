#pragma once

/*
 * Aggregation boundary for character transcoding protocols and operations.
 *
 * The included files declare transcoder capability CPOs, normalized execution
 * helpers, and printable transcoder manipulators. Transcoding is a typed
 * transformation sublayer: higher IO operations decide why and where to emit,
 * while device primitives ultimately move the converted code units.
 */

#include "defines.h"
#include "ops.h"
#include "transcoder.h"

namespace fast_io
{

namespace transcoders
{
}

namespace trsc = transcoders;

} // namespace fast_io
