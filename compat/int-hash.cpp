#include "compat/defs.hpp"
#include "int-hash.hpp"
#include <bit>

namespace floormat {

namespace {

template<size_t N> struct fnvhash_params;

template<> struct fnvhash_params<32> { uint32_t a = 0x811c9dc5u, b = 0x01000193u; };
template<> struct fnvhash_params<64> { uint64_t a = 0xcbf29ce484222325u, b = 0x100000001b3u; };

[[maybe_unused]]
CORRADE_ALWAYS_INLINE size_t fnvhash_uint_32(uint32_t x)
{
    constexpr auto params = fnvhash_params<sizeof(size_t)*8>{};
    constexpr auto a = params.a, b = params.b;
    auto hash = a;
    const auto* str = (const char*)&x;

    hash *= b; hash ^= (uint8_t)*str++; // 0
    hash *= b; hash ^= (uint8_t)*str++; // 1
    hash *= b; hash ^= (uint8_t)*str++; // 2
    hash *= b; hash ^= (uint8_t)*str++; // 3

    return hash;
}

CORRADE_ALWAYS_INLINE size_t fnvhash_uint_64(uint64_t x)
{
    constexpr auto params = fnvhash_params<sizeof(std::common_type_t<size_t>)*8>{};
    constexpr auto a = params.a, b = params.b;
    auto hash = a;
    const auto* str = (const char*)&x;

    hash *= b; hash ^= (uint8_t)*str++; // 0
    hash *= b; hash ^= (uint8_t)*str++; // 1
    hash *= b; hash ^= (uint8_t)*str++; // 2
    hash *= b; hash ^= (uint8_t)*str++; // 3
    hash *= b; hash ^= (uint8_t)*str++; // 4
    hash *= b; hash ^= (uint8_t)*str++; // 5
    hash *= b; hash ^= (uint8_t)*str++; // 6
    hash *= b; hash ^= (uint8_t)*str++; // 7

    return hash;
}

} // namespace

uint32_t hash_32(const void* buf, size_t size) noexcept
{
    constexpr auto params = fnvhash_params<32>{};
    constexpr auto a = params.a, b = params.b;
    auto hash = a;
    const auto *str = (const char*)buf, *const end = str + size;

fm_UNROLL_4
    for (; str != end; ++str)
    {
        hash *= b;
        hash ^= (uint8_t)*str;
    }
    return hash;
}

uint64_t hash_64(const void* buf, size_t size) noexcept
{
    constexpr auto params = fnvhash_params<sizeof(size_t)*8>{};
    constexpr auto a = params.a, b = params.b;
    auto hash = a;
    const auto *str = (const char*)buf, *const end = str + size;

fm_UNROLL_8
    for (; str != end; ++str)
    {
        hash *= b;
        hash ^= (uint8_t)*str;
    }
    return hash;
}

size_t hash_int(uint32_t x) noexcept
{
    return fnvhash_uint_32(x);
}

size_t hash_int(uint64_t x) noexcept
{
    return fnvhash_uint_64(x);
}

} // namespace floormat
