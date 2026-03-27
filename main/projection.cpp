#include "main-impl.hpp"
#include "src/tile-constants.hpp"
#include "src/point.hpp"
#include "compat/limits.hpp"
#include "src/camera-offset.hpp"
#include <floormat/draw-bounds.hpp>
#include <cr/Pair.h>
#include <cr/GrowableArray.h>
#include <cr/StructuredBindings.h>
#include <mg/Range.h>
#include <algorithm>
#include <limits>
#include <array>

namespace floormat {

namespace {

double dot2(const Vector2d& a, const Vector2d& b) noexcept
{
    return a[0]*b[0] + a[1]*b[1];
}

template<size_t N>
Pair<double, double> project_interval(const std::array<Vector2d, N>& pts, const Vector2d& axis) noexcept
{
    double mn = dot2(pts[0], axis);
    double mx = mn;
    for (std::size_t i = 1; i < N; ++i)
    {
        const double p = dot2(pts[i], axis);
        mn = Math::min(mn, p);
        mx = Math::max(mx, p);
    }
    return {mn, mx};
}

bool sat_rhombus_vs_rect(const std::array<Vector2d, 4>& poly, Range2Di screen_rect) noexcept
{
    const std::array<Vector2d, 4> rect = {
        Vector2d(screen_rect.min().x(), screen_rect.min().y()),
        Vector2d(screen_rect.max().x(), screen_rect.min().y()),
        Vector2d(screen_rect.max().x(), screen_rect.max().y()),
        Vector2d(screen_rect.min().x(), screen_rect.max().y()),
    };

    const Vector2d e0 = poly[1] - poly[0];
    const Vector2d e1 = poly[3] - poly[0];

    const std::array<Vector2d, 4> axes = {
        Vector2d{1, 0},
        Vector2d{0, 1},
        Vector2d{e0[1], -e0[0]},
        Vector2d{e1[1], -e1[0]},
    };

    for (Vector2d axis : axes)
    {
        const auto [a0, a1] = project_interval(poly, axis);
        const auto [b0, b1] = project_interval(rect, axis);
        if (a1 < b0 || b1 < a0)
            return false;
    }

    return true;
}

} // namespace

global_coords main_impl::pixel_to_tile(Vector2d position, int8_t z_level) const noexcept
{
    auto vec = pixel_to_tile_(position);
    auto vec_ = Math::floor(vec);
    return { (int32_t)vec_.x(), (int32_t)vec_.y(), z_level };
}

Vector2d main_impl::pixel_to_tile_(Vector2d position) const noexcept
{
    constexpr Vector2d pixel_size{tile_size_xy};
    constexpr Vector2d half{.5, .5};
    const Vector2d px = position - Vector2d{window_size()}*.5 - _shader.camera_offset();
    return tile_shader::unproject(px*.5) / pixel_size + half;
}

point main_impl::pixel_to_point(Vector2d pixel, int8_t z_level) const noexcept
{
    const auto tileʹ = pixel_to_tile_(pixel);
    const auto tileʹʹ = Math::floor(tileʹ);
    const auto tile = global_coords{(int)tileʹʹ.x(), (int)tileʹʹ.y(), z_level};
    const auto subpixelʹ = Math::fmod(Vector2(tileʹ), 1.f);
    const auto subpixelʹ_neg = Vector2{Vector2i(tile.chunk()) < Vector2i{}};
    auto subpixel = TILE_SIZE2 * (subpixelʹ + subpixelʹ_neg);
    constexpr auto half_tile = Vector2(iTILE_SIZE2/2);
    subpixel -= half_tile;
    subpixel = Math::clamp(Math::round(subpixel), -half_tile, half_tile-Vector2{1.f});
    return point{ tile, Vector2b{subpixel} };
}

ArrayView<chunk_coords_> main_impl::get_draw_bounds(Array<chunk_coords_>& output, Range2Di extra_pixels) const noexcept
{
    arrayResize(output, 0);

    const Vector2i win = window_size();

    const auto pixel_to_chunk = [this](Vector2i screen_pos) {
        return Vector2i(pixel_to_tile(Vector2d(screen_pos)).chunk());
    };

    constexpr auto z_height = (chunk_z_max - chunk_z_min + 1)*tile_size_z;
    static_assert(z_height >= 0);

    const auto p00 = pixel_to_chunk({         -tile_size_xy + extra_pixels.min().x(),          -z_height + extra_pixels.min().y()});
    const auto p10 = pixel_to_chunk({win.x() + tile_size_xy + extra_pixels.max().x(),          -z_height + extra_pixels.min().y()});
    const auto p01 = pixel_to_chunk({         -tile_size_xy + extra_pixels.min().x(), win.y() + z_height + extra_pixels.max().y()});
    const auto p11 = pixel_to_chunk({win.x() + tile_size_xy + extra_pixels.max().x(), win.y() + z_height + extra_pixels.max().y()});

    Vector2i min_xy = Math::min(Math::min(p00, p10), Math::min(p01, p11));
    Vector2i max_xy = Math::max(Math::max(p00, p10), Math::max(p01, p11));

#if 0
    if (extra_pixels.min().isZero() && extra_pixels.max().isZero())
        DBG << "min" << min_xy << "max" << max_xy;
#endif

    const Vector2i span = max_xy - min_xy + Vector2i{1, 1};
    constexpr auto z_count = size_t{int(chunk_z_max) - int(chunk_z_min) + 1};
    const Vector2d base_camera = _shader.camera_offset();

    fm_assert(span >= Vector2i{});
    arrayReserve(output, size_t((span.x()+1) * (span.y()+1)) * z_count);

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
                    const Vector2d effective_offset = base_camera + with_shifted_camera_offset::get_projected_chunk_offset(ch);
#if 0
                    if (extra_pixels.min().isZero() && extra_pixels.max().isZero())
                        DBG << "  test" << ch << check_chunk_visible(effective_offset, win);
#endif

                    if (check_chunk_visible(effective_offset, win))
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

bool floormat_main::check_chunk_visible(Vector2d offset, Vector2i win) noexcept
{
    // Chunk footprint in world XY, projected to an isometric rhombus.
    constexpr Vector3d len = dTILE_SIZE * TILE_MAX_DIM20d;

    std::array<Vector2d, 4> rhombus = {
        tile_shader::project(Vector3d{0.,       0.,       0.}),
        tile_shader::project(Vector3d{len.x(),  0.,       0.}),
        tile_shader::project(Vector3d{len.x(),  len.y(),  0.}),
        tile_shader::project(Vector3d{0.,       len.y(),  0.}),
    };

    // Same mapping used by rendering / pixel_to_tile inverse:
    // screen = project(world)*2 + win*0.5 + camera_offset
    const Vector2d origin = Vector2d{win}*.5 + offset;
    for (Vector2d& p : rhombus)
        p += origin;

    // Only top vertical expansion by one z tile.
    const Range2Di screen_rect{
        Vector2i{-tile_size_xy, -tile_size_z},
        Vector2i{ tile_size_xy +  win.x(), tile_size_z + win.y()},
    };

#if 0
    DBG << "rhombus" << rhombus;
#endif

    return sat_rhombus_vs_rect(rhombus, screen_rect);
}

} // namespace floormat
