#include "quad-floor.hpp"
#include "wireframe.hpp"
#include <array>
#include <Magnum/GL/Renderer.h>

namespace floormat::wireframe {

auto quad_floor::make_vertex_array() const -> vertex_array
{
    const float x = size[0]*.5f, y = size[1]*.5f;
    const auto cx = center[0], cy = center[1], cz = center[2];
    return {{
        { -x + cx, -y + cy, cz },
        {  x + cx, -y + cy, cz },
        {  x + cx,  y + cy, cz },
        { -x + cx,  y + cy, cz },
    }};
}

quad_floor::quad_floor(Vector3 center, Vector2 size, float line_width) :
      center(center), size(size), line_width{line_width}
{}

void quad_floor::on_draw() const
{
    mesh_base::set_line_width(line_width);
}

} // namespace floormat::wireframe
