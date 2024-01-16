#pragma once
#include "wireframe.hpp"
#include "compat/defs.hpp"
#include "box.hpp"
#include "quad-floor.hpp"
#include "quad-wall-n.hpp"
#include "quad-wall-w.hpp"
#include "quad.hpp"
#include <Magnum/GL/Texture.h>

namespace floormat::wireframe {

struct meshes final
{
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(meshes);
    fm_DECLARE_DELETED_MOVE_ASSIGNMENT(meshes);

    meshes();

    GL::Texture2D _wireframe_texture;
    wireframe_mesh<struct wireframe::quad_floor>  quad;
    wireframe_mesh<struct wireframe::quad_wall_n> wall_n;
    wireframe_mesh<struct wireframe::quad_wall_w> wall_w;
    wireframe_mesh<struct wireframe::box>         box;
    wireframe_mesh<struct wireframe::quad>        rect;
};

} // namespace floormat::wireframe
