#include "app.hpp"
#include "src/raycast-line.hpp"
#include <mg/Functions.h>

namespace floormat {

namespace {

using rc::bresenham_line;

constexpr int32_t max_len = 24;

bool test_plain_bresenham_misses()
{
    const auto line = bresenham_line{{0, 0}, {5, 2}};
    constexpr int32_t main_y[] = { 0, 0, 1, 1, 2, 2 };
    if (line.steps() != 5)
        return false;
    for (auto k = 0u; k <= line.steps(); k++)
        if (line.pixel_at(k).x() != (int32_t)k || line.pixel_at(k).y() != main_y[k])
            return false;
    for (auto k : { 0u, 2u, 4u })
        if (line.off_axis_at(k).diagonal)
            return false;
    // Plain Bresenham skips (1, 1) and (4, 1), which the segment crosses.
    const auto o1 = line.off_axis_at(1), o3 = line.off_axis_at(3);
    return o1.diagonal && o1.pixels[1].x() == 1 && o1.pixels[1].y() == 1 && o1.crossed[1] && !o1.crossed[0] &&
           o3.diagonal && o3.pixels[0].x() == 4 && o3.pixels[0].y() == 1 && o3.crossed[0] && !o3.crossed[1];
}

// Incremental Bresenham, ties away from `from` like the closed form.
void check_against_incremental(Vector2i from, Vector2i to)
{
    const auto line = bresenham_line{from, to};
    const auto d = to - from;
    const bool y_major = Math::abs(d.y()) > Math::abs(d.x());
    const auto major = y_major ? d.y() : d.x(), minor = y_major ? d.x() : d.y();
    const auto len = Math::abs(major), minor_len = Math::abs(minor);
    const auto major_sign = major < 0 ? -1 : 1, minor_sign = minor < 0 ? -1 : 1;

    fm_assert_equal((uint32_t)len, line.steps());
    fm_assert(line.pixel_at(0) == from);
    fm_assert(line.pixel_at(line.steps()) == to);

    // err is 2*k*minor_len + len - 2*len*m, in [0, 2*len).
    int32_t m = 0, err = len;
    for (int32_t k = 0; k <= len; k++)
    {
        const auto p = line.pixel_at((uint32_t)k);
        const auto rel = y_major ? Vector2i{minor_sign*m, major_sign*k} : Vector2i{major_sign*k, minor_sign*m};
        if (p != from + rel)
            fm_abort("line (%d %d)-(%d %d) step %d: (%d %d), expected (%d %d)",
                     from.x(), from.y(), to.x(), to.y(), k, p.x(), p.y(), from.x() + rel.x(), from.y() + rel.y());
        fm_assert_equal((uint32_t)k, line.step_of(p));
        if (k < len)
        {
            const auto q = line.pixel_at((uint32_t)k + 1);
            const auto o = line.off_axis_at((uint32_t)k);
            fm_assert(o.diagonal == (p.x() != q.x() && p.y() != q.y()));
            if (o.diagonal)
            {
                fm_assert(o.pixels[0] == Vector2i(q.x(), p.y()));
                fm_assert(o.pixels[1] == Vector2i(p.x(), q.y()));
                fm_assert(o.crossed[0] || o.crossed[1]);
            }
        }
        err += 2*minor_len;
        if (err >= 2*len)
        {
            m++;
            err -= 2*len;
        }
    }
}

// The closed segment from 0 to d touches the closed unit square centred on q. Separating axes
// x, y and the segment's normal, in doubled coordinates to stay integer.
bool touches_square(Vector2i d, Vector2i q)
{
    const auto lo = 2*q - Vector2i{1}, hi = 2*q + Vector2i{1};
    for (auto i = 0u; i < 2; i++)
        if (Math::max(0, 2*d[i]) < lo[i] || Math::min(0, 2*d[i]) > hi[i])
            return false;
    auto above = 0, below = 0;
    for (auto x : { lo.x(), hi.x() })
        for (auto y : { lo.y(), hi.y() })
        {
            const auto s = d.x()*y - d.y()*x;
            above += s > 0;
            below += s < 0;
        }
    return above < 4 && below < 4;
}

void check_supercover(Vector2i from, Vector2i d)
{
    constexpr auto size_max = 2*max_len + 3;
    const auto line = bresenham_line{from, from + d};
    const auto lo = Math::min(Vector2i{0}, d) - Vector2i{1}, hi = Math::max(Vector2i{0}, d) + Vector2i{1};
    const auto size = hi - lo + Vector2i{1};
    fm_assert(size.x() <= size_max && size.y() <= size_max);

    bool seen[size_max*size_max] = {};
    auto mark = [&](Vector2i p) {
        const auto r = p - from - lo;
        fm_assert(r.x() >= 0 && r.y() >= 0 && r.x() < size.x() && r.y() < size.y());
        seen[r.y()*size.x() + r.x()] = true;
    };
    for (auto k = 0u; k <= line.steps(); k++)
    {
        mark(line.pixel_at(k));
        if (k == line.steps())
            break;
        const auto o = line.off_axis_at(k);
        for (auto i = 0u; i < 2; i++)
            if (o.diagonal && o.crossed[i])
                mark(o.pixels[i]);
    }

    for (auto y = lo.y(); y <= hi.y(); y++)
        for (auto x = lo.x(); x <= hi.x(); x++)
        {
            const bool expected = touches_square(d, {x, y});
            const bool actual = seen[(y - lo.y())*size.x() + (x - lo.x())];
            if (expected != actual)
                fm_abort("supercover of (%d %d): pixel (%d %d) expected %d, got %d",
                         d.x(), d.y(), x, y, (int)expected, (int)actual);
        }
}

void check_steps_within(Vector2i from, Vector2i to)
{
    const auto line = bresenham_line{from, to};
    for (auto axis = 0u; axis < 2; axis++)
    {
        const auto lo_min = Math::min(from[axis], to[axis]) - 2, hi_max = Math::max(from[axis], to[axis]) + 2;
        for (auto lo = lo_min; lo <= hi_max; lo++)
            for (auto hi = lo - 1; hi <= hi_max; hi++)
            {
                auto first = UINT32_MAX, last = 0u;
                for (auto k = 0u; k <= line.steps(); k++)
                {
                    const auto c = line.pixel_at(k)[axis];
                    if (c >= lo && c <= hi)
                    {
                        first = Math::min(first, k);
                        last = k;
                    }
                }
                const auto r = line.steps_within(axis, lo, hi);
                const bool ok = first == UINT32_MAX ? r.first > r.last : r.first == first && r.last == last;
                if (!ok)
                    fm_abort("line (%d %d)-(%d %d) axis %u [%d %d]: got [%u %u], expected [%u %u]",
                             from.x(), from.y(), to.x(), to.y(), axis, lo, hi, r.first, r.last, first, last);
            }
    }
}

// World pixels span about 2^26 per axis, where 2*step*minor reaches 2^53.
void check_long_line(Vector2i from, Vector2i to)
{
    const auto line = bresenham_line{from, to};
    const auto d = to - from;
    const bool y_major = Math::abs(d.y()) > Math::abs(d.x());
    const int64_t len = line.steps(), minor_len = Math::abs(y_major ? d.x() : d.y());
    fm_assert_equal(Math::abs(y_major ? d.y() : d.x()), (int32_t)len);

    for (int64_t k = 0; k <= len; k = k == len ? len + 1 : Math::min(k + 9973, len))
    {
        const auto p = line.pixel_at((uint32_t)k);
        fm_assert_equal((uint32_t)k, line.step_of(p));
        const int64_t m = Math::abs(y_major ? p.x() - from.x() : p.y() - from.y());
        // |m - k*minor_len/len| <= 1/2
        fm_assert(Math::abs(2*m*len - 2*k*minor_len) <= len);
        for (auto axis = 0u; axis < 2; axis++)
        {
            const auto r = line.steps_within(axis, p[axis], p[axis]);
            fm_assert(r.first <= k && k <= r.last);
            fm_assert(line.pixel_at(r.first)[axis] == p[axis] && line.pixel_at(r.last)[axis] == p[axis]);
            fm_assert(r.first == 0 || line.pixel_at(r.first - 1)[axis] != p[axis]);
            fm_assert(r.last == len || line.pixel_at(r.last + 1)[axis] != p[axis]);
        }
        if (k < len)
        {
            const auto q = line.pixel_at((uint32_t)k + 1);
            fm_assert(Math::abs(q.x() - p.x()) <= 1 && Math::abs(q.y() - p.y()) <= 1);
            fm_assert(Math::abs(y_major ? q.y() - p.y() : q.x() - p.x()) == 1);
        }
    }
}

} // namespace

void Test::test_raycast_line()
{
    fm_assert(test_plain_bresenham_misses());

    constexpr auto from = Vector2i{-3, 7};
    fm_assert(bresenham_line{from, from}.steps() == 0);
    fm_assert(bresenham_line{from, from}.pixel_at(0) == from);

    for (auto dy = -max_len; dy <= max_len; dy++)
        for (auto dx = -max_len; dx <= max_len; dx++)
        {
            const auto d = Vector2i{dx, dy};
            check_against_incremental(from, from + d);
            check_supercover(from, d);
            if (Math::abs(dx) <= 10 && Math::abs(dy) <= 10)
                check_steps_within(from, from + d);
        }

    constexpr int32_t e = 1 << 25;
    check_long_line({-e, -e + 1}, {e - 1, e - 3});
    check_long_line({e - 2, -e}, {-e + 5, e - 1});
}

} // namespace floormat
