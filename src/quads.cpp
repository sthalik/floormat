#include "quads.hpp"
#include "depth.hpp"
#include "renderer.hpp"
#include <mg/Vector2.h>
#include <mg/Vector3.h>

namespace floormat::Quads {

indexes quad_indexes(size_t N)
{
    using u16 = UnsignedShort;
    return {                                        /* 3--1  1 */
        (u16)(0+N*4), (u16)(1+N*4), (u16)(2+N*4),   /* | /  /| */
        (u16)(2+N*4), (u16)(1+N*4), (u16)(3+N*4),   /* |/  / | */
    };                                              /* 2  2--0 */
}

quad floor_quad(const Vector3 center, const Vector2 size)
{
    float x = size.x()*.5f, y = size.y()*.5f;
    return {{
        { x + center.x(), -y + center.y(), center.z()},
        { x + center.x(),  y + center.y(), center.z()},
        {-x + center.x(), -y + center.y(), center.z()},
        {-x + center.x(),  y + center.y(), center.z()},
    }};
}

texcoords texcoords_at(Vector2ui pos_, Vector2ui size_, Vector2ui image_size_)
{
    auto pos = Vector2(pos_), size = Vector2(size_), image_size = Vector2(image_size_);
    auto offset = pos + Vector2(.5f), end = offset + size - Vector2(1);
    auto x0 = offset / image_size, x1 = end / image_size;
    return {{
        { x1.x(), 1.f - x1.y() }, // bottom right
        { x1.x(), 1.f - x0.y() }, // top right
        { x0.x(), 1.f - x1.y() }, // bottom left
        { x0.x(), 1.f - x0.y() }, // top left
    }};
}

template<bool LR_1, bool LR_2, bool LR_3, bool LR_4>
depths depth_quad(point L, point R, int32_t depth_offset)
{
    static const float S = Render::get_status().is_clipdepth01_enabled ? 0.f : -1.f;

    const float LR[2] = {
        Depth::value_at(S, L, depth_offset),
        Depth::value_at(S, R, depth_offset),
    };

    return {
        LR[LR_1],
        LR[LR_2],
        LR[LR_3],
        LR[LR_4],
    };
}

template depths depth_quad<>(point, point, int32_t);
template depths depth_quad<0, 1, 0, 1>(point, point, int32_t);
template depths depth_quad<1, 0, 1, 0>(point, point, int32_t);
template depths depth_quad<0, 0, 1, 1>(point, point, int32_t);

} // namespace floormat::Quads
