#pragma once
#include <array>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>

namespace floormat::Quads {

using quad = std::array<Vector3, 4>;
using texcoords = std::array<Vector2, 4>;
using indexes = std::array<UnsignedShort, 6>;

quad floor_quad(Vector3 center, Vector2 size);
quad wall_quad_N(Vector3 center, Vector3 size);
quad wall_quad_W(Vector3 center, Vector3 size);
indexes quad_indexes(size_t N);
texcoords texcoords_at(Vector2ui pos, Vector2ui size, Vector2ui image_size);

} // namespace floormat::Quads
