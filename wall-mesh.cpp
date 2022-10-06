#include "wall-mesh.hpp"
#include "tile-atlas.hpp"
#include "shaders/tile-shader.hpp"
#include "chunk.hpp"
#include <Magnum/GL/RectangleTexture.h>
#include <Magnum/GL/MeshView.h>

namespace Magnum::Examples {

constexpr auto quad_index_count = 6;

wall_mesh::wall_mesh()
{
    _mesh.setCount((int)(quad_index_count * COUNT))
         .addVertexBuffer(_vertex_buffer, 0, tile_shader::TextureCoordinates{}, tile_shader::Position{})
         .setIndexBuffer(_index_buffer, 0, GL::MeshIndexType::UnsignedShort);
    CORRADE_INTERNAL_ASSERT(_mesh.isIndexed());
}

void wall_mesh::add_wall(vertex_array& data, texture_array& textures, std::size_t& pos_,
                         tile_image& img, const position_array& positions)
{
    const auto pos = pos_++;
    CORRADE_INTERNAL_ASSERT(pos < data.size());
    auto texcoords = img.atlas->texcoords_for_id(img.variant);
    for (std::size_t i = 0; i < 4; i++)
    {
        data[pos][i] = { texcoords[i], positions[i] };
        textures[pos] = &img.atlas->texture();
    }
}

void wall_mesh::maybe_add_tile(vertex_array& data, texture_array& textures, std::size_t& pos, tile& x, local_coords pt)
{
    constexpr float X = TILE_SIZE[0], Y = TILE_SIZE[1], Z = TILE_SIZE[2];
    constexpr Vector3 size = {X, Y, Z};
    Vector3 center{(float)(X*pt.x), (float)(Y*pt.y), 0};

    if (auto& wall = x.wall_north; wall.atlas)
        add_wall(data, textures, pos, wall, tile_atlas::wall_quad_N(center, size));
    if (auto& wall = x.wall_west; wall.atlas)
        add_wall(data, textures, pos, wall, tile_atlas::wall_quad_W(center, size));
}

void wall_mesh::draw(tile_shader& shader, chunk& c)
{
    texture_array textures = {};
    std::size_t pos = 0;
    {
        vertex_array data;
        c.foreach_tile([&](tile& x, std::size_t, local_coords pt) {
          maybe_add_tile(data, textures, pos, x, pt);
        });
        _vertex_buffer.setSubData(0, Containers::arrayView(data.data(), pos));
    }

    const GL::RectangleTexture* last_texture = nullptr;
    Magnum::GL::MeshView mesh{_mesh};
    for (std::size_t i = 0; i < pos; i++)
    {
        auto* const tex = textures[i];
        CORRADE_INTERNAL_ASSERT(tex != nullptr);
        mesh.setCount(quad_index_count);
        mesh.setIndexRange((int)(i*quad_index_count), 0, quad_index_count*COUNT - 1);
        if (tex != last_texture)
            tex->bind(0);
        last_texture = tex;
        shader.draw(mesh);
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

} // namespace Magnum::Examples
