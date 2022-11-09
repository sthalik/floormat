#include "wall.hpp"
#include "tile-atlas.hpp"
#include "shaders/tile.hpp"
#include "chunk.hpp"
#include "tile-image.hpp"
#include "anim-atlas.hpp"
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/MeshView.h>

namespace floormat {

constexpr auto quad_index_count = 6;

wall_mesh::wall_mesh()
{
    _mesh.setCount((int)(quad_index_count * COUNT))
         .addVertexBuffer(_vertex_buffer, 0, tile_shader::TextureCoordinates{})
         .addVertexBuffer(_positions_buffer, 0, tile_shader::Position{})
         .setIndexBuffer(_index_buffer, 0, GL::MeshIndexType::UnsignedShort);
    CORRADE_INTERNAL_ASSERT(_mesh.isIndexed());
}

void wall_mesh::add_wall(vertex_array& data, texture_array& textures, const tile_image_ref& img, std::size_t pos)
{
    CORRADE_INTERNAL_ASSERT(pos < data.size());
    auto texcoords = img.atlas->texcoords_for_id(img.variant);
    for (std::size_t i = 0; i < 4; i++)
    {
        data[pos][i] = { texcoords[i] };
        textures[pos] = &img.atlas->texture();
    }
}

void wall_mesh::maybe_add_tile(vertex_array& data, texture_array& textures, tile_ref x, std::size_t pos)
{
    if (auto wall = x.wall_north(); wall.atlas)
        add_wall(data, textures, wall, pos * 2 + 0);
    if (auto wall = x.wall_west(); wall.atlas)
        add_wall(data, textures, wall, pos * 2 + 1);
}

void wall_mesh::draw(tile_shader& shader, chunk& c)
{
    //_texcoord_buffer.setData({nullptr, sizeof(vertex_array)}, Magnum::GL::BufferUsage::DynamicDraw); // orphan the buffer
    texture_array textures = {};
    {
        vertex_array data;
        for (auto [x, idx, pt] : c) {
          maybe_add_tile(data, textures, x, idx);
        }
        _vertex_buffer.setSubData(0, data);
    }

    const GL::Texture2D* last_texture = nullptr;
    Magnum::GL::MeshView mesh{_mesh};
    for (std::size_t idx = 0; idx < TILE_COUNT; idx++)
    {
        for (std::size_t i = idx*2; i <= idx*2+1; i++)
          if (auto* const tex = textures[i]; tex)
          {
              mesh.setCount(quad_index_count);
              mesh.setIndexRange((int)(i*quad_index_count), 0, quad_index_count*COUNT - 1);
              if (tex != last_texture)
                  tex->bind(0);
              last_texture = tex;
              shader.draw(mesh);
          }
        if (auto a = c[idx].scenery(); a.atlas)
        {
          auto& tex = a.atlas->texture();
          if (&tex != last_texture)
              tex.bind(0);
          last_texture = &a.atlas->texture();
          auto frame = a.frame;
#if 1
          static std::size_t f = 0;
          f++;
          if (f > a.atlas->info().nframes * 3)
              f = 0;
          frame.frame = (scenery::frame_t)std::min(f, a.atlas->info().nframes - 1);
#endif
          _anim_mesh.draw(shader, *a.atlas, a.frame.r, frame.frame, local_coords{idx});
        }
    }
}

std::array<std::array<UnsignedShort, 6>, wall_mesh::COUNT> wall_mesh::make_index_array()
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    std::array<std::array<UnsignedShort, 6>, COUNT> array;

    for (std::size_t i = 0; i < std::size(array); i++)
        array[i] = tile_atlas::indices(i);
    return array;
}

std::array<std::array<Vector3, 4>, wall_mesh::COUNT> wall_mesh::make_position_array()
{
    std::array<std::array<Vector3, 4>, COUNT> array;
    for (std::uint8_t j = 0; j < TILE_MAX_DIM; j++)
        for (std::uint8_t i = 0; i < TILE_MAX_DIM; i++)
        {
            const std::size_t idx = (j*TILE_MAX_DIM + i) * 2u;
            const auto center = Vector3(i, j, 0) * TILE_SIZE;
            array[idx + 0] = tile_atlas::wall_quad_N(center, TILE_SIZE);
            array[idx + 1] = tile_atlas::wall_quad_W(center, TILE_SIZE);
        }
    return array;
}

} // namespace floormat
