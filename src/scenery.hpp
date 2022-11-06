#pragma once
#include "compat/integer-types.hpp"
#include <memory>

namespace floormat {

enum class rotation : std::uint16_t {
    N, NE, E, SE, S, SW, W, NW,
    COUNT,
};

struct scenery final
{
    using frame_t = std::uint16_t;

    frame_t frame : 12 = (1 << 12) - 1;
    rotation r    : 4  = rotation::N;

    constexpr operator bool() const noexcept;
};

static_assert(sizeof(scenery) == sizeof(std::uint16_t));

constexpr scenery::operator bool() const noexcept
{
    return frame == (1 << 13) - 1;
}

} // namespace floormat
