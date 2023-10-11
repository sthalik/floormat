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

#define fnvhash_64_ROUND() do { hash *= 0x01000193u; hash ^= (uint8_t)*str++; } while(false)

uint64_t fnvhash_64(const void* str_, size_t size)
{
    const auto *str = (const char*)str_;
    uint32_t hash = 0x811c9dc5u;
    size_t full = size / 8;
    for (size_t i = 0; i < full; i++)
    {
        fnvhash_64_ROUND();
        fnvhash_64_ROUND();
        fnvhash_64_ROUND();
        fnvhash_64_ROUND();
        fnvhash_64_ROUND();
        fnvhash_64_ROUND();
        fnvhash_64_ROUND();
        fnvhash_64_ROUND();
    }
    switch (size & 7u)
    {
    case 7: fnvhash_64_ROUND(); [[fallthrough]];
    case 6: fnvhash_64_ROUND(); [[fallthrough]];
    case 5: fnvhash_64_ROUND(); [[fallthrough]];
    case 4: fnvhash_64_ROUND(); [[fallthrough]];
    case 3: fnvhash_64_ROUND(); [[fallthrough]];
    case 2: fnvhash_64_ROUND(); [[fallthrough]];
    case 1: fnvhash_64_ROUND(); [[fallthrough]];
    case 0: break;
    }
    return hash;
}

uint64_t fnvhash_32(const void* str_, size_t size)
{
    const auto *str = (const char*)str_, *end = str + size;
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
