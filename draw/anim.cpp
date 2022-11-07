#include "anim.hpp"
#include "anim-atlas.hpp"
#include "shaders/tile.hpp"
#include "wireframe.hpp"
#include "quad-floor.hpp"

namespace floormat {

anim_mesh::anim_mesh()
{
    _mesh.setCount(6)
        .addVertexBuffer(_vertex_buffer, 0, tile_shader::TextureCoordinates{})
        .addVertexBuffer(_positions_buffer, 0, tile_shader::Position{})
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

void anim_mesh::draw(tile_shader& shader, const anim_atlas& atlas, const anim_frame& frame, local_coords xy)
{
    const auto center = Vector3(xy.x, xy.y, 0.f) * TILE_SIZE;
    const auto pos = atlas.frame_quad(center, frame);
    _positions_buffer.setSubData(0, pos);
    const auto texcoords = atlas.texcoords_for_frame(frame);
    _vertex_buffer.setSubData(0, texcoords);
    shader.draw(_mesh);
}

} // namespace floormat
