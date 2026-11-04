#include "raycast-line.hpp"
#include "compat/assert.hpp"

namespace floormat::rc {

bresenham_line::bresenham_line(Vector2i start_pos, Vector2i end_pos) : _start_pos{start_pos}
{
    const auto dx = (i64)end_pos.x() - start_pos.x(), dy = (i64)end_pos.y() - start_pos.y();
    const auto adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    _y_major = ady > adx;
    _major_len = (u32)(_y_major ? ady : adx);
    _minor_len = (u32)(_y_major ? adx : ady);
    _major_sign = (_y_major ? dy : dx) < 0 ? -1 : 1;
    _minor_sign = (_y_major ? dx : dy) < 0 ? -1 : 1;
    // With major below 2^31, 2*step is below 2^32 and minor below 2^31, so the products in
    // minor_at(), first_step_at_minor() and orthogonal_at() stay below 2^63 and fit i64. World
    // coordinates span 2^26 units.
    fm_assert(_major_len < 1u << 31);
}

u32 bresenham_line::steps() const { return _major_len; }
Vector2i bresenham_line::start_pos() const { return _start_pos; }
u32 bresenham_line::major_axis() const { return _y_major; }
u32 bresenham_line::minor_len() const { return _minor_len; }
i32 bresenham_line::sign(u32 axis) const { return (axis == 1) == _y_major ? _major_sign : _minor_sign; }

Vector2i bresenham_line::point_at(u32 step) const
{
    fm_debug_assert(step <= _major_len);
    return octant_to_point(step, minor_at(step));
}

auto bresenham_line::orthogonal_at(u32 step) const -> orthogonal_points
{
    fm_debug_assert(step < _major_len);
    const u64 k = step, major = _major_len, minor = _minor_len;
    const auto m0 = minor_at(step);
    if (m0 == minor_at(step + 1))
        return { .points = {}, .in_supercover = {}, .diagonal = false };
    // Twice the line's minor coordinate at major k + 1/2, minus the edge between m0 and m0 + 1,
    // all times major. The line meets the edge before k + 1/2 when e > 0.
    const auto e = (i64)((2*k + 1) * minor) - (i64)((2*m0 + 1) * major);
    const auto major_first = octant_to_point(step + 1, m0), minor_first = octant_to_point(step, m0 + 1);
    if (!_y_major)
        return { .points = { major_first, minor_first }, .in_supercover = { e <= 0, e >= 0 }, .diagonal = true };
    else
        return { .points = { minor_first, major_first }, .in_supercover = { e >= 0, e <= 0 }, .diagonal = true };
}

u32 bresenham_line::step_of(Vector2i pt) const
{
    const auto d = _y_major ? (i64)pt.y() - _start_pos.y() : (i64)pt.x() - _start_pos.x();
    const auto step = (u32)(d < 0 ? -d : d);
    fm_debug_assert(step <= _major_len);
    fm_debug_assert(point_at(step).x() == pt.x() && point_at(step).y() == pt.y());
    return step;
}

auto bresenham_line::interval_bounds_to_steps(u32 axis, i32 lo, i32 hi) const -> step_interval
{
    fm_debug_assert(axis < 2);
    const bool is_major = (axis == 1) == _y_major;
    const i64 start = axis == 0 ? _start_pos.x() : _start_pos.y();
    const i64 len = is_major ? _major_len : _minor_len;
    const bool forward = (is_major ? _major_sign : _minor_sign) > 0;
    i64 a = forward ? (i64)lo - start : start - (i64)hi;
    i64 b = forward ? (i64)hi - start : start - (i64)lo;
    a = a < 0 ? 0 : a;
    b = b > len ? len : b;
    if (a > b)
        return { 1, 0 };
    if (is_major)
        return { (u32)a, (u32)b };
    return { first_step_at_minor((u64)a), b == len ? _major_len : first_step_at_minor((u64)b + 1) - 1 };
}

u64 bresenham_line::minor_at(u32 step) const
{
    if (_major_len == 0)
        return 0;
    const u64 major = _major_len;
    return (2*(u64)step*_minor_len + major) / (2*major);
}

// minor_at(k) >= m exactly when k >= (2m - 1)*major / (2*minor), for 1 <= m <= minor.
u32 bresenham_line::first_step_at_minor(u64 m) const
{
    if (m == 0)
        return 0;
    const u64 a = (2*m - 1) * _major_len, b = 2 * (u64)_minor_len;
    return (u32)((a + b - 1) / b);
}

Vector2i bresenham_line::octant_to_point(u32 step, u64 minor) const
{
    const auto a = _major_sign * (i32)step, b = _minor_sign * (i32)minor;
    if (!_y_major)
        return { _start_pos.x() + a, _start_pos.y() + b };
    else
        return { _start_pos.x() + b, _start_pos.y() + a };
}

} // namespace floormat::rc
