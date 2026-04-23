#pragma once
#include <array>
#include <cr/ArrayViewStl.h>
#include <mg/Vector3.h>
#include <mg/Mesh.h>

namespace floormat::wireframe {

struct quad
{
    quad(Vector3 start, Vector2 size, float line_width);

    static constexpr size_t num_vertices = 4, num_indexes = 0;
    static constexpr GL::MeshPrimitive primitive = GL::MeshPrimitive::LineLoop;

    using vertex_array = std::array<Vector3, num_vertices>;
    using index_array = std::array<UnsignedShort, num_indexes>;

    vertex_array make_vertex_array() const;
    static ArrayView<const void> make_index_array() { return {}; }
    void on_draw() const;

private:
    Vector3 center;
    Vector2 size;
    float line_width = 2;
};

} // namespace floormat::wireframe
