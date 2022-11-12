#include "wireframe.hpp"
#include "shaders/tile.hpp"
//#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayView.h>
#include <Magnum/GL/TextureFormat.h>
//#include <Magnum/ImageFlags.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
//#include <Magnum/PixelStorage.h>
#include <Magnum/Trade/ImageData.h>

namespace floormat::wireframe
{

GL::Texture2D make_constant_texture()
{
    const Vector4ub data[] = { {255, 255, 255, 255} };
    Trade::ImageData2D img{PixelFormat::RGBA8Unorm, {1, 1}, {},
                           Containers::arrayView(data, 1), {}, {}};
    GL::Texture2D tex;
    tex.setWrapping(GL::SamplerWrapping::ClampToEdge)
       .setMagnificationFilter(GL::SamplerFilter::Nearest)
       .setMinificationFilter(GL::SamplerFilter::Nearest)
       .setStorage(1, GL::textureFormat(img.format()), img.size())
       .setSubImage(0, {}, std::move(img));
    return tex;
}

struct constant_buf {
    Vector2 texcoords;
    float depth = 1;
};

mesh_base::mesh_base(GL::MeshPrimitive primitive, ArrayView<const void> index_data,
                     std::size_t num_vertices, std::size_t num_indexes, GL::Texture2D* texture) :
    _vertex_buffer{Containers::Array<Vector3>{ValueInit, num_vertices}, GL::BufferUsage::DynamicDraw},
    _constant_buffer{Containers::Array<constant_buf>{ValueInit, num_vertices}},
    _index_buffer{num_indexes == 0 ? GL::Buffer{NoCreate} : GL::Buffer{index_data}},
    _texture{texture}
{
    _mesh.setCount((int)(num_indexes > 0 ? num_indexes : num_vertices))
        .setPrimitive(primitive)
        .addVertexBuffer(_vertex_buffer, 0, tile_shader::Position{})
        .addVertexBuffer(_constant_buffer, 0, tile_shader::TextureCoordinates{}, tile_shader::Depth{});
    if (num_indexes > 0)
        _mesh.setIndexBuffer(_index_buffer, 0, GL::MeshIndexType::UnsignedShort);
}

void mesh_base::draw(tile_shader& shader)
{
    _texture->bind(0);
    shader.draw(_mesh);
}

void mesh_base::set_subdata(ArrayView<const void> array)
{
    _vertex_buffer.setSubData(0, array);
}

} // namespace floormat::wireframe
