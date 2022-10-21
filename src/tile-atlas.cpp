#include "tile-atlas.hpp"
#include "compat/assert.hpp"
#include <Corrade/Containers/StringView.h>
#include <Magnum/Math/Color.h>
#include <Magnum/ImageView.h>
#include <Magnum/GL/TextureFormat.h>

namespace floormat {

tile_atlas::tile_atlas(Containers::StringView name, const ImageView2D& image, Vector2ui dims) :
    texcoords_{make_texcoords_array(Vector2ui(image.size()), dims)},
    name_{name}, size_{image.size()}, dims_{dims}
{
    fm_assert(dims_[0] > 0 && dims_[1] > 0);
    fm_assert(size_ % dims_ == Vector2ui());
    fm_assert(dims_.product() < 256);
    tex_.setWrapping(GL::SamplerWrapping::ClampToEdge)
        .setMagnificationFilter(GL::SamplerFilter::Linear)
        .setMinificationFilter(GL::SamplerFilter::Linear)
        .setMaxAnisotropy(1)
        .setBorderColor(Color4{1, 0, 0, 1})
        .setStorage(1, GL::textureFormat(image.format()), image.size())
        .setSubImage(0, {}, image);
}

std::array<Vector2, 4> tile_atlas::texcoords_for_id(std::size_t i) const
{
    fm_assert(i < (size_/dims_).product());
    return texcoords_[i];
}

auto tile_atlas::make_texcoords(Vector2ui size, Vector2ui dims, std::uint_fast16_t i) -> texcoords
{
    const auto sz = size/dims;
    const Vector2ui id = { (UnsignedInt)i % dims[0], (UnsignedInt)i / dims[0] };
    const Vector2 p0(id * sz), p1(sz);
    const auto x0 = p0.x(), x1 = p1.x()-1, y0 = p0.y(), y1 = p1.y()-1;
    return {{
        { (x0+x1)/size[0], (y0+y1)/size[1]  }, // bottom right
        { (x0+x1)/size[0], y0     /size[1]  }, // top right
        { x0     /size[0], (y0+y1)/size[1]  }, // bottom left
        { x0     /size[0], y0     /size[1]  }  // top left
    }};
}

auto tile_atlas::make_texcoords_array(Vector2ui size, Vector2ui dims) -> std::unique_ptr<const texcoords[]>
{
    const auto sz = size/dims;
    const std::uint32_t max = sz.product();
    auto ptr = std::make_unique<std::array<Vector2, 4>[]>(max);
    for (std::uint_fast16_t i = 0; i < max; i++)
        ptr[i] = make_texcoords(size, dims, i);
    return ptr;
}

} // namespace floormat
