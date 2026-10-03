#include "main-impl.hpp"
#include "src/tile-constants.hpp"
#include "src/point.hpp"
#include "src/camera-offset.hpp"
#include <cr/Pair.h>
#include <cr/GrowableArray.h>
#include <cr/StructuredBindings.h>
#include <mg/Range.h>
#include <mg/Functions.h>
#include <algorithm>
#include <array>

namespace floormat {

namespace {

// The visibility test only sees a chunk's floor rhombus, but sprites anchored
// inside it overhang it: tall or wide frames, wall tops, group.offset shifts.
constexpr int chunk_overhang_x = tile_size_xy * 4;
constexpr int chunk_overhang_y = tile_size_z * 2;

template<size_t N>
Pair<int, int> project_interval(const std::array<Vector2i, N>& pts, Vector2i axis) noexcept
{
    int mn = Math::dot(pts[0], axis);
    int mx = mn;
    for (std::size_t i = 1; i < N; ++i)
    {
        const int p = Math::dot(pts[i], axis);
        mn = Math::min(mn, p);
        mx = Math::max(mx, p);
    }
    return {mn, mx};
}

bool sat_rhombus_vs_rect(const std::array<Vector2i, 4>& poly, Range2Di screen_rect) noexcept
{
    const std::array<Vector2i, 4> rect = {
        screen_rect.min(),
        Vector2i(screen_rect.max().x(), screen_rect.min().y()),
        screen_rect.max(),
        Vector2i(screen_rect.min().x(), screen_rect.max().y()),
    };

    // Edge normals of the rhombus, divided by chunk_size so the dot products fit in int32.
    constexpr std::array<Vector2i, 4> axes = {
        Vector2i{1, 0},
        Vector2i{0, 1},
        Vector2i{1, -2},
        Vector2i{1, 2},
    };

    for (Vector2i axis : axes)
    {
        const auto [a0, a1] = project_interval(poly, axis);
        const auto [b0, b1] = project_interval(rect, axis);
        if (a1 < b0 || b1 < a0)
            return false;
    }

    return true;
}

// In doubled pixels, the units of camera2 and project2().
bool check_chunk_visible(Vector2i camera2, Vector2i win) noexcept
{
    constexpr auto len = chunk_size<int32_t>;
    const Vector2i origin2 = win + camera2;

    const std::array<Vector2i, 4> rhombus = {
        tile_shader::project2(Vector3i{0,   0,   0}) + origin2,
        tile_shader::project2(Vector3i{len, 0,   0}) + origin2,
        tile_shader::project2(Vector3i{len, len, 0}) + origin2,
        tile_shader::project2(Vector3i{0,   len, 0}) + origin2,
    };

    const Range2Di screen_rect{
        2*Vector2i{-chunk_overhang_x, -chunk_overhang_y},
        2*Vector2i{ chunk_overhang_x + win.x(), chunk_overhang_y + win.y()},
    };

    return sat_rhombus_vs_rect(rhombus, screen_rect);
}

} // namespace

global_coords main_impl::pixel_to_tile(Vector2i position, int8_t z_level) const noexcept
{
    return pixel_to_point(position, z_level).coord();
}

point main_impl::pixel_to_point(Vector2i pixel, int8_t z_level) const noexcept
{
    return tile_shader::pixel_to_point(pixel, window_size(), _shader.camera2(), z_level);
}

ArrayView<chunk_coords_> main_impl::get_draw_bounds(Array<chunk_coords_>& output, Range2Di extra_pixels) const noexcept
{
    arrayResize(output, 0);

    const Vector2i win = window_size();

    const auto pixel_to_chunk = [this](Vector2i screen_pos) {
        return Vector2i(pixel_to_tile(screen_pos).chunk());
    };

    constexpr auto z_height = chunk_z_count*tile_size_z;
    static_assert(z_height >= 0);

    const auto p00 = pixel_to_chunk({         -chunk_overhang_x + extra_pixels.min().x(),          -z_height - chunk_overhang_y + extra_pixels.min().y()});
    const auto p10 = pixel_to_chunk({win.x() + chunk_overhang_x + extra_pixels.max().x(),          -z_height - chunk_overhang_y + extra_pixels.min().y()});
    const auto p01 = pixel_to_chunk({         -chunk_overhang_x + extra_pixels.min().x(), win.y() + z_height + chunk_overhang_y + extra_pixels.max().y()});
    const auto p11 = pixel_to_chunk({win.x() + chunk_overhang_x + extra_pixels.max().x(), win.y() + z_height + chunk_overhang_y + extra_pixels.max().y()});

    Vector2i min_xy = Math::min(Math::min(p00, p10), Math::min(p01, p11));
    Vector2i max_xy = Math::max(Math::max(p00, p10), Math::max(p01, p11));

#if 0
    if (extra_pixels.min().isZero() && extra_pixels.max().isZero())
        DBG << "min" << min_xy << "max" << max_xy;
#endif

    const Vector2i span = max_xy - min_xy + Vector2i{1, 1};
    const Vector2i base_camera2 = _shader.camera2();

    fm_assert(span >= Vector2i{});
    arrayReserve(output, size_t((span.x()+1) * (span.y()+1)) * size_t{chunk_z_count});

#if 0
    if (extra_pixels.min().isZero() && extra_pixels.max().isZero())
        DBG << ">>>";
#endif

    for (int z = int(chunk_z_max); z >= int(chunk_z_min); --z)
        for (int y = max_xy.y(); y >= min_xy.y(); --y)
            for (int x = max_xy.x(); x >= min_xy.x(); --x)
            {
                const chunk_coords_ ch{(int16_t)x, (int16_t)y, (int8_t)z};
                if (_world.contains(ch))
                {
                    const Vector2i camera2 = base_camera2 + with_shifted_camera_offset::get_projected_chunk_offset2(ch);
#if 0
                    if (extra_pixels.min().isZero() && extra_pixels.max().isZero())
                        DBG << "  test" << ch << check_chunk_visible(camera2, win);
#endif

                    if (check_chunk_visible(camera2, win))
                    {
                        arrayAppend(output, ch);
#if 0
                        if (extra_pixels.min().isZero() && extra_pixels.max().isZero())
                            DBG << "  added" << ch;
#endif
                    }
                }
            }

#if 0
    if (extra_pixels.min().isZero() && extra_pixels.max().isZero())
        DBG << "<<<" << arraySize(output);
#endif

    return output;
}

} // namespace floormat
