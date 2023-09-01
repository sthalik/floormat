#include "app.hpp"
#include "src/global-coords.hpp"
#include "shaders/shader.hpp"
#include "floormat/main.hpp"
#include "src/RTree-search.hpp"
#include "src/world.hpp"
#include "src/camera-offset.hpp"
#include <algorithm>

namespace floormat {

void app::do_camera(float dt, const key_set& cmds, int mods)
{
    if (cmds[key_camera_reset])
    {
        reset_camera_offset();
        update_cursor_tile(cursor.pixel);
        do_mouse_move(mods);
        return;
    }

    Vector2d dir{};

    if (cmds[key_camera_up])
        dir += Vector2d{0, -1};
    else if (cmds[key_camera_down])
        dir += Vector2d{0,  1};
    if (cmds[key_camera_left])
        dir += Vector2d{-1, 0};
    else if (cmds[key_camera_right])
        dir += Vector2d{1,  0};

    if (dir != Vector2d{})
    {
        auto& shader = M->shader();
        const auto sz = M->window_size();
        constexpr double screens_per_second = 0.75;

        const double pixels_per_second = sz.length() / screens_per_second;
        auto camera_offset = shader.camera_offset();
        const auto max_camera_offset = Vector2d(sz * 10);

        camera_offset -= dir.normalized() * (double)dt * pixels_per_second;
        camera_offset[0] = std::clamp(camera_offset[0], -max_camera_offset[0], max_camera_offset[0]);
        camera_offset[1] = std::clamp(camera_offset[1], -max_camera_offset[1], max_camera_offset[1]);

        shader.set_camera_offset(camera_offset, shader.depth_offset());

        update_cursor_tile(cursor.pixel);
        do_mouse_move(mods);
    }
}

void app::reset_camera_offset()
{
    constexpr Vector3d size = TILE_MAX_DIM20d*dTILE_SIZE*-.5;
    constexpr auto projected = tile_shader::project(size);
    M->shader().set_camera_offset(projected, 0);
    _z_level = 0;
    update_cursor_tile(cursor.pixel);
}

object_id app::object_at_cursor()
{
    auto [z_min, z_max, z_cur, only] = get_z_bounds();
    if (only)
        z_min = z_max = z_cur;
    const auto [minx, maxx, miny, maxy] = M->get_draw_bounds();
    const auto sz = M->window_size();
    auto& world = M->world();
    auto& shader = M->shader();

    using rtree_type = std::decay_t<decltype(*world[chunk_coords_{}].rtree())>;
    using rect_type = typename rtree_type::Rect;

    if (cursor.pixel)
    {
        auto pos = tile_shader::project(Vector3d{0., 0., -_z_level*dTILE_SIZE[2]});
        auto pixel = Vector2d{*cursor.pixel} + pos;
        auto coord = M->pixel_to_tile(pixel);
        auto tile = global_coords{coord.chunk(), coord.local(), 0};

        constexpr auto eps = 1e-6f;
        constexpr auto m = TILE_SIZE2 * Vector2(1- eps, 1- eps);
        const auto tile_ = Vector2(M->pixel_to_tile_(Vector2d(pixel)));
        const auto curchunk = Vector2(tile.chunk()), curtile = Vector2(tile.local());
        const auto subpixel_ = Vector2(std::fmod(tile_[0], 1.f), std::fmod(tile_[1], 1.f));
        const auto subpixel = m * Vector2(curchunk[0] < 0 ? 1 + subpixel_[0] : subpixel_[0],
                                          curchunk[1] < 0 ? 1 + subpixel_[1] : subpixel_[1]);
        for (int16_t y = miny; y <= maxy; y++)
            for (int16_t x = minx; x <= maxx; x++)
            {
                const chunk_coords_ c_pos{x, y, _z_level};
                auto* c_ = world.at(c_pos);
                if (!c_)
                    continue;
                auto& c = *c_;
                c.ensure_passability();
                const with_shifted_camera_offset o{shader, c_pos, {minx, miny}, {maxx, maxy}};
                if (floormat_main::check_chunk_visible(shader.camera_offset(), sz))
                {
                    constexpr auto half_tile = TILE_SIZE2/2;
                    constexpr auto chunk_size = TILE_SIZE2 * TILE_MAX_DIM;
                    auto chunk_dist = (curchunk - Vector2(c_pos.x, c_pos.y))*chunk_size;
                    auto t0 = chunk_dist + curtile*TILE_SIZE2 + subpixel - half_tile;
                    auto t1 = t0+Vector2(1e-4f);
                    const auto* rtree = c.rtree();
                    object_id ret = 0;
                    rtree->Search(t0.data(), t1.data(), [&](uint64_t data, const rect_type& rect) {
                        [[maybe_unused]] auto x = std::bit_cast<collision_data>(data);
                        if (x.tag == (uint64_t)collision_type::geometry)
                            return true;
                        Vector2 min(rect.m_min[0], rect.m_min[1]), max(rect.m_max[0], rect.m_max[1]);
                        if (t0 >= min && t0 <= max)
                        {
                            if (auto e_ = world.find_object(x.data);
                                e_ && Vector2ui(e_->bbox_size).product() != 0)
                            {
                                ret = x.data;
                                return false;
                            }
                        }
                        return true;
                    });
                    if (ret)
                        return ret;
                }
            }
    }
    return 0;
}

void app::update_cursor_tile(const Optional<Vector2i>& pixel)
{
    cursor.pixel = pixel;
    if (pixel)
    {
        auto coord = M->pixel_to_tile(Vector2d{*pixel});
        cursor.tile = global_coords{coord.chunk(), coord.local(), _z_level};
    }
    else
        cursor.tile = NullOpt;
}

} // namespace floormat
