#include "wall-mesh.hpp"
#include "tile-atlas.hpp"
#include "shaders/tile-shader.hpp"
#include "chunk.hpp"
#include <Magnum/GL/MeshView.h>

namespace Magnum::Examples {

#if 0
static auto make_wall_positions_array()
{
    constexpr float X = TILE_SIZE[0], Y = TILE_SIZE[1], Z = TILE_SIZE[2];
    Vector3 center {(float)(X*i), (float)(Y*j), 0};
    array[k] = { tile_atlas::wall_quad_N(center, {X, Y, Z}) };
    array[k+1] = { tile_atlas::wall_quad_W(center, {X, Y, Z}) };
}
#endif

wall_mesh::wall_mesh()
{
    _mesh.setCount((int)(_index_data.size() * _index_data[0].size()))
         .addVertexBuffer(_vertex_buffer, 0, tile_shader::TextureCoordinates{}, tile_shader::Position{})
         .setIndexBuffer(_index_buffer, 0, GL::MeshIndexType::UnsignedShort);
    CORRADE_INTERNAL_ASSERT(_mesh.isIndexed());
}

void wall_mesh::add_wall(vertex_array& data, std::size_t& pos_, tile_image& img, const position_array& positions)
{
    const auto pos = pos_++;
    CORRADE_INTERNAL_ASSERT(pos < data.size());
    auto texcoords = img.atlas->texcoords_for_id(img.variant);
    for (std::size_t i = 0; i < 4; i++)
        data[pos][i] = { texcoords[i], positions[i] };
}

void wall_mesh::add_tile(vertex_array& data, std::size_t& pos, tile& x, local_coords pt)
{
    constexpr float X = TILE_SIZE[0], Y = TILE_SIZE[1], Z = TILE_SIZE[2];
    constexpr Vector3 size = {X, Y, Z};
    Vector3 center{(float)(X*pt.x), (float)(Y*pt.y), 0};

    if (auto& wall = x.wall_north; wall.atlas)
        add_wall(data, pos, wall, tile_atlas::wall_quad_N(center, size));
    if (auto& wall = x.wall_west; wall.atlas)
        add_wall(data, pos, wall, tile_atlas::wall_quad_W(center, size));
}

void wall_mesh::draw(tile_shader& shader, chunk& c)
{
    {
        vertex_array data;
        std::size_t pos = 0;
        c.foreach_tile([&](tile& x, std::size_t, local_coords pt) {
          add_tile(data, pos, x, pt);
        });
        _vertex_buffer.setData(data, Magnum::GL::BufferUsage::DynamicDraw);
    }
    {
        tile_atlas* last_tile_atlas = nullptr;
        Magnum::GL::MeshView mesh{_mesh};
        mesh.setCount(quad_index_count);
        c.foreach_tile([&](tile& x, std::size_t i, local_coords) {
          mesh.setIndexRange((int)(i*quad_index_count), 0, quad_index_count*COUNT - 1);
          if (auto* atlas = x.ground_image.atlas.get(); atlas != last_tile_atlas)
          {
              atlas->texture().bind(0);
              last_tile_atlas = atlas;
          }
          shader.draw(mesh);
        });
    }
}

decltype(wall_mesh::_index_data) wall_mesh::make_index_array()
{
    constexpr auto quad_index_count = std::tuple_size_v<decltype(tile_atlas::indices(0))>;
    std::array<std::array<UnsignedShort, quad_index_count>, COUNT> array; // NOLINT(cppcoreguidelines-pro-type-member-init)

    for (std::size_t i = 0; i < std::size(array); i++)
        array[i] = tile_atlas::indices(i);
    return array;
}

const auto wall_mesh::_index_data = wall_mesh::make_index_array();

} // namespace Magnum::Examples
