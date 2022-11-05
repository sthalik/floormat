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

    frame_t frame : 13 = 0;
    rotation r    : 3  = rotation::N;
};

static_assert(sizeof(scenery) == sizeof(std::uint16_t));

} // namespace floormat
