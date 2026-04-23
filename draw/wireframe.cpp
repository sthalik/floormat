#include "wireframe.hpp"
#include "shaders/shader.hpp"
#include <cr/Array.h>
#include <mg/Range.h>
#include <mg/TextureArray.h>
#include <mg/ImageView.h>
#include <mg/PixelFormat.h>
#include <mg/Context.h>
#include <mg/Renderer.h>
#include <mg/TextureFormat.h>
#include <mg/ImageData.h>

namespace floormat::wireframe {

GL::Texture2DArray make_constant_texture()
{
    const Vector4ub data[] = { {255, 255, 255, 255} };
    Trade::ImageData2D img{PixelFormat::RGBA8Unorm, {1, 1}, {},
                           Containers::arrayView(data, 1), {}, {}};
    const ImageView3D img3d{img.storage(), img.format(),
                            {img.size(), 1}, img.data()};
    GL::Texture2DArray tex;
    tex.setWrapping(GL::SamplerWrapping::ClampToEdge)
       .setMagnificationFilter(GL::SamplerFilter::Nearest)
       .setMinificationFilter(GL::SamplerFilter::Nearest)
       .setStorage(1, GL::textureFormat(img.format()), {img.size(), 1})
       .setSubImage(0, {}, img3d);
    return tex;
}

struct constant_buf {
    Vector3 texcoords;
    float depth = 1;
};

mesh_base::mesh_base(GL::MeshPrimitive primitive, ArrayView<const void> index_data,
                     size_t num_vertices, size_t num_indexes, GL::Texture2DArray* texture) :
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
    shader.draw(*_texture, _mesh);
}

void mesh_base::set_subdata(ArrayView<const void> array)
{
    _vertex_buffer.setSubData(0, array);
}

void mesh_base::set_line_width(float width)
{
    if (GL::Context::current().detectedDriver() == GL::Context::DetectedDriver::Svga3D)
        return;

    auto range = GL::Renderer::lineWidthRange();
    if (range.contains(width))
        GL::Renderer::setLineWidth(width);
}

} // namespace floormat::wireframe
