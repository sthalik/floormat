#include "atlas.hpp"
#include "defs.hpp"
#include <Magnum/ImageView.h>
#include <Magnum/GL/TextureFormat.h>

namespace Magnum::Examples {

texture_atlas::texture_atlas(const ImageView2D& image, Vector2i dims) :
    size_{image.size()},
    dims_{dims},
    tile_size_{size_ / dims}
{
    CORRADE_INTERNAL_ASSERT(dims_[0] > 0 && dims_[1] > 0);
    CORRADE_INTERNAL_ASSERT(tile_size_ * dims_ == size_);
    CORRADE_INTERNAL_ASSERT(size_ % dims_ == Vector2i{});
    CORRADE_INTERNAL_ASSERT(dims.product() < 256);
    tex_.setWrapping(GL::SamplerWrapping::ClampToEdge)
        .setMagnificationFilter(GL::SamplerFilter::Nearest)
        .setMinificationFilter(GL::SamplerFilter::Linear)
        .setMaxAnisotropy(0)
        .setStorage(1, GL::textureFormat(image.format()), image.size())
        .setSubImage(0, {}, image);
}

std::array<Vector2, 4> texture_atlas::texcoords_for_id(int id_) const
{
    CORRADE_INTERNAL_ASSERT(id_ >= 0 && id_ < dims_.product());
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

using vertex_array_type = texture_atlas::vertex_array_type;

vertex_array_type texture_atlas::floor_quad(Vector3 center, Vector2 size)
{
    float x = size[0]*.5f, y = size[1]*.5f;
    return {{
        { x + center[0], -y + center[1], center[2]},
        { x + center[0],  y + center[1], center[2]},
        {-x + center[0], -y + center[1], center[2]},
        {-x + center[0],  y + center[1], center[2]},
    }};
}

vertex_array_type texture_atlas::wall_quad_W(Vector3 center, Vector3 size)
{
    float x = size[0]*.5f, y = size[1]*.5f, z = size[2];
    return {{
        { x + center[0], y + center[1],     center[2] },
        { x + center[0], y + center[1], z + center[2] },
        {-x + center[0], y + center[1],     center[2] },
        {-x + center[0], y + center[1], z + center[2] },
    }};
}

vertex_array_type texture_atlas::wall_quad_S(Vector3 center, Vector3 size)
{
    float x = size[0]*.5f, y = size[1]*.5f, z = size[2];
    return {{
        {-x + center[0],  y + center[1],     center[2] },
        {-x + center[0],  y + center[1], z + center[2] },
        {-x + center[0], -y + center[1],     center[2] },
        {-x + center[0], -y + center[1], z + center[2] },
    }};
}

vertex_array_type texture_atlas::wall_quad_E(Vector3 center, Vector3 size)
{
    float x = size[0]*.5f, y = size[1]*.5f, z = size[2];
    return {{
        { x + center[0], -y + center[1],     center[2] },
        { x + center[0], -y + center[1], z + center[2] },
        {-x + center[0], -y + center[1],     center[2] },
        {-x + center[0], -y + center[1], z + center[2] },
    }};
}

vertex_array_type texture_atlas::wall_quad_N(Vector3 center, Vector3 size)
{
    float x = size[0]*.5f, y = size[1]*.5f, z = size[2];
    return {{
        { x + center[0], -y + center[1],     center[2] },
        { x + center[0], -y + center[1], z + center[2] },
        { x + center[0],  y + center[1],     center[2] },
        { x + center[0],  y + center[1], z + center[2] },
    }};
}

std::array<UnsignedShort, 6> texture_atlas::indices(int N)
{
    CORRADE_INTERNAL_ASSERT(N >= 0);
    using u16 = UnsignedShort;
    return {                                        /* 3--1 1 */
        (u16)(0+N*4), (u16)(1+N*4), (u16)(2+N*4),   /* | / /| */
        (u16)(2+N*4), (u16)(1+N*4), (u16)(3+N*4),   /* |/ / | */
    };                                              /* 2 2--0 */
}

} // namespace Magnum::Examples
