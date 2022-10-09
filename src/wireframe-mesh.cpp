#include "wireframe-mesh.hpp"
#include "shaders/tile-shader.hpp"
#include <Corrade/Containers/Array.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImageFlags.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/PixelStorage.h>
#include <Magnum/Trade/ImageData.h>

namespace Magnum::Examples::wireframe
{

GL::RectangleTexture mesh_base::make_constant_texture()
{
    const Vector4ub data[] = { {255, 255, 255, 255} };
    Trade::ImageData2D img{PixelStorage{}.setImageHeight(1).setRowLength(1).setAlignment(1),
                           PixelFormat::RGBA8Unorm, {1, 1}, {},
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

mesh_base::mesh_base(GL::MeshPrimitive primitive, std::size_t num_vertices) :
    _texcoords_buffer{std::vector<Vector2>{num_vertices}}
{
    _mesh.setCount((int)num_vertices)
        .setPrimitive(primitive)
        .addVertexBuffer(_vertex_buffer, 0, tile_shader::Position{})
        .addVertexBuffer(_texcoords_buffer, 0, tile_shader::TextureCoordinates{});
}

void mesh_base::draw(tile_shader& shader)
{
    _texture.bind(0);
    shader.draw(_mesh);
}


} // namespace Magnum::Examples::wireframe
