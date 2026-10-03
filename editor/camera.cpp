#include "app.hpp"
#include "src/tile-constants.hpp"
#include "src/global-coords.hpp"
#include "src/point.inl"
#include "shaders/shader.hpp"
#include "floormat/main.hpp"
#include "src/RTree-search.hpp"
#include "src/object.hpp"
#include "src/world.hpp"
#include "src/camera-offset.hpp"
#include "src/timer.hpp"
#include "compat/enum-bitset.hpp"
#include "compat/borrowed-ptr.hpp"
#include <bit>
#include <mg/Range.h>
#include <mg/Functions.h>

namespace floormat {

void app::do_camera(const Ns& dt, const key_set& cmds, int mods)
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
        const auto max_camera2 = sz * 20;

        _camera_remainder += dir.normalized() * (double)Time::to_seconds(dt) * pixels_per_second;
        const auto step2 = Vector2i(Math::round(_camera_remainder*2));
        _camera_remainder -= Vector2d(step2)*.5;
        shader.set_camera2(Math::clamp(shader.camera2() - step2, -max_camera2, max_camera2));

        update_cursor_tile(cursor.pixel);
        do_mouse_move(mods);
    }
}

void app::reset_camera_offset()
{
    M->shader().set_camera2(tile_shader::project2(Vector3i{-chunk_size<Vector2i>/2, 0}));
    _camera_remainder = {};
    _z_level = 0;
    update_cursor_tile(cursor.pixel);
}

object_id app::get_object_colliding_with_cursor()
{
    const auto chunks = M->get_draw_bounds(_chunk_bounds_array, {});

    auto& world = M->world();

    using rtree_type = std::decay_t<decltype(*world[{}].rtree())>;
    using rect_type = rtree_type::Rect;

    if (cursor.pixel)
    {
        const auto pt = M->pixel_to_point(*cursor.pixel, _z_level);

        for (auto ch : chunks)
        {
            if (ch.z != _z_level)
                continue;
            auto* cʹ = world.at(ch);
            if (!cʹ)
                continue;
            auto& c = *cʹ;
            c.ensure_passability();
            auto t0 = Vector2(pt - point{ch, {}, {}});
            auto t1 = t0+Vector2(1e-4f);
            const auto* rtree = c.rtree();
            object_id ret = 0;
            rtree->Search(t0.data(), t1.data(), [&](uint64_t data, const rect_type& rect) {
                [[maybe_unused]] auto x = std::bit_cast<collision_data>(data);
                if (x.type == (uint64_t)collision_type::geometry)
                    return true;
                Vector2 min{rect.m_min}, max{rect.m_max};
                if (t0 >= min && t0 <= max)
                {
                    if (auto e_ = world.find_object(x.id);
                        e_ && Vector2ui(e_->bbox_size).product() != 0)
                    {
                        ret = x.id;
                        return false;
                    }
                }
                return true;
            });
            if (ret)
                return ret;
        }
    }
    return 0;
}

void app::update_cursor_tile(const Optional<Vector2i>& pixel)
{
    cursor.pixel = pixel;
    // assert_invariant !!cursor.tile == !!cursor.subpixel;
    if (pixel)
    {
        const auto pt = M->pixel_to_point(*pixel, _z_level);
        cursor.tile = pt.coord();
        cursor.subpixel = pt.offset();
    }
    else
    {
        cursor.tile = NullOpt;
        cursor.subpixel = NullOpt;
    }
}

void app::center_camera_on(point pt)
{
    // Solve towards the integer pixel rather than the true center: an odd window size makes
    // win/2 fractional, and half a pixel is still a whole unit of point::offset.
    _z_level = pt.chunk3().z;
    const auto win = M->window_size();
    const auto target = win/2;
    const auto camera2 = 2*target - win - tile_shader::project2(Vector3i(pt));
    M->shader().set_camera2(camera2);
    _camera_remainder = {};
    update_cursor_tile(target);
}

void app::set_cursor_at(point pt)
{
    fm_assert(pt.chunk3().z == _z_level);
    update_cursor_tile(Vector2i{Math::round(point_to_pixel(pt))});
}

Vector2 app::point_to_pixel(point pt)
{
    auto& shader = M->shader();
    auto win_size = M->window_size();
    auto c3 = pt.chunk3();
    with_shifted_camera_offset co{shader, c3};
    auto world_pos = TILE_SIZE20 * Vector3(pt.local()) + Vector3(Vector2(pt.offset()), 0);
    return Vector2(shader.camera2() + win_size)*.5f + shader.project(world_pos);
}

} // namespace floormat
