#pragma once

#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/GL/Mesh.h>

namespace Magnum::Examples::wireframe {

template<typename T>
struct wireframe_mesh;

struct quad final
{
    quad(Vector3 center, Vector2 size);

    static constexpr std::size_t num_vertices = 4;
    static constexpr GL::MeshPrimitive primitive = GL::MeshPrimitive::LineLoop;

    using vertex_array = std::array<Vector3, num_vertices>;

    vertex_array make_vertex_array() const;
    void on_draw() const;

private:
    Vector3 center;
    Vector2 size;
    float line_width = 2;
};



} // namespace Magnum::Examples::wireframe
