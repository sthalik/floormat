#pragma once
#include "compat/assert.hpp"
#include "rotation.hpp"
#include <Corrade/Containers/PairStl.h>
#include <Corrade/Containers/TripleStl.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>

namespace floormat {

constexpr Triple<Vector2b, Vector2ub, Vector2ub> rotation_symmetry(rotation r)
{
    constexpr Pair<rotation, Triple<Vector2b, Vector2ub, Vector2ub>> rotation_symmetries[] = {
        { rotation::N, { { 1,  1}, {0, 1}, {0, 1} } },
        { rotation::E, { {-1,  1}, {1, 0}, {1, 0} } },
        { rotation::S, { {-1, -1}, {0, 1}, {0, 1} } },
        { rotation::W, { { 1, -1}, {1, 0}, {1, 0} } },
    };

    fm_assert(r < rotation_COUNT);
    auto idx = (size_t)r / 2;
    const auto& [r1, sym] = rotation_symmetries[idx];
    return sym;
}

constexpr Vector2b rotate_point(Vector2b rect, rotation r_old, rotation r_new)
{
    fm_assert(r_old < rotation_COUNT && r_new < rotation_COUNT);
    auto [m_offset0, i_offset0, i_size0] = rotation_symmetry(r_old);
    auto offset0_ = rect * m_offset0;
    auto offset_n = Vector2b(offset0_[i_offset0[0]], offset0_[i_offset0[1]]);
    auto [m_offset1, i_offset1, i_size1] = rotation_symmetry(r_new);
    return Vector2b{offset_n[i_offset1[0]], offset_n[i_offset1[1]]}*m_offset1;
}

constexpr Vector2ub rotate_size(Vector2ub size0, rotation r_old, rotation r_new)
{
    fm_assert(r_old < rotation_COUNT && r_new < rotation_COUNT);
    auto [m_offset0, i_offset0, i_size0] = rotation_symmetry(r_old);
    auto size_n = Vector2ub(size0[i_size0[0]], size0[i_size0[1]]);
    //fm_debug_assert(r_old != rotation::N || offset_n == offset0 && size_n == size0);
    auto [m_offset1, i_offset1, i_size1] = rotation_symmetry(r_new);
    return Vector2ub{size_n[i_size1[0]], size_n[i_size1[1]]};
}

constexpr Pair<Vector2b, Vector2ub> rotate_bbox(Vector2b offset0, Vector2ub size0, rotation r_old, rotation r_new)
{
    return {
        rotate_point(offset0, r_old, r_new),
        rotate_size(size0, r_old, r_new),
    };
}

} // namespace floormat
