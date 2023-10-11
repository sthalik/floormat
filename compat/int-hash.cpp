#include "compat/defs.hpp"
#include "int-hash.hpp"
#include <bit>

namespace floormat {

namespace {

[[maybe_unused]]
CORRADE_ALWAYS_INLINE uint32_t FNVHash_32(uint32_t x)
{
    const auto *str = (const char*)&x, *end = str + 4;
    uint32_t hash = 0x811c9dc5u;
fm_UNROLL_4
    for (; str != end; ++str)
    {
        hash *= 0x01000193u;
        hash ^= (uint8_t)*str;
    }
    return hash;
}

CORRADE_ALWAYS_INLINE uint64_t FNVHash_64(uint64_t x)
{
    const auto *str = (const char*)&x, *end = str + 8;
    uint64_t hash = 0xcbf29ce484222325u;
fm_UNROLL_8
    for (; str != end; ++str)
    {
        hash *= 0x100000001b3u;
        hash ^= (uint8_t)*str;
    }
    return hash;
}

} // namespace

uint64_t fnvhash_32(const void* buf, size_t size)
{
    const auto *str = (const char*)buf, *const end = str + size;
    uint32_t hash = 0x811c9dc5u;
fm_UNROLL_8
    for (; str != end; ++str)
    {
        hash *= 0x01000193u;
        hash ^= (uint8_t)*str;
    }
    return hash;
}

uint64_t fnvhash_64(const void* buf, size_t size)
{
    const auto *str = (const char*)buf, *const end = str + size;
    uint64_t hash = 0xcbf29ce484222325u;
fm_UNROLL_4
    for (; str != end; ++str)
    {
        hash *= 0x100000001b3u;
        hash ^= (uint8_t)*str;
    }
    return hash;
}

size_t int_hash(uint32_t x) noexcept
{
    if constexpr(sizeof(size_t) == 4)
        return FNVHash_32(x);
    else
        return FNVHash_64(x);
}

size_t int_hash(uint64_t x) noexcept
{
    return FNVHash_64(x);
}

} // namespace floormat
