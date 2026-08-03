#pragma once
#include "wireframe.hpp"

namespace floormat::wireframe {

struct quad_wall_n
{
    Vector3 center;
    Vector3 size;
    float line_width;

    vertex_array make_vertex_array() const;
};

struct mesh_wall_n
{
    void draw(tile_shader& shader, quad_wall_n x);
};

} // namespace floormat::wireframe
