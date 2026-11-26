#pragma once

/*

GCC provides more headers in freestanding environment than standard requires because WG21 sucks very badly.
no ::std::array, ::std::copy and others

*/
#if defined(__GLIBCXX__) && __has_include(<bits/ranges_base.h>) && __has_include(<bits/stl_iterator.h>) && _GLIBCXX_RELEASE >= 11
#include <initializer_list>
#if _GLIBCXX_RELEASE < 12
// libstdc++ 11 declares tuple_size_v in its public tuple header.
#include <tuple>
#endif
#include <bits/stl_iterator.h>
#include <bits/ranges_base.h>
#else
#include <iterator>
#include <ranges>
#endif

#if defined(__GLIBCXX__) && __has_include(<bits/move.h>)
#include <bits/move.h>
#else
#include <utility>
#endif

#if defined(__GLIBCXX__) && __has_include(<bits/ptr_traits.h>)
#include <bits/ptr_traits.h>
#else
#include <memory>
#endif

#if defined(_LIBCPP_VERSION) && __has_include(<__memory/allocator.h>)
#include <__memory/allocator.h>
#include "std_allocator.h"
#elif defined(__GLIBCXX__) && __has_include(<bits/allocator.h>) && __has_include(<bits/new_allocator.h>)
#if _GLIBCXX_HOSTED == 1
#include <bits/allocator.h>
#include "std_allocator.h"
#else
#include "allocator.h"
#endif
#elif defined(_MSVC_STL_UPDATE) && __has_include(<xmemory>)
#include <xmemory>
#include "std_allocator.h"
#else
#include "allocator.h"
#endif

#if defined(__GLIBCXX__) && __has_include(<bits/stl_algo.h>)
// The freestanding algorithms use the classic declarations. Including their defining headers avoids
// instantiating the hosted ranges algorithms on every print, scan and transmit translation unit.
#include <bits/stl_algobase.h>
#include <bits/stl_algo.h>
#else
#include <algorithm>
#endif

#include "noexcept_call.h"
#include "array.h"
#include "cstr_len.h"
#include "bytes.h"
#include "relocatable.h"
#include "algorithm.h"
#include "ranges.h"
