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

using index_type = UnsignedShort; // widen (e.g. UnsignedInt) to raise the per-buffer cap
constexpr inline uint32_t vertexes_per_quad = 4;
constexpr inline uint32_t indexes_per_quad  = 6;

using quad      = std::array<Vector3, vertexes_per_quad>;
using texcoords = std::array<Vector3, vertexes_per_quad>;
using indexes   = std::array<index_type, indexes_per_quad>;
using vertexes  = std::array<vertex, vertexes_per_quad>;
using depths    = std::array<float, vertexes_per_quad>;

// 2^bits in uint64: a size_t accumulator wraps to 0 for UnsignedInt on a 32-bit size_t.
constexpr inline uint64_t max_quads_per_buffer =
    (uint64_t{1} << (sizeof(index_type) * 8)) / vertexes_per_quad;

// Reorders horizontal-plane quads to CCW winding in NDC for GL_CCW face culling
constexpr inline std::array<uint8_t, 4> ccw_order = { 1, 0, 3, 2 };

quad floor_quad(Vector3 center, Vector2 size);
indexes quad_indexes(size_t N);
texcoords texcoords_at(Vector2ui pos, Vector2ui size, Vector2ui image_size);
texcoords texcoords_at(Vector2ui pos, Vector2ui size, Vector2ui image_size, bool mirror, bool rotated);

template<bool LR_1 = true, bool LR_2 = true, bool LR_3 = false, bool LR_4 = false>
depths depth_quad(point L, point R, int32_t depth_offset);

} // namespace floormat::Quads
