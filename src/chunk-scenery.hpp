#pragma once
#include "chunk.hpp"
#include <Corrade/Containers/Array.h>
#include <Magnum/Math/Vector2.h>

namespace floormat {

struct chunk::topo_sort_data
{
    Vector2i min, max, center;
    float slope = 0, ord;
    bool visited : 1 = false;
    bool is_character : 1;

    bool intersects(const topo_sort_data& other) const;
    friend bool operator<(const topo_sort_data& a, const topo_sort_data& b);
};
struct chunk::draw_entity
{
    entity* e;
    float ord;
    topo_sort_data data;
};
struct chunk::scenery_mesh_tuple {
    GL::Mesh& mesh;
    ArrayView<draw_entity> array;
    size_t size;
};

} // namespace floormat
