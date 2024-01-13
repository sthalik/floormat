#include "wireframe-meshes.hpp"

namespace floormat::wireframe {

meshes::meshes() :
    _wireframe_texture{make_constant_texture()},
    _wireframe_quad   {_wireframe_texture},
    _wireframe_wall_n {_wireframe_texture},
    _wireframe_wall_w {_wireframe_texture},
    _wireframe_box    {_wireframe_texture},
    _wireframe_rect   {_wireframe_texture}
{
}

} // namespace floormat::wireframe
