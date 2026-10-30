#pragma once
#include <array>
#include <mg/Vector2.h>

namespace floormat::rc {

// The points of a Bresenham line from `start_pos` to `end_pos`, both included, indexed by step.
// Point k is k units from `start_pos` along the major axis, the axis the line spans further (x
// when the spans are equal). Its coordinate on the other axis is k*minor/major rounded with ties
// away from `start_pos`, so the line from `end_pos` to `start_pos` can differ on ties.
class bresenham_line
{
public:
    // For a step that changes both coordinates: the other two points of the 2x2 block that holds
    // point_at(step) and point_at(step + 1). points[0] has the x of step + 1 and the y of step,
    // points[1] the other way round.
    struct orthogonal_points
    {
        std::array<Vector2i, 2> points;
        // in_supercover[i] is true when the segment from `start_pos` to `end_pos` touches the
        // closed square of points[i], where point (x, y) is [x - 1/2, x + 1/2] by
        // [y - 1/2, y + 1/2], as in Dedu's supercover line. A segment through the point where the
        // four squares meet touches both.
        std::array<bool, 2> in_supercover;
        // True when the step changes both coordinates. When false, points and in_supercover hold
        // zeros.
        bool diagonal;
    };

    // The steps first through last, both included. Empty when first > last.
    struct step_interval
    {
        uint32_t first, last;
    };

    bresenham_line(Vector2i start_pos, Vector2i end_pos);

    // The number of steps from `start_pos` to `end_pos`. point_at() takes 0 through steps().
    uint32_t steps() const;
    Vector2i start_pos() const;
    // 0 when x is the major axis, 1 when y.
    uint32_t major_axis() const;
    // How far the line spans along the minor axis. At most steps().
    uint32_t minor_len() const;
    // The direction the line moves along axis (0 for x, 1 for y): -1 or 1. It is 1 along an axis
    // the line does not move along.
    int32_t sign(uint32_t axis) const;
    // The point of step, 0 through steps(). point_at(0) is `start_pos`, point_at(steps()) is
    // `end_pos`.
    Vector2i point_at(uint32_t step) const;
    // The orthogonal points of the step from point_at(step) to point_at(step + 1). step must be
    // below steps().
    orthogonal_points orthogonal_at(uint32_t step) const;
    // The step whose point is pt, which must be on the line. Debug builds assert it.
    uint32_t step_of(Vector2i pt) const;
    // The steps whose point has its x (axis 0) or y (axis 1) in [lo, hi]. They are consecutive,
    // because both coordinates change monotonically along the line. {1, 0} when there are none.
    step_interval interval_bounds_to_steps(uint32_t axis, int32_t lo, int32_t hi) const;

private:
    // step*minor/major rounded with ties up, as an unsigned offset from `start_pos`'s minor
    // coordinate.
    uint64_t minor_at(uint32_t step) const;
    // The first step k with minor_at(k) >= m, for m <= minor_len, and 0 for m == 0.
    uint32_t first_step_at_minor(uint64_t m) const;
    // The point `step` units from `start_pos` along the major axis and `minor` along the minor
    // axis, each in the direction the line moves. The octant is _y_major, _major_sign and
    // _minor_sign.
    Vector2i octant_to_point(uint32_t step, uint64_t minor) const;

    Vector2i _start_pos;
    uint32_t _major_len = 0, _minor_len = 0;
    int32_t _major_sign = 1, _minor_sign = 1;
    bool _y_major = false;
};

} // namespace floormat::rc
