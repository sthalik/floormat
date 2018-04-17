//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

// Note - The x86 and x64 versions do _not_ produce the same results, as the
// algorithms are optimized for their respective platforms. You can still
// compile and run any of them on any platform, but your performance with the
// non-native version will be less than optimal.

#pragma once

#include <cinttypes>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#   define MURMUR_FORCE_INLINE __forceinline
#   include <intrin.h>
#   include <cstdlib>
#else
#   define MURMUR_FORCE_INLINE inline __attribute__((always_inline))
#endif

using murmur_result = std::pair<std::uint64_t, std::uint64_t>;
murmur_result murmur128(const void* key, int len, std::uint32_t seed = 0);
