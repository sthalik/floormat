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
    {T::num_indexes} -> std::convertible_to<std::size_t>;
    {x.primitive} -> std::convertible_to<GL::MeshPrimitive>;
    {x.make_vertex_array() } -> std::same_as<std::array<Vector3, T::num_vertices>>;
    {x.make_index_array() } -> std::same_as<std::array<UnsignedShort, T::num_indexes>>;
};

struct null final
{
    static constexpr auto primitive = GL::MeshPrimitive::Triangles;
    static constexpr std::size_t num_vertices = 0, num_indexes = 0;
    static GL::RectangleTexture make_constant_texture();
    static std::array<Vector3, 0> make_vertex_array() { return {}; }
    static std::array<UnsignedShort, 0> make_index_array() { return {}; }
};

struct quad final
{
    quad(Vector3 center, Vector2 size);
    constexpr quad() = default;

    static constexpr std::size_t num_vertices = 4, num_indexes = 6;
    static constexpr GL::MeshPrimitive primitive = GL::MeshPrimitive::Triangles;

    using vertex_array = std::array<Vector3, num_vertices>;
    using index_array = std::array<UnsignedShort, num_indexes>;

    vertex_array make_vertex_array() const;
    static index_array make_index_array() ;

private:
    Vector3 center = {};
    Vector2 size = { TILE_SIZE[0], TILE_SIZE[1] };
};

} // namespace wireframe

template<wireframe::traits T>
struct wireframe_mesh final
{
    wireframe_mesh();
    void draw(tile_shader& shader, T traits);

private:
    GL::Buffer _vertex_buffer{std::array<Vector3, T::num_vertices>{}, GL::BufferUsage::DynamicDraw},
               _texcoords_buffer{std::array<Vector2, T::num_vertices>{}, GL::BufferUsage::DynamicDraw},
               _index_buffer{std::array<UnsignedShort , T::num_indexes>{}, GL::BufferUsage::DynamicDraw};
    GL::RectangleTexture _texture = wireframe::null::make_constant_texture();
    GL::Mesh _mesh;
};

extern template struct wireframe_mesh<wireframe::null>;
extern template struct wireframe_mesh<wireframe::quad>;

using wireframe_quad_mesh = wireframe_mesh<wireframe::quad>;

} // namespace Magnum::Examples
