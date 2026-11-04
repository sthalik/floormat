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

using index_type = UnsignedInt; // widen (e.g. UnsignedInt) to raise the per-buffer cap

constexpr inline u32 vertexes_per_quad = 4;
constexpr inline u32 indexes_per_quad  = 6;

using quad      = std::array<Vector3, vertexes_per_quad>;
using texcoords = std::array<Vector3, vertexes_per_quad>;
using indexes   = std::array<index_type, indexes_per_quad>;
using vertexes  = std::array<vertex, vertexes_per_quad>;
using depths    = std::array<float, vertexes_per_quad>;

// 2^bits in uint64: a usize accumulator wraps to 0 for UnsignedInt on a 32-bit usize.
constexpr inline u64 max_quads_per_buffer =
    (u64{1} << (sizeof(index_type) * 8)) / vertexes_per_quad;

// Reorders horizontal-plane quads to CCW winding in NDC for GL_CCW face culling
constexpr inline std::array<u8, 4> ccw_order = { 1, 0, 3, 2 };

quad floor_quad(Vector3 center, Vector2 size);
indexes quad_indexes(usize N);
texcoords texcoords_at(Vector2ui pos, Vector2ui size, Vector2ui image_size);
texcoords texcoords_at(Vector2ui pos, Vector2ui size, Vector2ui image_size, bool mirror, bool rotated);

template<bool LR_1 = true, bool LR_2 = true, bool LR_3 = false, bool LR_4 = false>
depths depth_quad(point L, point R, i32 depth_offset);

vertexes make_vertexes(const quad& q, const texcoords& tc, const depths& d, Vector3 center = {});
vertexes make_vertexes(const quad& q, const texcoords& tc, float depth, Vector3 center = {});

} // namespace floormat::Quads
