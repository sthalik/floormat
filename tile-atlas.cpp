#include "tile-atlas.hpp"
#include <Corrade/Containers/StringView.h>
#include <Magnum/ImageView.h>
#include <Magnum/GL/TextureFormat.h>

namespace Magnum::Examples {

tile_atlas::tile_atlas(const Containers::StringView& name, const ImageView2D& image, Vector2i dims) :
    name_{name},
    size_{image.size()},
    dims_{dims}
{
    CORRADE_INTERNAL_ASSERT(dims_[0] > 0 && dims_[1] > 0);
    CORRADE_INTERNAL_ASSERT(size_ % dims_ == Vector2i{});
    CORRADE_INTERNAL_ASSERT(dims.product() < 256);
    CORRADE_INTERNAL_ASSERT(tile_size() * dims_ == size_);
    tex_.setWrapping(GL::SamplerWrapping::ClampToEdge)
        .setMagnificationFilter(GL::SamplerFilter::Nearest)
        .setMinificationFilter(GL::SamplerFilter::Linear)
        .setMaxAnisotropy(0)
        .setStorage(1, GL::textureFormat(image.format()), image.size())
        .setSubImage(0, {}, image);
}

std::array<Vector2, 4> tile_atlas::texcoords_for_id(std::size_t id2) const
{
    const auto tile_size_ = tile_size();
    auto id_ = (int)id2;
    ASSERT(id_ >= 0 && id_ < dims_.product());
    Vector2i id = { id_ % dims_[0], id_ / dims_[0] };
    auto p0 = Vector2(id * tile_size_) / Vector2(size_);
    auto p1 = Vector2(tile_size_) / Vector2(size_);
    auto x0 = p0.x(), x1 = p1.x(), y0 = p0.y(), y1 = p1.y();
    return {{
        { x0+x1, y0+y1 }, // bottom right
        { x0+x1, y0    }, // top right
        { x0,    y0+y1 }, // bottom left
        { x0,    y0    }  // top left
    }};
}

} // namespace Magnum::Examples
