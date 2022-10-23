#pragma once

#include "tile-defs.hpp"
#include <array>
#include <utility>
#include <Corrade/Containers/ArrayView.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Mesh.h>
#include "Magnum/GL/Texture.h"

namespace floormat {

struct tile_shader;

namespace wireframe
{

template<typename T>
concept traits = requires (const T& x) {
    {T::num_vertices} -> std::convertible_to<std::size_t>;
    {T::num_indexes} -> std::convertible_to<std::size_t>;
    {x.primitive} -> std::convertible_to<GL::MeshPrimitive>;
    {x.make_vertex_array() } -> std::convertible_to<Containers::ArrayView<const void>>;
    {T::make_index_array() } -> std::convertible_to<Containers::ArrayView<const void>>;
    {x.on_draw()} -> std::same_as<void>;
};

struct mesh_base
{
    static GL::Texture2D make_constant_texture();
    GL::Buffer _vertex_buffer{{}, GL::BufferUsage::DynamicDraw}, _texcoords_buffer, _index_buffer;
    GL::Texture2D _texture = make_constant_texture();
    GL::Mesh _mesh;

    mesh_base(GL::MeshPrimitive primitive, Containers::ArrayView<const void> index_data,
              std::size_t num_vertices, std::size_t num_indexes);
    void draw(tile_shader& shader);
    void set_subdata(Containers::ArrayView<const void> array);
};

} // namespace wireframe

template<wireframe::traits T>
struct wireframe_mesh final : private wireframe::mesh_base
{
    wireframe_mesh();
    void draw(tile_shader& shader, T traits);
};

template<wireframe::traits T>
wireframe_mesh<T>::wireframe_mesh() :
      wireframe::mesh_base{T::primitive, T::make_index_array(), T::num_vertices, T::num_indexes}
{
}

template <wireframe::traits T> void wireframe_mesh<T>::draw(tile_shader& shader, T x)
{
    //_vertex_buffer.setData({nullptr, sizeof(Vector3) * T::num_vertices}, GL::BufferUsage::DynamicDraw); // orphan the buffer
    set_subdata(x.make_vertex_array());
    x.on_draw();
    mesh_base::draw(shader);
}

} // namespace floormat
