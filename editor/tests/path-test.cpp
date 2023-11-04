#include "../tests-private.hpp"
#include "../app.hpp"
#include "floormat/main.hpp"
#include "src/path-search.hpp"
#include "src/critter.hpp"
#include "shaders/shader.hpp"
#include "../imgui-raii.hpp"
#include "src/camera-offset.hpp"
#include <Magnum/Math/Functions.h>

namespace floormat::tests {

bool path_test::handle_key(app& a, const key_event& e, bool is_down)
{
    (void)a; (void)e; (void)is_down;
    return false;
}

bool path_test::handle_mouse_click(app& a, const mouse_button_event& e, bool is_down)
{
    if (is_down)
        return false;

    switch (e.button)
    {
    case mouse_button_left: {
        auto& M = a.main();
        auto& w = M.world();
        auto C = a.ensure_player_character(w);
        if (auto pt = a.cursor_state().point())
        {
            constexpr auto chunk_size = iTILE_SIZE2 * TILE_MAX_DIM;
            auto pt0 = C->position();
            auto vec = Math::abs((pt->coord() - pt0.coord())) * iTILE_SIZE2 * 2 + chunk_size * 1;
            auto dist = (uint32_t)vec.length();

            has_pending = true;
            pending = { .from = pt0, .to = *pt, .own_id = C->id,
                        .max_dist = dist, .own_size = C->bbox_size, };
        }
        return true;
    }
    case mouse_button_right: {
        bool ret = false;
        if (has_pending)
        {
            has_pending = false;
            pending = {};
            ret = true;
        }
        if (has_result)
        {
            has_result = false;
            result = {};
            ret = true;
        }
        return ret;
    }
    default:
        return false;
    }
}

bool path_test::handle_mouse_move(app& a, const mouse_move_event& e)
{
    (void)a; (void)e;
    return false;
}

void path_test::draw_overlay(app& a)
{
    if (!has_result)
        return;

    const auto win_size = a.main().window_size();
    const auto line_color = ImGui::ColorConvertFloat4ToU32({0, 0, 1, 1});
    const auto dot_color = ImGui::ColorConvertFloat4ToU32({1, 0, 0, 1});
    constexpr float line_thickness = 3, dot_radius = 5;
    auto& shader = a.main().shader();
    ImDrawList& draw = *ImGui::GetForegroundDrawList();

    constexpr auto get_screen_pos = [](tile_shader& shader, point pt, Vector2i win_size) {
        auto c3 = pt.chunk3();
        auto c2 = pt.chunk();
        with_shifted_camera_offset co{shader, c3, c2, c2 };
        auto world_pos = TILE_SIZE20 * Vector3(pt.local()) + Vector3(Vector2(pt.offset()), 0);
        return Vector2(shader.camera_offset()) + Vector2(win_size)*.5f + shader.project(world_pos);
    };

    auto last = get_screen_pos(shader, result.from, win_size);
    draw.AddCircleFilled({last.x(), last.y()}, dot_radius, dot_color);

    for (auto pt : result.path)
    {
        auto pos = get_screen_pos(shader, pt, win_size);
        draw.AddLine({pos.x(), pos.y()}, {last.x(), last.y()}, line_color, line_thickness);
        draw.AddCircleFilled({pos.x(), pos.y()}, dot_radius, dot_color);
        last = pos;
    }
}

void path_test::update_pre(app& a)
{
    if (!has_pending)
        return;

    has_result = false;
    has_pending = false;

    auto& M = a.main();
    auto& w = M.world();
    auto& astar = M.astar();

    auto res = astar.Dijkstra(w, pending.from, pending.to, pending.own_id, pending.max_dist, pending.own_size, 1);
    if (res)
    {
        has_result = true;
        result.from = pending.from;
        result.path = res.path();
    }
}

void path_test::update_post(app& a)
{
    (void)a;
}

} // namespace floormat::tests
