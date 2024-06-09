#include "hash-impl.hpp"
#include "compat/iota.hpp"
#include <cstdio>
#include <bit>

#ifdef __clang__
#pragma GCC diagnostic ignored "-Wdisabled-macro-expansion"
#pragma GCC diagnostic ignored "-Wused-but-marked-unused"
#endif
#include "xxhash.inl"

namespace floormat::xxHash {

namespace {

CORRADE_ALWAYS_INLINE size_t do_xxhash(const void* __restrict buf, size_t size) noexcept
{
#ifdef __AVX2__
    return (size_t)XXH3_64bits(buf, size);
#elif __SSE2__
    return (size_t)XXH3_64bits(buf, size);
#else
    constexpr auto seed = std::bit_cast<size_t>(iota_array<uint8_t, sizeof nullptr>);
    if constexpr(sizeof nullptr > 4)
        return XXH64(buf, size, seed);
    else
        return XXH32(buf, size, seed);
#endif
}

} // namespace

size_t hash_buf(const void* __restrict buf, size_t size) noexcept { return do_xxhash(buf, size); }
size_t hash_int(uint32_t x) noexcept { return do_xxhash(&x, sizeof x); }
size_t hash_int(uint64_t x) noexcept { return do_xxhash(&x, sizeof x); }

} // namespace floormat::xxHash
