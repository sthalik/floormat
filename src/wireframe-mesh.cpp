#include "wireframe-mesh.hpp"
#include "shaders/tile-shader.hpp"
#include "tile-atlas.hpp"
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

GL::RectangleTexture wireframe::null::make_constant_texture()
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

quad::vertex_array quad::make_vertex_array() const
{
#if 0
    constexpr auto X = TILE_SIZE[0], Y = TILE_SIZE[1];
    constexpr float Z = 0;
    return {{
        { -X + center[0], -Y + center[1], Z + center[2] },
        {  X + center[0], -Y + center[1], Z + center[2] },
        {  X + center[0],  Y + center[1], Z + center[2] },
        { -X + center[0],  Y + center[1], Z + center[2] },
    }};
#else
    return tile_atlas::floor_quad(center, {TILE_SIZE[0], TILE_SIZE[1]});
#endif
}

quad::index_array quad::make_index_array()
{
    return tile_atlas::indices(0);
}

quad::quad(Vector3 center, Vector2 size) : center(center), size(size) {}

} // namespace Magnum::Examples::wireframe

namespace Magnum::Examples {

template <wireframe::traits T> wireframe_mesh<T>::wireframe_mesh()
{
    _mesh.setCount((int)T::num_indexes)
         .addVertexBuffer(_texcoords_buffer, 0, tile_shader::TextureCoordinates{})
         .addVertexBuffer(_vertex_buffer, 0, tile_shader::Position{})
         .setIndexBuffer(_index_buffer, 0, GL::MeshIndexType::UnsignedShort);
}

template <wireframe::traits T> void wireframe_mesh<T>::draw(tile_shader& shader, T x)
{
    GL::Renderer::setLineWidth(2);
    _vertex_buffer.setSubData(0, x.make_vertex_array());
    _index_buffer.setSubData(0, x.make_index_array());
    _texture.bind(0);
    shader.draw(_mesh);
}

template struct wireframe_mesh<wireframe::null>;
template struct wireframe_mesh<wireframe::quad>;

} // namespace Magnum::Examples

