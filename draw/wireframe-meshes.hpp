#pragma once
#include "wireframe.hpp"
#include "compat/defs.hpp"
#include "box.hpp"
#include "quad-floor.hpp"
#include "quad-wall-n.hpp"
#include "quad-wall-w.hpp"
#include "quad.hpp"
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/GL/Texture.h>

namespace floormat::wireframe {

struct meshes final
{
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(meshes);
    fm_DECLARE_DELETED_MOVE_ASSIGNMENT(meshes);

    meshes();

    GL::Texture2D _wireframe_texture;
    wireframe_mesh<wireframe::quad_floor>  _wireframe_quad   {_wireframe_texture};
    wireframe_mesh<wireframe::quad_wall_n> _wireframe_wall_n {_wireframe_texture};
    wireframe_mesh<wireframe::quad_wall_w> _wireframe_wall_w {_wireframe_texture};
    wireframe_mesh<wireframe::box>         _wireframe_box    {_wireframe_texture};
    wireframe_mesh<wireframe::quad>        _wireframe_rect   {_wireframe_texture};
};

} // namespace floormat::wireframe
