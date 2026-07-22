#pragma once

#if !defined(__cplusplus)
#error "You must be using a C++ compiler"
#endif

#include <algorithm>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

#include "impl/misc/push_macros.h"
#include "impl/misc/push_warnings.h"
#include "../fast_io_core_impl/terminate.h"
#include "impl/hash_map.h"

namespace fast_io
{

template <typename Key, typename T, typename Hash = ::std::hash<Key>,
		  typename KeyEqual = ::std::equal_to<>,
		  typename Allocator = ::std::allocator<::std::pair<Key const, T>>>
using hash_map = ::fast_io::containers::basic_hash_map<
	Key, T, Hash, KeyEqual, Allocator>;

} // namespace fast_io

#include "impl/misc/pop_warnings.h"
#include "impl/misc/pop_macros.h"
