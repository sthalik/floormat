#pragma once
#include "src/chunk.hpp"
#include "compat/assert.hpp"

namespace Magnum::Examples {

struct chunk_coords final {
    std::int16_t x = 0, y = 0;
};

struct global_coords final {
    std::int32_t x = 0, y = 0;
    constexpr chunk_coords chunk() const noexcept;
    constexpr chunk_coords local() const noexcept;
};

constexpr chunk_coords global_coords::chunk() const noexcept {
    constexpr std::uint32_t mask = 0xffff0000u;
    const auto x_ = (std::int16_t)(std::uint16_t)((std::uint32_t)x & mask >> 24),
               y_ = (std::int16_t)(std::uint16_t)((std::uint32_t)y & mask >> 24);
    return {x_, y_};
}

constexpr chunk_coords global_coords::local() const noexcept {
    const auto x_ = (std::uint8_t)x, y_ = (std::uint8_t)y;
    return {x_, y_};
}

} // namespace Magnum::Examples
