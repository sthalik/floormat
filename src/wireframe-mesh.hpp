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

namespace wireframe
{

template<typename T>
concept traits = requires (const T& x) {
    {T::num_vertices} -> std::convertible_to<std::size_t>;
    {x.primitive} -> std::convertible_to<GL::MeshPrimitive>;
    {x.make_vertex_array() } -> std::same_as<std::array<Vector3, T::num_vertices>>;
};


struct null final
{
    static constexpr auto primitive = GL::MeshPrimitive::Triangles;
    static constexpr std::size_t num_vertices = 0;
    static GL::RectangleTexture make_constant_texture();
    static std::array<Vector3, 0> make_vertex_array() { return {}; }
};

struct quad final
{
    quad(Vector3 center, Vector2 size);
    constexpr quad() = default;

    static constexpr std::size_t num_vertices = 4;
    static constexpr GL::MeshPrimitive primitive = GL::MeshPrimitive::LineLoop;

    using vertex_array = std::array<Vector3, num_vertices>;
    vertex_array make_vertex_array() const;

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
    GL::Buffer _vertex_buffer{}, _texcoords_buffer{std::array<Vector2, T::num_vertices>{}};
    GL::RectangleTexture _texture = wireframe::null::make_constant_texture();
    GL::Mesh _mesh;
};

extern template struct wireframe_mesh<wireframe::null>;
extern template struct wireframe_mesh<wireframe::quad>;

using wireframe_quad_mesh = wireframe_mesh<wireframe::quad>;

} // namespace Magnum::Examples
