#pragma once
#include "chunk.hpp"
#include <Corrade/Containers/Array.h>
#include <Magnum/Math/Vector2.h>

namespace floormat {

struct chunk::topo_sort_data
{
    enum m : uint8_t { mode_none, mode_static, mode_character,  };

    const entity* in = nullptr;
    Vector2i min, max, center;
    float slope = 0, ord;
    m mode : 2 = mode_none;
    uint8_t visited : 1 = false;

    bool intersects(const topo_sort_data& other) const;
};
struct chunk::draw_entity
{
    const entity *e;
    float ord;
    topo_sort_data data;
};
struct chunk::scenery_mesh_tuple {
    GL::Mesh& mesh;
    ArrayView<draw_entity> array;
    size_t size;
};

} // namespace floormat
