#pragma once

#include <array>
#include <Magnum/Math/Vector3.h>
#include <Magnum/GL/Mesh.h>
#include <Corrade/Containers/ArrayView.h>

namespace floormat::wireframe {

struct quad_wall_n
{
    quad_wall_n(Vector3 center, Vector3 size, float line_width);

    static constexpr size_t num_vertices = 4, num_indexes = 0;
    static constexpr auto primitive = GL::MeshPrimitive::LineLoop;

    using vertex_array = std::array<Vector3, num_vertices>;

    static ArrayView<const void> make_index_array() { return {}; }
    vertex_array make_vertex_array() const;
    void on_draw() const;

private:
    Vector3 center;
    Vector3 size;
    float line_width;
};

} // namespace floormat::wireframe
