#include "hash-impl.hpp"

// ReSharper disable CppDefaultCaseNotHandledInSwitchStatement
// NOLINTBEGIN(*-multiway-paths-covered, *-switch-missing-default-case)

namespace floormat::FNVHash {
namespace {

template<size_t N = sizeof nullptr * 8> struct fnvhash_params;
template<> struct fnvhash_params<32> { [[maybe_unused]] static constexpr uint32_t a = 0x811c9dc5u, b = 0x01000193u; };
template<> struct fnvhash_params<64> { [[maybe_unused]] static constexpr uint64_t a = 0xcbf29ce484222325u, b = 0x100000001b3u; };

CORRADE_ALWAYS_INLINE size_t fnvhash_uint(uint64_t x)
{
    constexpr auto params = fnvhash_params{};
    constexpr auto a = params.a, b = params.b;
    auto hash = a;
    const auto* str = (const uint8_t*)&x;

    hash ^= *str++; hash *= b; // 0
    hash ^= *str++; hash *= b; // 1
    hash ^= *str++; hash *= b; // 2
    hash ^= *str++; hash *= b; // 3
    hash ^= *str++; hash *= b; // 4
    hash ^= *str++; hash *= b; // 5
    hash ^= *str++; hash *= b; // 6
    hash ^= *str++; hash *= b; // 7

    return hash;
}

CORRADE_ALWAYS_INLINE size_t fnvhash_uint(uint32_t x)
{
    constexpr auto params = fnvhash_params{};
    constexpr auto a = params.a, b = params.b;

    auto hash = a;
    const auto* str = (const uint8_t*)&x;

    hash ^= *str++; hash *= b; // 0
    hash ^= *str++; hash *= b; // 1
    hash ^= *str++; hash *= b; // 2
    hash ^= *str++; hash *= b; // 3

    return hash;
}

CORRADE_ALWAYS_INLINE size_t fnvhash_buf(const void* __restrict buf, size_t size) noexcept
{
    constexpr auto params = fnvhash_params{};
    constexpr auto a = params.a, b = params.b;
    size_t full_rounds = size / 8, rest = size % 8;
    size_t hash = a;
    const auto* str = (const uint8_t*)buf;

    while (full_rounds--)
    {
        hash ^= *str++; hash *= b; // 0
        hash ^= *str++; hash *= b; // 1
        hash ^= *str++; hash *= b; // 2
        hash ^= *str++; hash *= b; // 3
        hash ^= *str++; hash *= b; // 4
        hash ^= *str++; hash *= b; // 5
        hash ^= *str++; hash *= b; // 6
        hash ^= *str++; hash *= b; // 7
    }

    switch (rest)
    {
    case 7: hash ^= *str++; hash *= b; [[fallthrough]];
    case 6: hash ^= *str++; hash *= b; [[fallthrough]];
    case 5: hash ^= *str++; hash *= b; [[fallthrough]];
    case 4: hash ^= *str++; hash *= b; [[fallthrough]];
    case 3: hash ^= *str++; hash *= b; [[fallthrough]];
    case 2: hash ^= *str++; hash *= b; [[fallthrough]];
    case 1: hash ^= *str++; hash *= b; [[fallthrough]];
    case 0: break;
    }

    return hash;
}

} // namespace

size_t hash_buf(const void* __restrict buf, size_t size) noexcept
{
    return fnvhash_buf(buf, size);
}

size_t hash_int(uint32_t x) noexcept
{
    return fnvhash_uint(x);
}

size_t hash_int(uint64_t x) noexcept
{
    return fnvhash_uint(x);
}

} // namespace floormat::FNVHash

// NOLINTEND(*-multiway-paths-covered, *-switch-missing-default-case)
