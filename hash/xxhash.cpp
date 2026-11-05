#include "hash-impl.hpp"
#include "compat/iota.hpp"
#include <cstdio>
#include <bit>

#ifdef __clang__
#pragma GCC diagnostic ignored "-Wdisabled-macro-expansion"
#pragma GCC diagnostic ignored "-Wused-but-marked-unused"
#elif defined _MSC_VER
#pragma warning(disable: 4310)
#endif
#include "xxhash.inl"

namespace floormat::xxHash {

namespace {

CORRADE_ALWAYS_INLINE usize do_xxhash(const void* __restrict buf, usize size) noexcept
{
#ifdef __AVX2__
    return (usize)XXH3_64bits(buf, size);
#elifdef __SSE2__
    return (usize)XXH3_64bits(buf, size);
#else
    constexpr auto seed = std::bit_cast<usize>(iota_array<u8, sizeof nullptr>);
    if constexpr(sizeof nullptr > 4)
        return XXH64(buf, size, seed);
    else
        return XXH32(buf, size, (u32)seed);
#endif
}

} // namespace

usize hash_buf(const void* __restrict buf, usize size) noexcept { return do_xxhash(buf, size); }

} // namespace floormat::xxHash
