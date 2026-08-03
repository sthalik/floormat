#pragma once
#include "wireframe.hpp"

namespace floormat::wireframe {

struct quad
{
    Vector3 center;
    Vector2 size;
    float line_width;

    vertex_array make_vertex_array() const;
};

struct mesh_quad
{
    void draw(tile_shader& shader, quad x);
};

} // namespace floormat::wireframe
