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

namespace ent_detail {

struct murmur128_impl final
{
    murmur128_impl() = delete;

    using result_type = std::pair<std::uint64_t, std::uint64_t>;

    using uint64_t = std::uint64_t;
    using uint8_t = std::uint8_t;
    using uint32_t = std::uint32_t;

#ifdef _MSC_VER
    static inline uint32_t rotl32(uint32_t x, int n)
    {
        return _rotl(x, n);
    }
#else
    static inline uint32_t rotl32(uint32_t x, int n)
    {
        return ((x << n) | (x >> (64 - n)));
    }
#endif

    static MURMUR_FORCE_INLINE uint32_t fmix32(uint32_t h)
    {
      h ^= h >> 16;
      h *= 0x85ebca6b;
      h ^= h >> 13;
      h *= 0xc2b2ae35;
      h ^= h >> 16;

      return h;
    }

    static MURMUR_FORCE_INLINE uint32_t getblock32(const uint32_t* p, int i)
    {
      return p[i];
    }

    static result_type murmur128(const void* key, int len, uint32_t seed);
};

} // ns detail

static MURMUR_FORCE_INLINE ent_detail::murmur128_impl::result_type murmur128(const void* key, int len, uint32_t seed = 0u)
{
    return ent_detail::murmur128_impl::murmur128(key, len, seed);
}
