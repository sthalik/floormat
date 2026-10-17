#include "raycast-line.hpp"
#include "compat/assert.hpp"

namespace floormat::rc {

bresenham_line::bresenham_line(Vector2i from, Vector2i to) : _from{from}
{
    const auto dx = (int64_t)to.x() - from.x(), dy = (int64_t)to.y() - from.y();
    const auto adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    _y_major = ady > adx;
    _major_len = (uint32_t)(_y_major ? ady : adx);
    _minor_len = (uint32_t)(_y_major ? adx : ady);
    _major_sign = (_y_major ? dy : dx) < 0 ? -1 : 1;
    _minor_sign = (_y_major ? dx : dy) < 0 ? -1 : 1;
    // World pixels span 2^26, where 2*step*minor reaches 2^53. This keeps it under 2^64.
    fm_assert(_major_len < 1u << 31);
}

uint32_t bresenham_line::steps() const { return _major_len; }

Vector2i bresenham_line::pixel_at(uint32_t step) const
{
    fm_debug_assert(step <= _major_len);
    return make_pixel(step, minor_at(step));
}

auto bresenham_line::off_axis_at(uint32_t step) const -> off_axis_pixels
{
    fm_debug_assert(step < _major_len);
    const uint64_t k = step, major = _major_len, minor = _minor_len;
    const auto m0 = minor_at(k);
    if (m0 == minor_at(k + 1))
        return { .pixels = {}, .crossed = {}, .diagonal = false };
    // Twice the line's minor coordinate at major k + 1/2, minus the edge between m0 and m0 + 1,
    // all times major. The line meets the edge before k + 1/2 when e > 0.
    const auto e = (int64_t)((2*k + 1) * minor) - (int64_t)((2*m0 + 1) * major);
    const auto major_first = make_pixel(k + 1, m0), minor_first = make_pixel(k, m0 + 1);
    if (!_y_major)
        return { .pixels = { major_first, minor_first }, .crossed = { e <= 0, e >= 0 }, .diagonal = true };
    else
        return { .pixels = { minor_first, major_first }, .crossed = { e >= 0, e <= 0 }, .diagonal = true };
}

uint32_t bresenham_line::step_of(Vector2i pixel) const
{
    const auto d = _y_major ? (int64_t)pixel.y() - _from.y() : (int64_t)pixel.x() - _from.x();
    const auto step = (uint32_t)(d < 0 ? -d : d);
    fm_debug_assert(step <= _major_len);
    fm_debug_assert(pixel_at(step).x() == pixel.x() && pixel_at(step).y() == pixel.y());
    return step;
}

auto bresenham_line::steps_within(uint32_t axis, int32_t lo, int32_t hi) const -> step_range
{
    fm_debug_assert(axis < 2);
    const bool is_major = (axis == 1) == _y_major;
    const int64_t from = axis == 0 ? _from.x() : _from.y();
    const int64_t len = is_major ? _major_len : _minor_len;
    const bool forward = (is_major ? _major_sign : _minor_sign) > 0;
    int64_t a = forward ? (int64_t)lo - from : from - (int64_t)hi;
    int64_t b = forward ? (int64_t)hi - from : from - (int64_t)lo;
    a = a < 0 ? 0 : a;
    b = b > len ? len : b;
    if (a > b)
        return { 1, 0 };
    if (is_major)
        return { (uint32_t)a, (uint32_t)b };
    return { first_step_at_minor((uint64_t)a), b == len ? _major_len : first_step_at_minor((uint64_t)b + 1) - 1 };
}

uint64_t bresenham_line::minor_at(uint64_t step) const
{
    if (_major_len == 0)
        return 0;
    const uint64_t major = _major_len;
    return (2*step*_minor_len + major) / (2*major);
}

// minor_at(k) >= m exactly when k >= (2m - 1)*major / (2*minor), for 1 <= m <= minor.
uint32_t bresenham_line::first_step_at_minor(uint64_t m) const
{
    if (m == 0)
        return 0;
    const uint64_t a = (2*m - 1) * _major_len, b = 2 * (uint64_t)_minor_len;
    return (uint32_t)((a + b - 1) / b);
}

Vector2i bresenham_line::make_pixel(uint64_t step, uint64_t minor) const
{
    const auto a = _major_sign * (int32_t)step, b = _minor_sign * (int32_t)minor;
    if (!_y_major)
        return { _from.x() + a, _from.y() + b };
    else
        return { _from.x() + b, _from.y() + a };
}

} // namespace floormat::rc
