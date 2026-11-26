#pragma once

#include "nt_definitions.h"
#include "nt_api.h"
#if defined(_WIN32) && !defined(_WIN32_WINDOWS) && !defined(_KERNEL_MODE) && !defined(__CYGWIN__) && !defined(__WINE__)
#include "nt_async_api.h"
#endif
#include "nt_zw.h"
#include "rtl_unicode_string_unique_ptr.h"
#include "nt_code.h"
#include "nt_path.h"
