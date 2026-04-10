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

// Vertex order: {0:BR, 1:TR, 2:BL, 3:TL}.
//
// Atlas rotation convention: 90° CCW.
//   Original pixel (x, y) is stored at atlas-local (y, W-1-x).
//   A W×H sprite occupies an H×W rectangle in the atlas.
//   The pixel data must be rotated before uploading via glTexSubImage.
//
// UV permutations — derived from mapping each screen vertex
// to its corresponding atlas corner after CCW rotation:
//   normal  = {0,1,2,3}    (identity)
//   mirror  = {2,3,0,1}    (swap L↔R pairs)
//   rotated = {1,3,0,2}    (CCW: BR→TR, TR→TL, BL→BR, TL→BL)
//   both    = {0,2,1,3}    (rotated, then horizontal mirror swaps i↔i^2)
texcoords texcoords_at(Vector2ui pos_, Vector2ui size_, Vector2ui image_size_, bool mirror, bool rotated)
{
    auto pos = Vector2(pos_), size = Vector2(size_), image_size = Vector2(image_size_);
    auto offset = pos + Vector2(.5f), end = offset + size - Vector2(1);
    auto x0 = offset / image_size, x1 = end / image_size;

    const Vector2 corners[4] = {
        { x1.x(), 1.f - x1.y() }, // 0: BR
        { x1.x(), 1.f - x0.y() }, // 1: TR
        { x0.x(), 1.f - x1.y() }, // 2: BL
        { x0.x(), 1.f - x0.y() }, // 3: TL
    };

    constexpr struct {
        uint8_t a:2, b:2, c:2, d:2;
    } perm[4] = {
        {0, 1, 2, 3}, // normal
        {1, 3, 0, 2}, // rotated only
        {2, 3, 0, 1}, // mirrored only
        {0, 2, 1, 3}, // mirror + rotated
    };
    const auto p = perm[(unsigned)mirror << 1 | (unsigned)rotated];
    return {{ corners[p.a], corners[p.b], corners[p.c], corners[p.d] }};
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
