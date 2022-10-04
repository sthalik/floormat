#pragma once

#include "tile.hpp"
#include <array>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Buffer.h>

namespace Magnum::Examples {

struct tile_shader;
struct chunk;

struct wall_mesh final
{
    wall_mesh();
    void draw(tile_shader& shader, chunk& c);

private:
    static constexpr auto COUNT = TILE_MAX_DIM*2 * TILE_MAX_DIM*2;
    static constexpr auto quad_index_count = 6;

    using texcoords_array = std::array<Vector2, 4>;
    using position_array = std::array<Vector3, 4>;

    struct vertex final {
        typename texcoords_array::value_type texcoords;
        typename position_array::value_type position;
    };

    using quad = std::array<vertex, 4>;
    using vertex_array = std::array<quad, COUNT>;
    using texture_array = std::array<GL::Texture2D*, COUNT>;

    static void maybe_add_tile(vertex_array& data, texture_array& textures, std::size_t& pos,
                         tile& x, local_coords pt);
    static void add_wall(vertex_array& data, texture_array& textures, std::size_t& pos,
                         tile_image& img, const position_array& positions);

    GL::Mesh _mesh;
    GL::Buffer _vertex_buffer{vertex_array{}, Magnum::GL::BufferUsage::DynamicDraw},
               _index_buffer{_index_data, Magnum::GL::BufferUsage::StaticDraw};

    static const std::array<std::array<UnsignedShort, 6>, COUNT> _index_data;
    static decltype(_index_data) make_index_array();
};

} // namespace Magnum::Examples
