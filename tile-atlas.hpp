#pragma once
#include "compat/assert.hpp"
#include <Magnum/Magnum.h>
#include <Magnum/GL/Texture.h>
#include <array>

namespace Magnum::Examples {

struct tile_atlas final
{
    using quad = std::array<Vector3, 4>;

    tile_atlas(const ImageView2D& img, Vector2i dims);
    std::array<Vector2, 4> texcoords_for_id(std::size_t id) const;
    static constexpr quad floor_quad(Vector3 center, Vector2 size);
    static constexpr quad wall_quad_N(Vector3 center, Vector3 size);
    static constexpr quad wall_quad_W(Vector3 center, Vector3 size);
    static constexpr std::array<UnsignedShort, 6> indices(std::size_t N);
    std::size_t size() const { return (std::size_t)dims_.product(); }
    Vector2i tile_size() const { return size_ / dims_; }
    GL::Texture2D& texture() { return tex_; }

private:
    GL::Texture2D tex_;
    Vector2i size_, dims_;
};

constexpr std::array<UnsignedShort, 6> tile_atlas::indices(std::size_t N)
{
    using u16 = UnsignedShort;
    return {                                        /* 3--1  1 */
        (u16)(0+N*4), (u16)(1+N*4), (u16)(2+N*4),   /* | /  /| */
        (u16)(2+N*4), (u16)(1+N*4), (u16)(3+N*4),   /* |/  / | */
    };                                              /* 2  2--0 */
}

constexpr tile_atlas::quad tile_atlas::floor_quad(const Vector3 center, const Vector2 size)
{
    ASSERT(size[0] > 0);
    float x = size[0]*.5f, y = size[1]*.5f;
    return {{
        { x + center[0], -y + center[1], center[2]},
        { x + center[0],  y + center[1], center[2]},
        {-x + center[0], -y + center[1], center[2]},
        {-x + center[0],  y + center[1], center[2]},
    }};
}

constexpr tile_atlas::quad tile_atlas::wall_quad_W(const Vector3 center, const Vector3 size)
{
    float x = size[0]*.5f, y = size[1]*.5f, z = size[2];
    return {{
        {-x + center[0],  y + center[1],     center[2] },
        {-x + center[0],  y + center[1], z + center[2] },
        {-x + center[0], -y + center[1],     center[2] },
        {-x + center[0], -y + center[1], z + center[2] },
    }};
}

constexpr tile_atlas::quad tile_atlas::wall_quad_N(const Vector3 center, const Vector3 size)
{
    float x = size[0]*.5f, y = size[1]*.5f, z = size[2];
    return {{
        { x + center[0], -y + center[1],     center[2] },
        { x + center[0], -y + center[1], z + center[2] },
        {-x + center[0], -y + center[1],     center[2] },
        {-x + center[0], -y + center[1], z + center[2] },
    }};
}

} // namespace Magnum::Examples
