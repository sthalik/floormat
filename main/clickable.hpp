#pragma once
#include "src/global-coords.hpp"
#include <Corrade/Containers/BitArrayView.h>
#include <Magnum/Math/Range.h>

namespace floormat {

struct clickable final {
    Math::Range2D<unsigned> src;
    Math::Range2D<int> dest;
    BitArrayView bitmask;
    float depth = 0;
    std::uint32_t stride;
    chunk_coords chunk;
    local_coords pos;
    bool mirrored;
};

} // namespace floormat
