#pragma once
#include <array>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/GL/Mesh.h>

namespace floormat::wireframe {

struct box final
{
    box(Vector3 center, Vector3 size, float line_width);

    static constexpr std::size_t num_vertices = 8, num_indexes = 12*2;
    static constexpr GL::MeshPrimitive primitive = GL::MeshPrimitive::Lines;

    using vertex_array = std::array<Vector3, num_vertices>;
    using index_array = std::array<UnsignedShort, num_indexes>;

    vertex_array make_vertex_array() const;
    static index_array make_index_array();
    void on_draw() const;

private:
    Vector3 center;
    Vector3 size;
    float line_width = 2;
};

} // namespace floormat::wireframe
