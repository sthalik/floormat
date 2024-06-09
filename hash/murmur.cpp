#include "hash-impl.hpp"
#include <bit>

#ifdef __GNUG__
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

// ReSharper disable CppDefaultCaseNotHandledInSwitchStatement
// NOLINTBEGIN(*-switch-missing-default-case)

namespace floormat::MurmurHash {
namespace {
//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

// Note - The x86 and x64 versions do _not_ produce the same results, as the
// algorithms are optimized for their respective platforms. You can still
// compile and run any of them on any platform, but your performance with the
// non-native version will be less than optimal.

//-----------------------------------------------------------------------------
// Block read - if your platform needs to do endian-swapping or can only
// handle aligned reads, do the conversion here

CORRADE_ALWAYS_INLINE uint32_t getblock32 ( const uint32_t * p, int i )
{
  return p[i];
}

//-----------------------------------------------------------------------------
// Finalization mix - force all bits of a hash block to avalanche

CORRADE_ALWAYS_INLINE uint32_t fmix32 ( uint32_t h )
{
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;

  return h;
}

//-----------------------------------------------------------------------------

void MurmurHash3_x86_32 ( const void * __restrict key, size_t len, void * __restrict out )
{
  constexpr auto seed = uint32_t{0xa6b8edcd};
  const auto* data = (const uint8_t*)key;
  const int nblocks = (int)(len / 4);

  uint32_t h1 = seed;

  constexpr uint32_t c1 = 0xcc9e2d51;
  constexpr uint32_t c2 = 0x1b873593;

  //----------
  // body

  const auto* blocks = (const uint32_t *)(data + (size_t)(nblocks*4));

  for(int i = -nblocks; i; i++)
  {
    uint32_t k1 = getblock32(blocks,i);

    k1 *= c1;
    k1 = std::rotl(k1,15);
    k1 *= c2;

    h1 ^= k1;
    h1 = std::rotl(h1,13);
    h1 = h1*5+0xe6546b64;
  }

  //----------
  // tail

  const auto* tail = (const uint8_t*)(data + (size_t)(nblocks*4));

  uint32_t k1 = 0;

  switch(len & 3)
  {
  case 3: k1 ^= tail[2] << 16; [[fallthrough]];
  case 2: k1 ^= tail[1] << 8;  [[fallthrough]];
  case 1: k1 ^= tail[0];
          k1 *= c1; k1 = std::rotl(k1,15); k1 *= c2; h1 ^= k1;
  }

  //----------
  // finalization

  h1 ^= len;

  h1 = fmix32(h1);

  *(uint32_t*)out = h1;
}

} // namespace

size_t hash_buf(const void* __restrict buf, size_t size) noexcept
{
    uint32_t ret;
    MurmurHash3_x86_32(buf, size, &ret);
    return ret;
}

size_t hash_int(uint32_t x) noexcept
{
    return hash_buf(&x, sizeof x);
}

size_t hash_int(uint64_t x) noexcept
{
    return hash_buf(&x, sizeof x);
}

} // namespace floormat::MurmurHash

// NOLINTEND(*-switch-missing-default-case)
