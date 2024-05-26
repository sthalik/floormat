#pragma once
#include "object.hpp"
#include <array>

namespace floormat {

struct hole_proto final : object_proto
{
    bool affects_render  : 1 = true;
    bool affects_physics : 1 = false;
};

struct hole : object
{
    bool affects_render  : 1 = true;
    bool affects_physics : 1 = false;

    hole(object_id id, class chunk& c, const hole_proto& proto);
};

struct cut_rectangle_result
{
    struct bbox { Vector2i position; Vector2ub bbox_size; };
    struct rect { Vector2i min, max; };

    uint8_t size = 0;
    std::array<rect, 8> array;

    operator ArrayView<const bbox>() const;
};

cut_rectangle_result cut_rectangle(cut_rectangle_result::bbox input, cut_rectangle_result::bbox hole);

} // namespace floormat
