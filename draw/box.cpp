#include "box.hpp"
#include "wireframe.hpp"
#include <Magnum/GL/Renderer.h>

namespace floormat::wireframe {

box::box(Vector3 center, Vector3 size, float line_width) :
    center{center}, size{size}, line_width{line_width}
{}

box::vertex_array box::make_vertex_array() const
{
    const auto Sx = size[0]*.5f, Sy = size[1]*.5f, Sz = size[2];
    const auto Cx_0 = center[0] - Sx, Cx_1 = center[0] + Sx;
    const auto Cy_0 = center[1] - Sy, Cy_1 = center[1] + Sy;
    const auto Cz_0 = center[2] + 0,  Cz_1 = center[2] + Sz;
    return {{
        {Cx_0, Cy_0, Cz_0}, // (0) front left  bottom
        {Cx_1, Cy_0, Cz_0}, // (1) front right bottom
        {Cx_0, Cy_1, Cz_0}, // (2) back  left  bottom
        {Cx_1, Cy_1, Cz_0}, // (3) back  right bottom
        {Cx_0, Cy_0, Cz_1}, // (4) front left  top
        {Cx_1, Cy_0, Cz_1}, // (5) front right top
        {Cx_0, Cy_1, Cz_1}, // (6) back  left  top
        {Cx_1, Cy_1, Cz_1}, // (7) back  right top
    }};
}

void box::on_draw() const
{
    mesh_base::set_line_width(line_width);
}

box::index_array box::make_index_array()
{
    return {{
        0, 1,
        0, 2,
        0, 4,
        1, 3,
        1, 5,
        2, 3,
        2, 6,
        3, 7,
        4, 5,
        4, 6,
        5, 7,
        6, 7,
    }};
}

} // namespace floormat::wireframe
