#pragma once
#include "src/global-coords.hpp"
#include <Corrade/Containers/BitArrayView.h>
#include <Magnum/Math/Range.h>

namespace floormat {

template<typename Atlas, typename T>
struct clickable final {

    Atlas& atlas;
    T& item;
    Math::Range2D<UnsignedInt> src, dest;
    BitArrayView bitmask;
    float depth = 0;
    chunk_coords chunk;
    local_coords pos;
    bool mirrored = false;
};

} // namespace floormat
