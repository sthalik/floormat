#include "wireframe-mesh.hpp"
#include "shaders/tile-shader.hpp"
#include <Corrade/Containers/Array.h>
#include <Magnum/ImageView.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>

namespace Magnum::Examples::wireframe_traits {

GL::RectangleTexture wireframe_traits::null::make_constant_texture()
{
    Trade::ImageData2D img{PixelFormat::RGBA8UI, {1, 1}, // NOLINT(misc-const-correctness)
                           Containers::Array<char>{Corrade::DirectInit, 4, (char)(unsigned char)255}};

    GL::RectangleTexture tex;
    tex.setWrapping(GL::SamplerWrapping::Repeat)
       .setMagnificationFilter(GL::SamplerFilter::Nearest)
       .setMinificationFilter(GL::SamplerFilter::Nearest)
       .setMaxAnisotropy(0)
       .setStorage(GL::textureFormat(img.format()), img.size())
       .setSubImage({}, std::move(img));
    return tex;
}

quad::vertex_array quad::make_vertex_positions_array() const
{
    constexpr auto X = TILE_SIZE[0], Y = TILE_SIZE[1];
    constexpr float Z = 0;
    return {{
        { -X + center[0], -Y + center[1], Z + center[2] },
        {  X + center[0], -Y + center[1], Z + center[2] },
        {  X + center[0],  Y + center[1], Z + center[2] },
        { -X + center[0],  Y + center[1], Z + center[2] },
    }};
}

quad::quad(Vector3 center, Vector2 size) : center(center), size(size) {}

} // namespace Magnum::Examples::wireframe_traits

namespace Magnum::Examples {

using wireframe_traits::traits;

template <traits T> wireframe_mesh<T>::wireframe_mesh()
{
    _mesh.setCount((int)T::num_vertices)
         .addVertexBuffer(_texcoords_buffer, 0, tile_shader::TextureCoordinates{})
         .addVertexBuffer(_positions_buffer, 0, tile_shader::Position{});
    if constexpr(T::num_indices > 0)
        _mesh.setIndexBuffer(_index_buffer, 0, GL::MeshIndexType::UnsignedShort);
    CORRADE_INTERNAL_ASSERT(T::num_indices > 0 == _mesh.isIndexed());
};

template <traits T> void wireframe_mesh<T>::draw(tile_shader& shader, T x)
{
    GL::Renderer::setLineWidth(2);
    _positions_buffer.setData(x.make_vertex_positions_array(), GL::BufferUsage::DynamicDraw);
    if constexpr(T::num_indices > 0)
        _index_buffer.setData(x.make_index_array(), GL::BufferUsage::DynamicDraw);
    _texture.bind(0);
    shader.draw(_mesh);
}

template struct wireframe_mesh<wireframe_traits::null>;
template struct wireframe_mesh<wireframe_traits::quad>;

} // namespace Magnum::Examples

