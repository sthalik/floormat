#pragma once
#include <array>
#include <mg/Vector3.h>

namespace floormat { struct point; }

namespace floormat::Quads {

struct vertex {
    Vector3 position;
    Vector2 texcoords;
    float depth = -1;
};

using quad = std::array<Vector3, 4>;
using texcoords = std::array<Vector2, 4>;
using indexes = std::array<UnsignedShort, 6>;
using vertexes = std::array<vertex, 4>;
using depths = std::array<float, 4>;

quad floor_quad(Vector3 center, Vector2 size);
indexes quad_indexes(size_t N);
texcoords texcoords_at(Vector2ui pos, Vector2ui size, Vector2ui image_size);

template<bool LR_1 = true, bool LR_2 = true, bool LR_3 = false, bool LR_4 = false>
depths depth_quad(point L, point R, int32_t depth_offset);

} // namespace floormat::Quads
