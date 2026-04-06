#pragma once
#include <array>

namespace floormat { struct point; }

namespace floormat::Quads {

using quad = std::array<Vector3, 4>;
using texcoords = std::array<Vector2, 4>;
using indexes = std::array<UnsignedShort, 6>;
using depths = std::array<float, 4>;

quad floor_quad(Vector3 center, Vector2 size);
indexes quad_indexes(size_t N);
texcoords texcoords_at(Vector2ui pos, Vector2ui size, Vector2ui image_size);

template<bool LR_1 = true, bool LR_2 = true, bool LR_3 = false, bool LR_4 = false>
depths depth_quad(point L, point R, int32_t depth_offset);

} // namespace floormat::Quads
