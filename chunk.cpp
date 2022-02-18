#include "chunk.hpp"
#include <Magnum/ImageView.h>
#include <Magnum/GL/TextureFormat.h>

namespace Magnum::Examples {

atlas_texture::atlas_texture(const Trade::ImageData2D& image, Vector2i dims) :
    size_{image.size()},
    dims_{dims},
    tile_size_{size_ / dims}
{
    CORRADE_INTERNAL_ASSERT(dims_[0] > 0 && dims_[1] > 0);
    CORRADE_INTERNAL_ASSERT(tile_size_ * dims_ == size_);
    CORRADE_INTERNAL_ASSERT(size_ % dims_ == Vector2i{});
    tex_.setWrapping(GL::SamplerWrapping::ClampToEdge)
        .setMagnificationFilter(GL::SamplerFilter::Nearest)
        .setMinificationFilter(GL::SamplerFilter::Linear)
        .setStorage(1, GL::textureFormat(image.format()), image.size())
        .setSubImage(0, {}, image);
}

std::array<Vector2, 4> atlas_texture::texcoords_for_id(int id_) const
{
    CORRADE_INTERNAL_ASSERT(id_ >= 0 && id_ < dims_.product());
    constexpr Vector2 _05 = { 0.5f, 0.5f };
    constexpr Vector2i _1 = { 1, 1 };
    Vector2i id = { id_ % dims_[1], id_ / dims_[1] };
    auto p0 = (Vector2(id * tile_size_) + _05) / Vector2(size_);
    auto p1 = (Vector2((id + _1) * tile_size_) + _05) / Vector2(size_);
    return {{
        { p1[0], p1[1] }, // bottom right
        { p1[0], p0[1] }, // top right
        { p0[0], p1[1] }, // bottom left
        { p0[0], p0[1] }  // top left
    }};
}

std::array<Vector3, 4> atlas_texture::floor_quad(Vector3 center, Vector2 size)
{
    float x = size[0]*.5f, y = size[1]*.5f;
    return {{
        { x + center[0], -y + center[1], 0},
        { x + center[0],  y + center[1], 0},
        {-x + center[0], -y + center[1], 0},
        {-x + center[0],  y + center[1], 0},
    }};
}

std::array<UnsignedShort, 6> atlas_texture::indices(int N)
{
    CORRADE_INTERNAL_ASSERT(N >= 0);
    using u16 = UnsignedShort;
    return {                                        /* 3--1 1 */
        (u16)(0+N*4), (u16)(1+N*4), (u16)(2+N*4),   /* | / /| */
        (u16)(2+N*4), (u16)(1+N*4), (u16)(3+N*4),   /* |/ / | */
    };                                              /* 2 2--0 */
}

} // namespace Magnum::Examples
