#include "wireframe-mesh.hpp"
#include "shaders/tile-shader.hpp"
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/Array.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImageFlags.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/PixelStorage.h>
#include <Magnum/Trade/ImageData.h>

namespace floormat::wireframe
{

GL::RectangleTexture mesh_base::make_constant_texture()
{
    const Vector4ub data[] = { {255, 255, 255, 255} };
    Trade::ImageData2D img{PixelFormat::RGBA8Unorm, {1, 1}, {},
                           Containers::arrayView(data, 1), {}, {}};
    GL::RectangleTexture tex;
    tex.setWrapping(GL::SamplerWrapping::ClampToEdge)
       .setMagnificationFilter(GL::SamplerFilter::Nearest)
       .setMinificationFilter(GL::SamplerFilter::Nearest)
       .setMaxAnisotropy(1)
       .setStorage(GL::textureFormat(img.format()), img.size())
       .setSubImage({}, std::move(img));
    return tex;
}

mesh_base::mesh_base(GL::MeshPrimitive primitive, Containers::ArrayView<const void> index_data,
                     std::size_t num_vertices, std::size_t num_indexes) :
    _vertex_buffer{Containers::Array<Vector3>{ValueInit, num_vertices}, GL::BufferUsage::DynamicDraw},
    _texcoords_buffer{Containers::Array<Vector2>{ValueInit, num_vertices}},
    _index_buffer{num_indexes == 0 ? GL::Buffer{NoCreate} : GL::Buffer{index_data}}
{
    _mesh.setCount((int)(num_indexes > 0 ? num_indexes : num_vertices))
        .setPrimitive(primitive)
        .addVertexBuffer(_vertex_buffer, 0, tile_shader::Position{})
        .addVertexBuffer(_texcoords_buffer, 0, tile_shader::TextureCoordinates{});
    if (num_indexes > 0)
        _mesh.setIndexBuffer(_index_buffer, 0, GL::MeshIndexType::UnsignedShort);
}

void mesh_base::draw(tile_shader& shader)
{
    _texture.bind(0);
    shader.draw(_mesh);
}


} // namespace floormat::wireframe
