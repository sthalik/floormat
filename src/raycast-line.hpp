#pragma once
#include <array>
#include <mg/Vector2.h>

namespace floormat::rc {

// The pixels of a Bresenham line from `from` to `to`, both included, indexed by step. Pixel k is
// k pixels from `from` along the major axis, the axis the line spans further (x when the spans
// are equal). Its coordinate on the other axis is k*minor/major rounded with ties away from
// `from`, so the line from `to` to `from` can differ on ties.
class bresenham_line
{
public:
    // For a step that changes both coordinates: the other two pixels of the 2x2 block that holds
    // pixel_at(step) and pixel_at(step + 1). pixels[0] has the x of step + 1 and the y of step,
    // pixels[1] the other way round.
    struct off_axis_pixels
    {
        std::array<Vector2i, 2> pixels;
        // crossed[i] is true when the segment from `from` to `to` touches the closed square of
        // pixels[i], where pixel (x, y) is [x - 1/2, x + 1/2] by [y - 1/2, y + 1/2], as in Dedu's
        // supercover line. A segment through the point where the four squares meet touches both.
        std::array<bool, 2> crossed;
        // True when the step changes both coordinates. When false, pixels and crossed hold zeros.
        bool diagonal;
    };

    // The steps first through last, both included. Empty when first > last.
    struct step_range
    {
        uint32_t first, last;
    };

    bresenham_line(Vector2i from, Vector2i to);

    // The number of steps from `from` to `to`. pixel_at() takes 0 through steps().
    uint32_t steps() const;
    Vector2i from() const;
    // 0 when x is the major axis, 1 when y.
    uint32_t major_axis() const;
    // How far the line spans along the minor axis. At most steps().
    uint32_t minor_len() const;
    // The direction the line moves along axis (0 for x, 1 for y): -1 or 1. It is 1 along an axis
    // the line does not move along.
    int32_t sign(uint32_t axis) const;
    // The pixel of step, 0 through steps(). pixel_at(0) is `from`, pixel_at(steps()) is `to`.
    Vector2i pixel_at(uint32_t step) const;
    // The off-axis pixels of the step from pixel_at(step) to pixel_at(step + 1). step must be
    // below steps().
    off_axis_pixels off_axis_at(uint32_t step) const;
    // The step whose pixel is `pixel`, which must be on the line. Debug builds assert it.
    uint32_t step_of(Vector2i pixel) const;
    // The steps whose pixel has its x (axis 0) or y (axis 1) in [lo, hi]. They are consecutive,
    // because both coordinates change monotonically along the line. {1, 0} when there are none.
    step_range steps_within(uint32_t axis, int32_t lo, int32_t hi) const;

private:
    // step*minor/major rounded with ties up, as an unsigned offset from `from`'s minor coordinate.
    uint64_t minor_at(uint64_t step) const;
    // The first step k with minor_at(k) >= m, for m <= minor_len, and 0 for m == 0.
    uint32_t first_step_at_minor(uint64_t m) const;
    // The pixel at `step` pixels from `from` along the major axis and `minor` along the minor
    // axis, each in the direction the line moves.
    Vector2i make_pixel(uint64_t step, uint64_t minor) const;

    Vector2i _from;
    uint32_t _major_len = 0, _minor_len = 0;
    int32_t _major_sign = 1, _minor_sign = 1;
    bool _y_major = false;
};

} // namespace floormat::rc
