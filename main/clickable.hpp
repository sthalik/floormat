#pragma once
#include "src/global-coords.hpp"
#include <memory>
#include <Corrade/Containers/BitArrayView.h>
#include <Magnum/Math/Range.h>

namespace floormat {

struct entity;

struct clickable final {
    Math::Range2D<unsigned> src;
    Math::Range2D<int> dest;
    BitArrayView bitmask;
    entity* e;
    float depth;
    uint32_t stride;
    bool mirrored;
};

} // namespace floormat
