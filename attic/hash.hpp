#pragma once
#include <cstddef>
#include <cstdint>

namespace floormat {

template<unsigned N = sizeof(std::size_t)*8> struct hash;

template<>
struct hash<32> final {
    [[maybe_unused]]
    constexpr std::uint32_t operator()(std::uint32_t x) const noexcept {
        return (std::uint32_t)x*0x9e3779b1u;
    }
};

template<>
struct hash<64> final {
    [[maybe_unused]]
    constexpr std::uint64_t operator()(std::uint64_t x) const noexcept {
        return x*0x9e3779b97f4a7c15ull;
    }
};

} // namespace floormat
