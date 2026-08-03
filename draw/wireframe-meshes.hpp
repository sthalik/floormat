#pragma once
#include "compat/defs.hpp"
#include "quad-wall-n.hpp"
#include "quad-wall-w.hpp"
#include "quad.hpp"

namespace floormat::wireframe {

struct meshes final
{
    fm_DISABLE_COPY(meshes);
    fm_DISABLE_MOVE(meshes);

    meshes() = default;

    mesh_quad   quad;
    mesh_wall_n wall_n;
    mesh_wall_w wall_w;
    mesh_quad   rect;
};

} // namespace floormat::wireframe
