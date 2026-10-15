#pragma once
#include "src/object-id.hpp"
#include <cr/BitArrayView.h>
#include <mg/Range.h>

namespace floormat {

// Rebuilt only when drawing, so an event in between can remove the object.
struct clickable final {
    Math::Range2D<unsigned> src;
    Math::Range2D<int> dest;
    BitArrayView bitmask;
    object_id id;
    uint32_t depth;
    uint32_t stride;
    bool mirrored;
};

} // namespace floormat
