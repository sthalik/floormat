#include "int-hash.hpp"
#include <bit>

namespace floormat {

template<typename T, T offset_basis, T prime>
static CORRADE_ALWAYS_INLINE
T FNVHash(const char* str, size_t size)
{
    const auto* end = str + size;
    T hash = offset_basis;
    for (; str != end; ++str)
    {
        hash *= prime;
        hash ^= (uint8_t)*str;
    }
    return hash;
}

size_t int_hash(uint32_t x) noexcept
{
    if constexpr(sizeof(size_t) == 4)
        return FNVHash<uint32_t, 0x811c9dc5u, 0x01000193u>((const char*)&x, 4);
    else
        return int_hash(uint64_t(x));
}

size_t int_hash(uint64_t x) noexcept
{
    return FNVHash<uint64_t, 0xcbf29ce484222325u, 0x100000001b3u>((const char*)&x, 8);
}

} // namespace floormat
