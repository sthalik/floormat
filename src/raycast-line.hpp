#pragma once
#include <array>
#include <mg/Vector2.h>

namespace floormat::rc {

// Bresenham line from `from` to `to`, both included. Step k moves one pixel along the major axis.
// Its minor coordinate is k*minor/major rounded with ties away from `from`, so the reversed line
// can differ on ties.
class bresenham_line
{
public:
    // The two pixels beside the step from pixel_at(step) to pixel_at(step + 1). pixels[0] has the
    // x of step + 1 and the y of step, pixels[1] the other way round.
    struct off_axis_pixels
    {
        std::array<Vector2i, 2> pixels;
        // The segment touches the pixel's closed square, as in Dedu's supercover line.
        // At an exact corner it touches both.
        std::array<bool, 2> crossed;
        // When false, pixels and crossed are unset.
        bool diagonal;
    };

    // Empty when first > last.
    struct step_range
    {
        uint32_t first, last;
    };

    bresenham_line(Vector2i from, Vector2i to);

    uint32_t steps() const;
    Vector2i from() const;
    uint32_t major_axis() const;
    uint32_t minor_len() const;
    // -1 or 1
    int32_t sign(uint32_t axis) const;
    Vector2i pixel_at(uint32_t step) const;
    off_axis_pixels off_axis_at(uint32_t step) const;
    // pixel must be one of pixel_at's.
    uint32_t step_of(Vector2i pixel) const;
    // The steps whose pixel has its x (axis 0) or y (axis 1) in [lo, hi].
    step_range steps_within(uint32_t axis, int32_t lo, int32_t hi) const;

private:
    uint64_t minor_at(uint64_t step) const;
    uint32_t first_step_at_minor(uint64_t m) const;
    Vector2i make_pixel(uint64_t step, uint64_t minor) const;

    Vector2i _from;
    uint32_t _major_len = 0, _minor_len = 0;
    int32_t _major_sign = 1, _minor_sign = 1;
    bool _y_major = false;
};

} // namespace floormat::rc
