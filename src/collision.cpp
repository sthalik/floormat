#include "collision.hpp"
#include <bit>

namespace floormat {

void compact_bb_extractor::ExtractBoundingBox(compact_bb* object, BB* bbox)
{
    if constexpr(sizeof(void*) >= 8)
        *bbox = std::bit_cast<loose_quadtree::BoundingBox<std::int16_t>>((void*)object);
}

} // namespace floormat
