#include "tile-mesh.hpp"
#include "tile-shader.hpp"
#include "tile.hpp"

namespace Magnum::Examples {

tile_mesh::tile_mesh()
{
    _mesh.setCount((int)index_count)
        .addVertexBuffer(_vertex_buffer, 0,
                         tile_shader::Position{}, tile_shader::TextureCoordinates{})
        .setIndexBuffer(_index_buffer, 0, GL::MeshIndexType::UnsignedShort);
}

void tile_mesh::draw_quad(tile_shader& shader, tile_image& img, const std::array<Vector3, 4>& positions)
{
    auto texcoords = img.atlas->texcoords_for_id(img.variant);
    //auto positions = img.atlas->floor_quad(position, { TILE_SIZE[0], TILE_SIZE[1] });
    for (std::size_t i = 0; i < 4; i++)
        _vertex_data[i] = {positions[i], texcoords[i]};
    img.atlas->texture().bind(0);
    _vertex_buffer.setData(_vertex_data, Magnum::GL::BufferUsage::DynamicDraw);
    shader.draw(_mesh);
}

void tile_mesh::draw_floor_quad(tile_shader& shader, tile_image& img, Vector3 center)
{
    draw_quad(shader, img, img.atlas->floor_quad(center, { TILE_SIZE[0], TILE_SIZE[1] }));
}

} // namespace Magnum::Examples
