#include "wireframe-meshes.hpp"

namespace floormat::wireframe {

meshes::meshes() :
    _wireframe_texture{make_constant_texture()},
    quad   {_wireframe_texture},
    wall_n {_wireframe_texture},
    wall_w {_wireframe_texture},
    box    {_wireframe_texture},
    rect   {_wireframe_texture}
{
}

} // namespace floormat::wireframe
