#pragma once
#include <cr/BitArrayView.h>
#include <mg/Range.h>

namespace floormat {

struct object;

struct clickable final {
    Math::Range2D<unsigned> src;
    Math::Range2D<int> dest;
    BitArrayView bitmask;
    object* e;
    uint32_t stride;
    bool mirrored;
};

} // namespace floormat
