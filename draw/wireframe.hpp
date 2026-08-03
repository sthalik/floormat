#pragma once
#include <array>
#include <mg/Vector3.h>

namespace floormat { struct tile_shader; }

namespace floormat::wireframe {

constexpr inline uint32_t num_corners = 4;
using vertex_array = std::array<Vector3, num_corners>;

void draw_closed_polyline(tile_shader& shader, const vertex_array& corners, float line_width);

} // namespace floormat::wireframe
