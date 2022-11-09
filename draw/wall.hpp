#pragma once

#include "tile-defs.hpp"
#include "anim.hpp"
#include <array>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Buffer.h>

namespace floormat {

struct tile_shader;
struct chunk;
struct tile_ref;
struct tile_image_ref;

struct wall_mesh final
{
    wall_mesh();
    void draw(tile_shader& shader, chunk& c);

private:
    static constexpr auto COUNT1 = TILE_MAX_DIM*2, COUNT = COUNT1 * COUNT1;

    struct vertex final {
        Vector2 texcoords;
    };

    struct constant final {
        Vector3 position;
        float depth = -1;
    };

    using quad = std::array<vertex, 4>;
    using vertex_array = std::array<quad, COUNT>;
    using texture_array = std::array<GL::Texture2D*, COUNT>;

    anim_mesh _anim_mesh;

    static void maybe_add_tile(vertex_array& data, texture_array& textures, tile_ref x, std::size_t pos);
    static void add_wall(vertex_array& data, texture_array& textures, const tile_image_ref& img, std::size_t pos);

    GL::Mesh _mesh;
    GL::Buffer _vertex_buffer{vertex_array{}, Magnum::GL::BufferUsage::DynamicDraw},
               _index_buffer{make_index_array()},
               _constant_buffer{make_constant_array()};
    static std::array<std::array<UnsignedShort, 6>, COUNT> make_index_array();
    static std::array<std::array<constant, 4>, COUNT> make_constant_array();
};

} // namespace floormat
