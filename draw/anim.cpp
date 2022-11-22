#include "anim.hpp"
#include "anim-atlas.hpp"
#include "chunk.hpp"
#include "shaders/tile.hpp"
#include <Magnum/GL/Texture.h>

namespace floormat {

anim_mesh::anim_mesh()
{
    _mesh.setCount(6)
        .addVertexBuffer(_vertex_buffer, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{})
        .setIndexBuffer(_index_buffer, 0, GL::MeshIndexType::UnsignedShort);
    CORRADE_INTERNAL_ASSERT(_mesh.isIndexed());
}

std::array<UnsignedShort, 6> anim_mesh::make_index_array()
{
    return {{
        0, 1, 2,
        2, 1, 3,
    }};
}

void anim_mesh::draw(tile_shader& shader, anim_atlas& atlas, rotation r, std::size_t frame, local_coords xy)
{
    const auto center = Vector3(xy.x, xy.y, 0.f) * TILE_SIZE;
    const auto pos = atlas.frame_quad(center, r, frame);
    const auto texcoords = atlas.texcoords_for_frame(r, frame);
    const float depth = tile_shader::depth_value(xy, .25f);
    quad_data array;
    for (std::size_t i = 0; i < 4; i++)
        array[i] = { pos[i], texcoords[i], depth };
    _vertex_buffer.setSubData(0, array);
    atlas.texture().bind(0);
    shader.draw(_mesh);
}

} // namespace floormat
