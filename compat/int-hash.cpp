#include "compat/defs.hpp"
#include "int-hash.hpp"
#include <Corrade/Containers/StringView.h>
#include <bit>
#include <utility>

namespace floormat {

namespace {

using namespace floormat::Hash;

// todo implement in terms of fnvhash_buf()
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

size_t hash_int(uint32_t x) noexcept { return fnvhash_uint_32(x); }
size_t hash_int(uint64_t x) noexcept { return fnvhash_uint_64(x); }
size_t hash_string_view::operator()(StringView s) const noexcept { return Hash::fnvhash_buf(s.data(), s.size()); }

} // namespace floormat

namespace floormat::Hash {

size_t fnvhash_buf(const void* __restrict buf, size_t size, size_t seed) noexcept
{
    constexpr size_t b{fnvhash_params<sizeof nullptr*8>::b};
    size_t full_rounds = size / 8, rest = size % 8;
    size_t hash = seed;
    const char* str = (const char*)buf;

    while (full_rounds--)
    {
        hash *= b; hash ^= (uint8_t)*str++; // 0
        hash *= b; hash ^= (uint8_t)*str++; // 1
        hash *= b; hash ^= (uint8_t)*str++; // 2
        hash *= b; hash ^= (uint8_t)*str++; // 3
        hash *= b; hash ^= (uint8_t)*str++; // 4
        hash *= b; hash ^= (uint8_t)*str++; // 5
        hash *= b; hash ^= (uint8_t)*str++; // 6
        hash *= b; hash ^= (uint8_t)*str++; // 7
    }
    switch (rest)
    {
    case 7: hash *= b; hash ^= (uint8_t)*str++; [[fallthrough]];
    case 6: hash *= b; hash ^= (uint8_t)*str++; [[fallthrough]];
    case 5: hash *= b; hash ^= (uint8_t)*str++; [[fallthrough]];
    case 4: hash *= b; hash ^= (uint8_t)*str++; [[fallthrough]];
    case 3: hash *= b; hash ^= (uint8_t)*str++; [[fallthrough]];
    case 2: hash *= b; hash ^= (uint8_t)*str++; [[fallthrough]];
    case 1: hash *= b; hash ^= (uint8_t)*str++; [[fallthrough]];
    case 0: break;
    default: std::unreachable();
    }
    return hash;
}

} // namespace floormat::Hash
