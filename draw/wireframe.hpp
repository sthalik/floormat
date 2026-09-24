#pragma once

namespace floormat { struct tile_shader; }

namespace floormat::wireframe {

void draw_quad(tile_shader& shader, Vector3 center, Vector2 size, float line_width);
void draw_wall_n(tile_shader& shader, Vector3 center, Vector3 size, float line_width);
void draw_wall_w(tile_shader& shader, Vector3 center, Vector3 size, float line_width);

} // namespace floormat::wireframe
