#include "hash-impl.hpp"
#if FM_HASH_HAVE_MEOWHASH
#include "meow_hash_x64_aesni.h"
#include "compat/assert.hpp"

#ifdef __GNUG__
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-macros"
#endif

#define fm_MeowU64From(A, I) (*(meow_u64 *)&(A) + (I))

namespace floormat::meow_hash {
namespace {

CORRADE_ALWAYS_INLINE size_t hash(void* __restrict buf, size_t size)
{
#ifndef fm_NO_DEBUG
    if (size % 16 != 0) [[unlikely]]
        fm_abort("size %zu %% 16 == %zu", size, size % 16);
#endif
    meow_u128 hash = MeowHash(MeowDefaultSeed, size, buf);
    if constexpr(sizeof nullptr > 4)
        return fm_MeowU64From(hash, 0);
    else
        return MeowU32From(hash, 0);
}

} // namespace

size_t hash_buf(const void* __restrict buf, size_t size) noexcept
{
    return hash(const_cast<void*>(buf), size);
}

size_t hash_int(uint32_t x) noexcept
{
    return hash(&x, sizeof x);
}

size_t hash_int(uint64_t x) noexcept
{
    return hash(&x, sizeof x);
}

} // namespace floormat::meow_hash

#endif
