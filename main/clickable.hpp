#pragma once
#include "src/object-id.hpp"
#include <cr/BitArrayView.h>
#include <cr/Optional.h>
#include <mg/Range.h>

namespace floormat {

struct object;

// The overlay's array is rebuilt only when drawing, so an event in between can remove the object.
struct clickable final {
    Math::Range2D<unsigned> src;
    Math::Range2D<int> dest;
    BitArrayView bitmask;
    object_id id;
    u32 depth;
    u32 stride;
    bool mirrored;
};

bool clickable_hit_test(const clickable& c, Vector2i pixel);
Optional<clickable> clickable_for_object(const object& e, Vector2i win_size, Vector2i camera);

} // namespace floormat
