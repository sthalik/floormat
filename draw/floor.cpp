#include "floor.hpp"
#include "shaders/tile.hpp"
#include "tile.hpp"
#include "chunk.hpp"
#include "tile-atlas.hpp"
#include <Magnum/GL/MeshView.h>

namespace floormat {

constexpr auto quad_index_count = 6;

floor_mesh::floor_mesh()
{
    _mesh.setCount((int)(quad_index_count * TILE_COUNT))
         .addVertexBuffer(_positions_buffer, 0, tile_shader::Position{})
         .addVertexBuffer(_vertex_buffer, 0, tile_shader::TextureCoordinates{})
         .setIndexBuffer(_index_buffer, 0, GL::MeshIndexType::UnsignedShort);
}

void floor_mesh::set_tile(quad_data& data, tile& x)
{
    if (x.ground)
    {
        auto texcoords = x.ground.atlas->texcoords_for_id(x.ground.variant);
        for (size_t i = 0; i < 4; i++)
            data[i] = { texcoords[i] };
    }
    else
        for (size_t i = 0; i < 4; i++)
            data[i] = {};
}

void floor_mesh::draw(tile_shader& shader, chunk& c)
{
    //_vertex_buffer.setData({nullptr, sizeof(quad_data) * TILE_COUNT}, Magnum::GL::BufferUsage::DynamicDraw); // orphan the buffer
    std::array<quad_data, TILE_COUNT> data;
    for (auto& [x, idx, pt] : c) {
        set_tile(data[idx], x);
    }
    _vertex_buffer.setSubData(0, data);
    Magnum::GL::MeshView mesh{_mesh};

    tile_atlas* last_atlas = nullptr;
    std::size_t last_pos = 0;

    const auto do_draw = [&](std::size_t i, tile_atlas* atlas) {
        if (atlas == last_atlas)
            return;
        if (auto len = i - last_pos; last_atlas != nullptr && len > 0)
        {
            last_atlas->texture().bind(0);
            mesh.setCount((int)(quad_index_count * len));
            mesh.setIndexRange((int)(last_pos*quad_index_count), 0, quad_index_count*TILE_COUNT - 1);
            shader.draw(mesh);
        }
        last_atlas = atlas;
        last_pos = i;
    };

    for (auto& [x, i, pt] : c)
        do_draw(i, x.ground.atlas.get());
    do_draw(TILE_COUNT, nullptr);
}

std::array<std::array<UnsignedShort, 6>, TILE_COUNT> floor_mesh::make_index_array()
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    std::array<std::array<UnsignedShort, quad_index_count>, TILE_COUNT> array;
    for (std::size_t i = 0; i < std::size(array); i++)
        array[i] = tile_atlas::indices(i);
    return array;
}

std::array<std::array<Vector3, 4>, TILE_COUNT> floor_mesh::make_position_array()
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

} // namespace floormat
