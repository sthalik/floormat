#pragma once
#include "src/global-coords.hpp"
#include <Corrade/Containers/BitArrayView.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Range.h>

namespace floormat {

struct object;

struct clickable final {
    Math::Range2D<unsigned> src;
    Math::Range2D<int> dest;
    BitArrayView bitmask;
    object* e;
    float depth, slope;
    Vector2s bb_min, bb_max; // debug only
    uint32_t stride;
    bool mirrored;
};

} // namespace floormat
