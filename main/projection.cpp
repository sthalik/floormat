#include "main-impl.hpp"
#include "src/tile-constants.hpp"
#include <algorithm>
#include <limits>

namespace floormat {

global_coords main_impl::pixel_to_tile(Vector2d position) const noexcept
{
    auto vec = pixel_to_tile_(position);
    auto vec_ = Math::floor(vec);
    return { (int32_t)vec_.x(), (int32_t)vec.y(), 0 };
}

Vector2d main_impl::pixel_to_tile_(Vector2d position) const noexcept
{
    constexpr Vector2d pixel_size{tile_size_xy};
    constexpr Vector2d half{.5, .5};
    const Vector2d px = position - Vector2d{window_size()}*.5 - _shader.camera_offset();
    return tile_shader::unproject(px*.5) / pixel_size + half;
}

auto main_impl::get_draw_bounds() const noexcept -> draw_bounds
{
    using limits = std::numeric_limits<int16_t>;
    auto x0 = limits::max(), x1 = limits::min(), y0 = limits::max(), y1 = limits::min();

    const auto win = Vector2d(window_size());

    chunk_coords list[] = {
        pixel_to_tile(Vector2d{0, 0}).chunk(),
        pixel_to_tile(Vector2d{win[0]-1, 0}).chunk(),
        pixel_to_tile(Vector2d{0, win[1]-1}).chunk(),
        pixel_to_tile(Vector2d{win[0]-1, win[1]-1}).chunk(),
    };

    auto center = pixel_to_tile(Vector2d{win[0]/2, win[1]/2}).chunk();

    for (auto p : list)
    {
        x0 = Math::min(x0, p.x);
        x1 = Math::max(x1, p.x);
        y0 = Math::min(y0, p.y);
        y1 = Math::max(y1, p.y);
    }
    // todo test this with the default viewport size using --magnum-dpi-scaling=1
    x0 -= 1; y0 -= 1; x1 += 1; y1 += 1;

    constexpr int16_t mx = tile_shader::max_screen_tiles.x()/(int16_t)2,
                      my = tile_shader::max_screen_tiles.y()/(int16_t)2;
    int16_t minx = center.x - mx + 1, maxx = center.x + mx,
            miny = center.y - my + 1, maxy = center.y + my;

    x0 = Math::clamp(x0, minx, maxx);
    x1 = Math::clamp(x1, minx, maxx);
    y0 = Math::clamp(y0, miny, maxy);
    y1 = Math::clamp(y1, miny, maxy);

    return {x0, x1, y0, y1};
}

bool floormat_main::check_chunk_visible(const Vector2d& offset, const Vector2i& size) noexcept
{
    constexpr Vector3d len = dTILE_SIZE * TILE_MAX_DIM20d;
    enum : size_t { x, y, };
    constexpr Vector2d p00 = tile_shader::project(Vector3d(0, 0, 0)),
                       p10 = tile_shader::project(Vector3d(len[x], 0, 0)),
                       p01 = tile_shader::project(Vector3d(0, len[y], 0)),
                       p11 = tile_shader::project(Vector3d(len[x], len[y], 0));
    constexpr auto xx = std::minmax({ p00[x], p10[x], p01[x], p11[x], }),
                   yy = std::minmax({ p00[y], p10[y], p01[y], p11[y], });
    constexpr auto minx = xx.first, maxx = xx.second, miny = yy.first, maxy = yy.second;
    constexpr int W = (int)(maxx - minx + .5 + 1e-16), H = (int)(maxy - miny + .5 + 1e-16);
    const auto X = (int)(minx + (offset[x] + size[x])*.5), Y = (int)(miny + (offset[y] + size[y])*.5);
    return X + W > 0 && X < size[x] && Y + H > 0 && Y < size[y];
}


}
