#include "tile-atlas.hpp"
#include <Corrade/Containers/StringView.h>
#include <Magnum/ImageView.h>
#include <Magnum/GL/TextureFormat.h>

namespace Magnum::Examples {

tile_atlas::tile_atlas(Containers::StringView name, const ImageView2D& image, Vector2i dims) :
    name_{name},
    size_{image.size()},
    dims_{dims}
{
    CORRADE_INTERNAL_ASSERT(dims_[0] > 0 && dims_[1] > 0);
    CORRADE_INTERNAL_ASSERT(size_ % dims_ == Vector2i{});
    CORRADE_INTERNAL_ASSERT(dims.product() < 256);
    CORRADE_INTERNAL_ASSERT(tile_size() * dims_ == size_);
    tex_.setWrapping(GL::SamplerWrapping::ClampToBorder)
        .setMagnificationFilter(GL::SamplerFilter::Linear)
        .setMinificationFilter(GL::SamplerFilter::Linear)
        .setMaxAnisotropy(0)
        .setStorage(GL::textureFormat(image.format()), image.size())
        .setSubImage({}, image);
}

std::array<Vector2, 4> tile_atlas::texcoords_for_id(std::size_t id2) const
{
    const auto tile_size_ = tile_size();
    auto id_ = (int)id2;
    ASSERT(id_ >= 0 && id_ < dims_.product());
    Vector2i id = { id_ % dims_[0], id_ / dims_[0] };
    constexpr Vector2 _05{.5f, .5f};
    constexpr Vector2 _10{1, 1};
    auto p0 = Vector2(id * tile_size_);
    auto p1 = Vector2(tile_size_);
    auto x0 = p0.x(), x1 = p1.x(), y0 = p0.y(), y1 = p1.y();
    return {{
        { x0+x1, y0+y1 }, // bottom right
        { x0+x1, y0    }, // top right
        { x0,    y0+y1 }, // bottom left
        { x0,    y0    }  // top left
    }};
}

} // namespace Magnum::Examples
