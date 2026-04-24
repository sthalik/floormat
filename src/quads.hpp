#pragma once
#include <array>
#include <mg/Vector3.h>

namespace floormat { struct point; }

namespace floormat::Quads {

struct vertex {
    Vector3 position;
    Vector3 texcoords;
    float depth;
};

using quad = std::array<Vector3, 4>;
using texcoords = std::array<Vector3, 4>;
using indexes = std::array<UnsignedShort, 6>;
using vertexes = std::array<vertex, 4>;
using depths = std::array<float, 4>;

// Reorders horizontal-plane quads to CCW winding in NDC for GL_CCW face culling
constexpr inline std::array<uint8_t, 4> ccw_order = { 1, 0, 3, 2 };

quad floor_quad(Vector3 center, Vector2 size);
indexes quad_indexes(size_t N);
texcoords texcoords_at(Vector2ui pos, Vector2ui size, Vector2ui image_size);
texcoords texcoords_at(Vector2ui pos, Vector2ui size, Vector2ui image_size, bool mirror, bool rotated);

template<bool LR_1 = true, bool LR_2 = true, bool LR_3 = false, bool LR_4 = false>
depths depth_quad(point L, point R, int32_t depth_offset);

} // namespace floormat::Quads
