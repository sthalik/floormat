#include "quad-wall-n.hpp"
#include <array>
#include <Magnum/GL/Renderer.h>

namespace floormat::wireframe {

auto quad_wall_n::make_vertex_array() const -> vertex_array
{
    const float x = size[0]*.5f, y = size[1]*.5f, z = size[2];
    const auto cx = center[0], cy = center[1], cz = center[2];
    return {{
        { -x + cx, -y + cy,     cz },
        {  x + cx, -y + cy,     cz },
        {  x + cx, -y + cy, z + cz },
        { -x + cx, -y + cy, z + cz },
    }};
}

quad_wall_n::quad_wall_n(Vector3 center, Vector3 size, float line_width) :
      center(center), size(size), line_width{line_width}
{}

void quad_wall_n::on_draw() const
{
    GL::Renderer::setLineWidth(line_width);
}

} // namespace floormat::wireframe
