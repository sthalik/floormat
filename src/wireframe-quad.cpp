#include "wireframe-quad.hpp"
#include <array>
#include <Magnum/GL/Renderer.h>

namespace Magnum::Examples::wireframe {

quad::vertex_array quad::make_vertex_array() const
{
    const float X = size[0]*.5f, Y = size[1]*.5f;
    return {{
        { -X + center[0], -Y + center[1], center[2] },
        {  X + center[0], -Y + center[1], center[2] },
        {  X + center[0],  Y + center[1], center[2] },
        { -X + center[0],  Y + center[1], center[2] },
    }};
}

quad::quad(Vector3 center, Vector2 size) : center(center), size(size) {}

void quad::on_draw() const
{
    GL::Renderer::setLineWidth(line_width);
}

} // namespace Magnum::Examples::wireframe
