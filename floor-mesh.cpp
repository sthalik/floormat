#include "floor-mesh.hpp"
#include "tile-atlas.hpp"
#include "shaders/tile-shader.hpp"
#include "tile.hpp"
#include "chunk.hpp"
#include <Magnum/GL/MeshView.h>

namespace Magnum::Examples {

floor_mesh::floor_mesh()
{
    _mesh.setCount((int)(quad_index_count * _index_data.size()))
         .addVertexBuffer(_positions_buffer, 0, tile_shader::Position{})
         .addVertexBuffer(_vertex_buffer, 0, tile_shader::TextureCoordinates{})
         .setIndexBuffer(_index_buffer, 0, GL::MeshIndexType::UnsignedShort);
}

void floor_mesh::set_tile(tile& x, const local_coords pt)
{
    CORRADE_INTERNAL_ASSERT(x.ground_image);

    const auto idx = pt.to_index();
    auto texcoords = x.ground_image.atlas->texcoords_for_id(x.ground_image.variant);
    for (std::size_t i = 0; i < 4; i++)
        _vertex_data[idx][i] = { texcoords[i] };
}

void floor_mesh::draw(tile_shader& shader, chunk& c)
{
    c.foreach_tile([&](tile& x, std::size_t, local_coords pt) {
      set_tile(x, pt);
    });
    _vertex_buffer.setData(_vertex_data, Magnum::GL::BufferUsage::DynamicDraw);
#if 1
    Magnum::GL::MeshView mesh{ _mesh };
    mesh.setCount(quad_index_count);
    tile_atlas* last_tile_atlas = nullptr;
    c.foreach_tile([&](tile& x, std::size_t i, local_coords) {
      mesh.setIndexRange((int)(i*quad_index_count), 0, quad_index_count*TILE_COUNT - 1);
      if (auto* atlas = x.ground_image.atlas.get(); atlas != last_tile_atlas)
      {
          atlas->texture().bind(0);
          last_tile_atlas = atlas;
      }
      shader.draw(mesh);
    });
#else
    c[0].ground_image.atlas->texture().bind(0);
    shader.draw(_mesh);
#endif
}

static auto make_index_array()
{
    constexpr auto quad_index_count = std::tuple_size_v<decltype(tile_atlas::indices(0))>;
    std::array<std::array<UnsignedShort, quad_index_count>, TILE_COUNT> array; // NOLINT(cppcoreguidelines-pro-type-member-init)

    for (std::size_t i = 0; i < std::size(array); i++)
        array[i] = tile_atlas::indices(i);
    return array;
}

static auto make_floor_positions_array()
{
    std::array<std::array<Vector3, 4>, TILE_COUNT> array;
    constexpr float X = TILE_SIZE[0], Y = TILE_SIZE[1];
    for (std::size_t j = 0, k = 0; j < TILE_MAX_DIM; j++)
        for (std::size_t i = 0; i < TILE_MAX_DIM; i++, k++)
        {
            Vector3 center {(float)(X*i), (float)(Y*j), 0};
            array[k] = { tile_atlas::floor_quad(center, {X, Y}) };
        }
    return array;
}

const auto floor_mesh::_index_data = make_index_array();
const auto floor_mesh::_position_data = make_floor_positions_array();

} // namespace Magnum::Examples
