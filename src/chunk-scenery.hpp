#pragma once
#include "chunk.hpp"
#include <Corrade/Containers/Array.h>
#include <Magnum/Math/Vector2.h>

namespace floormat {

struct chunk::topo_sort_data
{
    enum m : uint8_t { mode_none, mode_static, mode_character,  };

    object* in = nullptr;
    Vector2i min, max, center;
    uint32_t in_mesh_idx;
    float slope = 0, ord;
    Vector2s bb_min = {}, bb_max = {};
    m mode : 2 = mode_none;
    uint8_t visited : 1 = false;

    bool intersects(const topo_sort_data& other) const;
};

struct chunk::object_draw_order
{
    object *e;
    uint32_t mesh_idx;
    float ord;
    topo_sort_data data;
};

struct chunk::scenery_mesh_tuple {
    GL::Mesh& mesh;
    ArrayView<const object_draw_order> array;
    size_t size;
};

struct chunk::scenery_scratch_buffers
{
    Array<object_draw_order>& array;
    Array<std::array<vertex, 4>>& scenery_vertexes;
    Array<std::array<UnsignedShort, 6>>& scenery_indexes;
};

} // namespace floormat
