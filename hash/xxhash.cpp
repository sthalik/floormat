#include "hash-impl.hpp"
#include "compat/map.hpp"
#include "compat/iota.hpp"
#include <bit>

#ifdef __GNUG__
//#pragma GCC diagnostic ignored "-Wdisabled-macro-expansion"
//#pragma GCC diagnostic ignored "-Wused-but-marked-unused"
#include "xxhash.inl"
#endif

namespace floormat::xxHash {

namespace {

constexpr inline auto seed = std::bit_cast<uint64_t>(iota_array<uint8_t, 8>);

CORRADE_ALWAYS_INLINE size_t do_xxhash(const void* __restrict buf, size_t size) noexcept
{
#ifdef __AVX2__
    return XXH3_64bits(buf, size);
#elif __SSE2__
    return (size_t)XXH3_64bits(buf, size);
#else
    if constexpr(sizeof nullptr > 4)
        return XXH364(buf, size);
    else
        return XXH32(buf, size);
#endif
}

} // namespace

size_t hash_buf(const void* __restrict buf, size_t size) noexcept { return do_xxhash(buf, size); }
size_t hash_int(uint32_t x) noexcept { return do_xxhash(&x, sizeof x); }
size_t hash_int(uint64_t x) noexcept { return do_xxhash(&x, sizeof x); }

} // namespace floormat::xxHash
