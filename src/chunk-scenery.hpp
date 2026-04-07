#pragma once
#include "chunk.hpp"
#include <cr/ArrayView.h>
#include <mg/Vector2.h>

namespace floormat {

struct chunk::object_draw_order
{
    object *e;
    uint32_t mesh_idx;
};

struct chunk::scenery_mesh_tuple {
    GL::Mesh& mesh;
    ArrayView<const object_draw_order> array;
    size_t size;
};

struct chunk::scenery_scratch_buffers
{
    Array<object_draw_order>& array;
    Array<Quads::vertexes>& scenery_vertexes;
    Array<Quads::indexes>& scenery_indexes;
};

} // namespace floormat
