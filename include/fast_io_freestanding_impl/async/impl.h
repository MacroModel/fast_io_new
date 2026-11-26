#pragma once

#include <coroutine>
#include <new>
#include <utility>
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
#include <exception>
#endif
#if defined(__HERBCEPTIONS__)
#include <herbceptions/error>
#define FAST_IO_ASYNC_THROWS throws
#else
#define FAST_IO_ASYNC_THROWS
#endif

#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && !defined(_LIBCPP_FREESTANDING)) || defined(FAST_IO_ENABLE_HOSTED_FEATURES)) && !defined(__AVR__) && !defined(__MSDOS__)
#include <atomic>
#define FAST_IO_ASYNC_HOSTED_CONFIG
#endif

#include "../../fast_io_core_impl/concepts/async_io.h"
#include "queue.h"
#include "scheduler.h"
#include "file_activity.h"
#include "observation.h"
#include "task.h"
#include "resident.h"
#include "blocking.h"
#include "points.h"
#include "basic_async.h"
#include "operations.h"
#include "conversion.h"

#undef FAST_IO_ASYNC_THROWS
#undef FAST_IO_ASYNC_HOSTED_CONFIG
