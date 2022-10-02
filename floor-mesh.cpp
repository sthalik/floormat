#include "floor-mesh.hpp"
#include "tile-atlas.hpp"
#include "tile-shader.hpp"
#include "tile.hpp"
#include "chunk.hpp"

namespace Magnum::Examples {

floor_mesh::floor_mesh()
{
    _floor_mesh.setCount((int)(quad_index_count * _index_data.size()))
               .addVertexBuffer(_positions_buffer, 0, tile_shader::Position{})
               .addVertexBuffer(_vertex_buffer, 0, tile_shader::TextureCoordinates{})
               .setIndexBuffer(_index_buffer, 0, GL::MeshIndexType::UnsignedShort);
}

void floor_mesh::set_tile(tile& x, const local_coords pt)
{
    CORRADE_INTERNAL_ASSERT(x.ground_image);

    constexpr float X = TILE_SIZE[0], Y = TILE_SIZE[1];
    const auto idx = pt.to_index();
    auto texcoords = x.ground_image.atlas->texcoords_for_id(x.ground_image.variant);
    //auto positions = img.atlas->floor_quad(center, { TILE_SIZE[0], TILE_SIZE[1] });
    for (std::size_t i = 0; i < 4; i++)
        _vertex_data[idx][i] = { texcoords[i] };
    x.ground_image.atlas->texture().bind(0); // TODO
}

void floor_mesh::draw(tile_shader& shader)
{
    _vertex_buffer.setData(_vertex_data, Magnum::GL::BufferUsage::DynamicDraw);
    shader.draw(_floor_mesh);
}

static auto make_index_array()
{
    constexpr auto quad_index_count = std::tuple_size_v<decltype(tile_atlas::indices(0))>;
    std::array<std::array<UnsignedShort, quad_index_count>, TILE_COUNT> array; // NOLINT(cppcoreguidelines-pro-type-member-init)

    for (std::size_t i = 0, k = 0; i < std::size(array); i++)
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
