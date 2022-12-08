#include "quad.hpp"
#include "wireframe.hpp"
#include <Magnum/GL/Renderer.h>

namespace floormat::wireframe {

quad::quad(Vector3 center, Vector2 size, float line_width) :
    center{center}, size{size}, line_width{line_width}
{}

quad::vertex_array quad::make_vertex_array() const
{
    const auto Sx = size[0]*.5f, Sy = size[1]*.5f;
    const auto Cx_0 = center[0] - Sx, Cx_1 = center[0] + Sx;
    const auto Cy_0 = center[1] - Sy, Cy_1 = center[1] + Sy;
    const auto Cz = center[2] + 0;
    return {{
        { Cx_0, Cy_0, Cz },
        { Cx_1, Cy_0, Cz },
        { Cx_1, Cy_1, Cz },
        { Cx_0, Cy_1, Cz },
    }};
}

void quad::on_draw() const
{
    mesh_base::set_line_width(line_width);
}

} // namespace floormat::wireframe
