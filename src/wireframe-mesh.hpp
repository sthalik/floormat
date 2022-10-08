#pragma once

#include "tile-atlas.hpp"
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

namespace wireframe_traits {

template<typename T>
concept traits = requires (const T& x) {
    {T::num_vertices} -> std::convertible_to<std::size_t>;
    {T::num_indices} -> std::convertible_to<std::size_t>;
    {x.primitive} -> std::convertible_to<GL::MeshPrimitive>;
    {x.make_vertex_positions_array() } -> std::same_as<std::array<Vector3, T::num_vertices>>;
    {x.make_index_array() } -> std::same_as<std::array<UnsignedShort, T::num_indices>>;
};


struct null final
{
    static constexpr auto primitive = GL::MeshPrimitive::Triangles;
    static constexpr std::size_t num_vertices = 0, num_indices = 0;
    static GL::RectangleTexture make_constant_texture();
    static std::array<Vector3, 0> make_vertex_positions_array() { return {}; }
    static std::array<UnsignedShort, 0> make_index_array() { return {}; }
};

struct quad final
{
    quad(Vector3 center, Vector2 size = {TILE_SIZE[0], TILE_SIZE[1]});
    constexpr quad() = default;

    static constexpr std::size_t num_vertices = 4;
    static constexpr std::size_t num_indices = 0;
    static constexpr GL::MeshPrimitive primitive = GL::MeshPrimitive::LineLoop;

    using vertex_array = std::array<Vector3, num_vertices>;
    using index_array = std::array<UnsignedShort, num_indices>;

    vertex_array make_vertex_positions_array() const;
    static index_array make_index_array() { return {}; }

private:
    Vector3 center = {};
    Vector2 size = { TILE_SIZE[0], TILE_SIZE[1] };
};

} // namespace wireframe_traits


template<wireframe_traits::traits T>
struct wireframe_mesh final
{
    wireframe_mesh();
    void draw(tile_shader& shader, T traits);

private:
    GL::Buffer _positions_buffer{},
               _texcoords_buffer{std::array<Vector2, T::num_vertices>{}},
               _index_buffer{T::num_indices == 0 ? GL::Buffer{NoCreate} : GL::Buffer{}};
    GL::RectangleTexture _texture = wireframe_traits::null::make_constant_texture();
    GL::Mesh _mesh;
};

extern template struct wireframe_mesh<wireframe_traits::null>;
extern template struct wireframe_mesh<wireframe_traits::quad>;

using wireframe_quad_mesh = wireframe_mesh<wireframe_traits::quad>;

} // namespace Magnum::Examples
