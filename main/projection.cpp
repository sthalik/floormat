#include "main-impl.hpp"
#include "src/tile-constants.hpp"
#include "src/point.hpp"
#include "src/camera-offset.hpp"
#include <cr/Pair.h>
#include <cr/GrowableArray.h>
#include <cr/StructuredBindings.h>
#include <mg/Range.h>
#include <mg/Functions.h>
#include <array>

namespace floormat {

namespace {

// The visibility test only sees a chunk's floor rhombus, but sprites anchored
// inside it overhang it: tall or wide frames, wall tops, group.offset shifts.
constexpr int chunk_overhang_x = tile_size_xy * 4;
constexpr int chunk_overhang_y = tile_size_z * 2;

template<usize N>
Pair<int, int> project_interval(const std::array<Vector2i, N>& pts, Vector2i axis) noexcept
{
    int mn = Math::dot(pts[0], axis);
    int mx = mn;
    for (usize i = 1; i < N; ++i)
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

bool check_chunk_visible(Vector2i camera, Vector2i win, Range2Di extra_pixels) noexcept
{
    // the first tile is centered on the chunk origin, so the floor starts half a tile before it
    constexpr auto lo = -half_tile<i32>, hi = chunk_size<i32> - half_tile<i32>;
    const Vector2i origin = win/2 + camera;

    // Chunk corners have even x+y, so the halving is exact.
    const std::array<Vector2i, 4> rhombus = {
        tile_shader::projectʹ(Vector3i{lo, lo, 0})/2 + origin,
        tile_shader::projectʹ(Vector3i{hi, lo, 0})/2 + origin,
        tile_shader::projectʹ(Vector3i{hi, hi, 0})/2 + origin,
        tile_shader::projectʹ(Vector3i{lo, hi, 0})/2 + origin,
    };

    const Range2Di screen_rect{
        Vector2i{-chunk_overhang_x, -chunk_overhang_y} + extra_pixels.min(),
        Vector2i{ chunk_overhang_x + win.x(), chunk_overhang_y + win.y()} + extra_pixels.max(),
    };

    return sat_rhombus_vs_rect(rhombus, screen_rect);
}

} // namespace

global_coords main_impl::pixel_to_tile(Vector2i position, i8 z_level) const noexcept
{
    return pixel_to_point(position, z_level).coord();
}

point main_impl::pixel_to_point(Vector2i pixel, i8 z_level) const noexcept
{
    return tile_shader::pixel_to_point(pixel, window_size(), _shader.camera_offsetʹ(), z_level);
}

ArrayView<chunk_coords_> main_impl::get_draw_bounds(Array<chunk_coords_>& output, Range2Di extra_pixels) const noexcept
{
    arrayResize(output, 0);

    const Vector2i win = window_size();
    const Vector2i base_camera = _shader.camera_offsetʹ();
    const Vector2i lo = Vector2i{-chunk_overhang_x, -chunk_overhang_y} + extra_pixels.min();
    const Vector2i hi = win + Vector2i{chunk_overhang_x, chunk_overhang_y} + extra_pixels.max();

    for (i32 z = chunk_z_max; z >= chunk_z_min; --z)
    {
        if (!_world.has_chunks_at_z(i8(z)))
            continue;

        const auto pixel_to_chunk = [&](Vector2i screen_pos) {
            return Vector2i(pixel_to_point(screen_pos, i8(z)).chunk());
        };
        const auto p00 = pixel_to_chunk(lo), p10 = pixel_to_chunk({hi.x(), lo.y()});
        const auto p01 = pixel_to_chunk({lo.x(), hi.y()}), p11 = pixel_to_chunk(hi);
        const Vector2i min_xy = Math::min(Math::min(p00, p10), Math::min(p01, p11));
        const Vector2i max_xy = Math::max(Math::max(p00, p10), Math::max(p01, p11));

        const usize n = arraySize(output) + usize((max_xy - min_xy + Vector2i{1}).product());
        if (arrayCapacity(output) < n)
            arrayReserve(output, n + n/2);

        for (i32 y = max_xy.y(); y >= min_xy.y(); --y)
            for (i32 x = max_xy.x(); x >= min_xy.x(); --x)
            {
                const chunk_coords_ ch{i16(x), i16(y), i8(z)};
                if (_world.contains(ch))
                {
                    const Vector2i camera = base_camera + with_shifted_camera_offset::get_projected_chunk_offset(ch);
                    if (check_chunk_visible(camera, win, extra_pixels))
                        arrayAppend(output, ch);
                }
            }
    }

    return output;
}

} // namespace floormat
