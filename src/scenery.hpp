#pragma once
#include "compat/integer-types.hpp"
#include <memory>

namespace floormat {

struct anim_atlas;

struct scenery final
{
    enum class rotation : std::uint16_t {
        N, NE, E, SE, S, SW, W, NW,
    };

    rotation r          : 3  = rotation::N;
    std::uint16_t frame : 13 = 0;
};

} // namespace floormat
