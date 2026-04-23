#pragma once
#include "compat/assert.hpp"
#include "rotation.hpp"
#include <mg/Vector2.h>

namespace floormat {

constexpr auto rotation_symmetry(rotation r)
{
    constexpr struct {
        Vector2b axis;
        Vector2ub point, rect;
    } rotation_symmetries[] = {
        { { 1,  1}, {0, 1}, {0, 1} }, // N
        { {-1,  1}, {1, 0}, {1, 0} }, // E
        { {-1, -1}, {0, 1}, {0, 1} }, // S
        { { 1, -1}, {1, 0}, {1, 0} }, // W
    };

    fm_assert(r < rotation_COUNT);
    auto idx = (size_t)r / 2;
    const auto sym = rotation_symmetries[idx];
    return sym;
}

template<typename T>
constexpr Math::Vector2<T> rotate_point(Math::Vector2<T> rect, rotation r_old, rotation r_new)
{
    fm_assert(r_old < rotation_COUNT && r_new < rotation_COUNT);
    auto [m_offset0, i_offset0, i_size0] = rotation_symmetry(r_old);
    auto offset0_ = rect * Math::Vector2<T>(m_offset0);
    auto offset_n = Math::Vector2<T>(offset0_[i_offset0.x()], offset0_[i_offset0.y()]);
    auto [m_offset1, i_offset1, i_size1] = rotation_symmetry(r_new);
    return Math::Vector2<T>{offset_n[i_offset1.x()], offset_n[i_offset1.y()]}*Math::Vector2<T>{m_offset1};
}

constexpr Vector2ub rotate_size(Vector2ub size0, rotation r_old, rotation r_new)
{
    fm_assert(r_old < rotation_COUNT && r_new < rotation_COUNT);
    auto [m_offset0, i_offset0, i_size0] = rotation_symmetry(r_old);
    auto size_n = Vector2ub(size0[i_size0.x()], size0[i_size0.y()]);
    //fm_debug_assert(r_old != rotation::N || offset_n == offset0 && size_n == size0);
    auto [m_offset1, i_offset1, i_size1] = rotation_symmetry(r_new);
    return Vector2ub{size_n[i_size1.x()], size_n[i_size1.y()]};
}

constexpr Pair<Vector2b, Vector2ub> rotate_bbox(Vector2b offset0, Vector2ub size0, rotation r_old, rotation r_new)
{
    return {
        rotate_point(offset0, r_old, r_new),
        rotate_size(size0, r_old, r_new),
    };
}

} // namespace floormat
