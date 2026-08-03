#include "quad.hpp"

namespace floormat::wireframe {

vertex_array quad::make_vertex_array() const
{
    const auto Sx = size[0]*.5f, Sy = size[1]*.5f;
    const auto Cx_0 = center[0] - Sx, Cx_1 = center[0] + Sx;
    const auto Cy_0 = center[1] - Sy, Cy_1 = center[1] + Sy;
    const auto Cz = center[2];
    return {{
        { Cx_0, Cy_0, Cz },
        { Cx_1, Cy_0, Cz },
        { Cx_1, Cy_1, Cz },
        { Cx_0, Cy_1, Cz },
    }};
}

void mesh_quad::draw(tile_shader& shader, quad x)
{
    draw_closed_polyline(shader, x.make_vertex_array(), x.line_width);
}

} // namespace floormat::wireframe
