#pragma once

#include "tile-defs.hpp"
#include <array>
#include <utility>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Mesh.h>
#include "Magnum/GL/RectangleTexture.h"

namespace Magnum::Examples {

struct tile_shader;

namespace wireframe
{

template<typename T>
concept traits = requires (const T& x) {
    {T::num_vertices} -> std::convertible_to<std::size_t>;
    {x.primitive} -> std::convertible_to<GL::MeshPrimitive>;
    {x.make_vertex_array() } -> std::convertible_to<Containers::ArrayView<const void>>;
    {x.on_draw()} -> std::same_as<void>;
};

struct mesh_base
{
    static GL::RectangleTexture make_constant_texture();
    GL::Buffer _vertex_buffer, _texcoords_buffer;
    GL::RectangleTexture _texture = make_constant_texture();
    GL::Mesh _mesh;

    mesh_base(GL::MeshPrimitive primitive, std::size_t num_vertices);
    void draw(tile_shader& shader);
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
      wireframe::mesh_base{T::primitive, T::num_vertices}
{}

template <wireframe::traits T> void wireframe_mesh<T>::draw(tile_shader& shader, T x)
{
    _vertex_buffer.setData(x.make_vertex_array(), GL::BufferUsage::DynamicDraw);
    mesh_base::draw(shader);
}

} // namespace Magnum::Examples
