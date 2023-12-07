#include "quads.hpp"

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

quad wall_quad_W(const Vector3 center, const Vector3 size)
{
    float x = size.x()*.5f, y = size.y()*.5f, z = size.z();
    return {{
        {-x + center.x(), -y + center.y(), z + center.z() },
        {-x + center.x(), -y + center.y(),     center.z() },
        {-x + center.x(),  y + center.y(), z + center.z() },
        {-x + center.x(),  y + center.y(),     center.z() },
    }};
}

quad wall_quad_N(const Vector3 center, const Vector3 size)
{
    float x = size.x()*.5f, y = size.y()*.5f, z = size.z();
    return {{
        { x + center.x(), -y + center.y(), z + center.z() },
        { x + center.x(), -y + center.y(),     center.z() },
        {-x + center.x(), -y + center.y(), z + center.z() },
        {-x + center.x(), -y + center.y(),     center.z() },
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

} // namespace floormat::Quads
