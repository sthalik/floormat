#pragma once

#include "tile-defs.hpp"
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Mesh.h>
#include "Magnum/GL/Texture.h"

namespace floormat {

struct tile_shader;

namespace wireframe {

template<typename T>
concept traits = requires (const T& x) {
    {T::num_vertices} -> std::convertible_to<size_t>;
    {T::num_indexes} -> std::convertible_to<size_t>;
    {x.primitive} -> std::convertible_to<GL::MeshPrimitive>;
    {x.make_vertex_array() } -> std::convertible_to<ArrayView<const void>>;
    {T::make_index_array() } -> std::convertible_to<ArrayView<const void>>;
    {x.on_draw()} -> std::same_as<void>;
};

GL::Texture2D make_constant_texture();

struct mesh_base {
    static void set_line_width(float width);

protected:
    GL::Buffer _vertex_buffer{{}, GL::BufferUsage::DynamicDraw}, _constant_buffer, _index_buffer;
    GL::Texture2D* _texture;
    GL::Mesh _mesh;

    mesh_base(GL::MeshPrimitive primitive, ArrayView<const void> index_data,
              size_t num_vertices, size_t num_indexes, GL::Texture2D* texture);
    void draw(tile_shader& shader);
    void set_subdata(ArrayView<const void> array);
};

} // namespace wireframe

template<wireframe::traits T>
struct wireframe_mesh final : private wireframe::mesh_base
{
    wireframe_mesh(GL::Texture2D& constant_texture);
    void draw(tile_shader& shader, T traits);
};

template<wireframe::traits T>
wireframe_mesh<T>::wireframe_mesh(GL::Texture2D& constant_texture) :
    wireframe::mesh_base{T::primitive, T::make_index_array(), T::num_vertices, T::num_indexes, &constant_texture}
{
}

template <wireframe::traits T> void wireframe_mesh<T>::draw(tile_shader& shader, T x)
{
    //_texcoord_buffer.setData({nullptr, sizeof(Vector3) * T::num_vertices}, GL::BufferUsage::DynamicDraw); // orphan the buffer
    set_subdata(x.make_vertex_array());
    x.on_draw();
    mesh_base::draw(shader);
}

} // namespace floormat
