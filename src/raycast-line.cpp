#include "raycast-line.hpp"
#include "compat/assert.hpp"

namespace floormat::rc {

bresenham_line::bresenham_line(Vector2i start_pos, Vector2i end_pos) : _start_pos{start_pos}
{
    const auto dx = (int64_t)end_pos.x() - start_pos.x(), dy = (int64_t)end_pos.y() - start_pos.y();
    const auto adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    _y_major = ady > adx;
    _major_len = (uint32_t)(_y_major ? ady : adx);
    _minor_len = (uint32_t)(_y_major ? adx : ady);
    _major_sign = (_y_major ? dy : dx) < 0 ? -1 : 1;
    _minor_sign = (_y_major ? dx : dy) < 0 ? -1 : 1;
    // With major below 2^31, 2*step is below 2^32 and minor below 2^31, so the products in
    // minor_at(), first_step_at_minor() and orthogonal_at() stay below 2^63 and fit int64_t. World
    // coordinates span 2^26 units.
    fm_assert(_major_len < 1u << 31);
}

uint32_t bresenham_line::steps() const { return _major_len; }
Vector2i bresenham_line::start_pos() const { return _start_pos; }
uint32_t bresenham_line::major_axis() const { return _y_major; }
uint32_t bresenham_line::minor_len() const { return _minor_len; }
int32_t bresenham_line::sign(uint32_t axis) const { return (axis == 1) == _y_major ? _major_sign : _minor_sign; }

Vector2i bresenham_line::point_at(uint32_t step) const
{
    fm_debug_assert(step <= _major_len);
    return octant_to_point(step, minor_at(step));
}

auto bresenham_line::orthogonal_at(uint32_t step) const -> orthogonal_points
{
    fm_debug_assert(step < _major_len);
    const uint64_t k = step, major = _major_len, minor = _minor_len;
    const auto m0 = minor_at(step);
    if (m0 == minor_at(step + 1))
        return { .points = {}, .in_supercover = {}, .diagonal = false };
    // Twice the line's minor coordinate at major k + 1/2, minus the edge between m0 and m0 + 1,
    // all times major. The line meets the edge before k + 1/2 when e > 0.
    const auto e = (int64_t)((2*k + 1) * minor) - (int64_t)((2*m0 + 1) * major);
    const auto major_first = octant_to_point(step + 1, m0), minor_first = octant_to_point(step, m0 + 1);
    if (!_y_major)
        return { .points = { major_first, minor_first }, .in_supercover = { e <= 0, e >= 0 }, .diagonal = true };
    else
        return { .points = { minor_first, major_first }, .in_supercover = { e >= 0, e <= 0 }, .diagonal = true };
}

uint32_t bresenham_line::step_of(Vector2i pt) const
{
    const auto d = _y_major ? (int64_t)pt.y() - _start_pos.y() : (int64_t)pt.x() - _start_pos.x();
    const auto step = (uint32_t)(d < 0 ? -d : d);
    fm_debug_assert(step <= _major_len);
    fm_debug_assert(point_at(step).x() == pt.x() && point_at(step).y() == pt.y());
    return step;
}

auto bresenham_line::interval_bounds_to_steps(uint32_t axis, int32_t lo, int32_t hi) const -> step_interval
{
    fm_debug_assert(axis < 2);
    const bool is_major = (axis == 1) == _y_major;
    const int64_t start = axis == 0 ? _start_pos.x() : _start_pos.y();
    const int64_t len = is_major ? _major_len : _minor_len;
    const bool forward = (is_major ? _major_sign : _minor_sign) > 0;
    int64_t a = forward ? (int64_t)lo - start : start - (int64_t)hi;
    int64_t b = forward ? (int64_t)hi - start : start - (int64_t)lo;
    a = a < 0 ? 0 : a;
    b = b > len ? len : b;
    if (a > b)
        return { 1, 0 };
    if (is_major)
        return { (uint32_t)a, (uint32_t)b };
    return { first_step_at_minor((uint64_t)a), b == len ? _major_len : first_step_at_minor((uint64_t)b + 1) - 1 };
}

uint64_t bresenham_line::minor_at(uint32_t step) const
{
    if (_major_len == 0)
        return 0;
    const uint64_t major = _major_len;
    return (2*(uint64_t)step*_minor_len + major) / (2*major);
}

// minor_at(k) >= m exactly when k >= (2m - 1)*major / (2*minor), for 1 <= m <= minor.
uint32_t bresenham_line::first_step_at_minor(uint64_t m) const
{
    if (m == 0)
        return 0;
    const uint64_t a = (2*m - 1) * _major_len, b = 2 * (uint64_t)_minor_len;
    return (uint32_t)((a + b - 1) / b);
}

Vector2i bresenham_line::octant_to_point(uint32_t step, uint64_t minor) const
{
    const auto a = _major_sign * (int32_t)step, b = _minor_sign * (int32_t)minor;
    if (!_y_major)
        return { _start_pos.x() + a, _start_pos.y() + b };
    else
        return { _start_pos.x() + b, _start_pos.y() + a };
}

} // namespace floormat::rc
