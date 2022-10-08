#include "tile-atlas.hpp"
#include "compat/assert.hpp"
#include <Corrade/Containers/StringView.h>
#include <Magnum/ImageView.h>
#include <Magnum/GL/TextureFormat.h>

namespace Magnum::Examples {

tile_atlas::tile_atlas(Containers::StringView name, const ImageView2D& image, Vector2ui dims) :
    name_{name},
    size_{image.size()},
    dims_{dims}
{
    CORRADE_INTERNAL_ASSERT(dims_[0] > 0 && dims_[1] > 0);
    CORRADE_INTERNAL_ASSERT(size_ % dims_ == Vector2ui{});
    CORRADE_INTERNAL_ASSERT(dims_.product() < 256);
    tex_.setWrapping(GL::SamplerWrapping::ClampToEdge)
        .setMagnificationFilter(GL::SamplerFilter::Nearest)
        .setMinificationFilter(GL::SamplerFilter::Linear)
        .setMaxAnisotropy(0)
        .setStorage(GL::textureFormat(image.format()), image.size())
        .setSubImage({}, image);
}

std::array<Vector2, 4> tile_atlas::texcoords_for_id(std::size_t id_) const
{
    const auto sz = size_/dims_;
    ASSERT(id_ < sz.product());
    const Vector2ui id = { (UnsignedInt)id_ % dims_[0], (UnsignedInt)id_ / dims_[0] };
    const Vector2 p0(id * sz), p1(sz);
    const auto x0 = p0.x(), x1 = p1.x(), y0 = p0.y(), y1 = p1.y();
    return {{
        { x0+x1, y0+y1 }, // bottom right
        { x0+x1, y0    }, // top right
        { x0,    y0+y1 }, // bottom left
        { x0,    y0    }  // top left
    }};
}

} // namespace Magnum::Examples
