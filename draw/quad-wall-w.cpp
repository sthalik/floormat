#include "quad-wall-w.hpp"

namespace floormat::wireframe {

vertex_array quad_wall_w::make_vertex_array() const
{
    const float x = size[0]*.5f, y = size[1]*.5f, z = size[2];
    const auto cx = center[0], cy = center[1], cz = center[2];
    return {{
        { -x + cx, -y + cy,     cz },
        { -x + cx,  y + cy,     cz },
        { -x + cx,  y + cy, z + cz },
        { -x + cx, -y + cy, z + cz },
    }};
}

void mesh_wall_w::draw(tile_shader& shader, quad_wall_w x)
{
    draw_closed_polyline(shader, x.make_vertex_array(), x.line_width);
}

} // namespace floormat::wireframe
