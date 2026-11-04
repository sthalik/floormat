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

CORRADE_ALWAYS_INLINE u32 getblock32 ( const u32 * p, int i )
{
  return p[i];
}

//-----------------------------------------------------------------------------
// Finalization mix - force all bits of a hash block to avalanche

CORRADE_ALWAYS_INLINE u32 fmix32 ( u32 h )
{
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;

  return h;
}

//-----------------------------------------------------------------------------

void MurmurHash3_x86_32 ( const void * __restrict key, usize len, void * __restrict out )
{
  constexpr auto seed = u32{0xa6b8edcd};
  const auto* data = (const u8*)key;
  const int nblocks = (int)(len / 4);

  u32 h1 = seed;

  constexpr u32 c1 = 0xcc9e2d51;
  constexpr u32 c2 = 0x1b873593;

  //----------
  // body

  const auto* blocks = (const u32 *)(data + (usize)(nblocks*4));

  for(int i = -nblocks; i; i++)
  {
    u32 k1 = getblock32(blocks,i);

    k1 *= c1;
    k1 = std::rotl(k1,15);
    k1 *= c2;

    h1 ^= k1;
    h1 = std::rotl(h1,13);
    h1 = h1*5+0xe6546b64;
  }

  //----------
  // tail

  const auto* tail = (const u8*)(data + (usize)(nblocks*4));

  u32 k1 = 0;

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

  *(u32*)out = h1;
}

} // namespace

usize hash_buf(const void* __restrict buf, usize size) noexcept
{
    u32 ret;
    MurmurHash3_x86_32(buf, size, &ret);
    return ret;
}

usize hash_int(u32 x) noexcept
{
    return hash_buf(&x, sizeof x);
}

usize hash_int(u64 x) noexcept
{
    return hash_buf(&x, sizeof x);
}

} // namespace floormat::MurmurHash

// NOLINTEND(*-switch-missing-default-case)
