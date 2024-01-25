#include "../tests-private.hpp"
#include "../app.hpp"
#include "compat/shared-ptr-wrapper.hpp"
#include "floormat/main.hpp"
#include "src/path-search.hpp"
#include "src/critter.hpp"
#include "shaders/shader.hpp"
#include "../imgui-raii.hpp"
#include "src/camera-offset.hpp"
#include <Magnum/Math/Functions.h>
#include <magnum/Math/Color.h>

namespace floormat::tests {

using namespace floormat::imgui;

struct path_test : base_test
{
    bool handle_key(app& a, const key_event& e, bool is_down) override;
    bool handle_mouse_click(app& a, const mouse_button_event& e, bool is_down) override;
    bool handle_mouse_move(app& a, const mouse_move_event& e) override;
    void draw_overlay(app& a) override;
    void draw_ui(app& a, float width) override;
    void update_pre(app& a) override;
    void update_post(app& a) override;

    struct pending_s
    {
        point from, to;
        object_id own_id;
        uint32_t max_dist;
        Vector2ub own_size;
    } pending = {};

    struct result_s
    {
        point from, to;
        std::vector<point> path;
        float time;
        uint32_t cost, distance;
        bool found : 1;
    } result;

    bool has_result : 1 = false, has_pending : 1 = false;
};


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
        auto C = a.ensure_player_character(w).ptr;
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
        result = {
            .from = pending.from,
            .to = pending.to,
            .path = std::move(res.path()),
            .time = res.time(),
            .cost = res.cost(),
            .distance = res.distance(),
            .found = res.is_found(),
        };
    }
}

void path_test::update_post(app& a)
{
    (void)a;
}

void path_test::draw_ui(app& a, float width)
{
    constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
    constexpr auto colflags_1 = ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoSort;
    constexpr auto colflags_0 = colflags_1 | ImGuiTableColumnFlags_WidthFixed;

    char buf[128];
    const auto& res = result;

    if (!has_result)
        return;

    auto from_c = Vector3i(res.from.chunk3()), to_c = Vector3i(res.to.chunk3());
    auto from_l = Vector2i(res.from.local()), to_l = Vector2i(res.to.local());
    auto from_p = Vector2i(res.from.offset()), to_p = Vector2i(res.to.offset());

    constexpr auto print_coord = [](auto&& buf, Vector3i c, Vector2i l, Vector2i p)
    {
        std::snprintf(buf, std::size(buf), "(%dx%d) <%dx%d> {%dx%d}", c.x(), c.y(), l.x(), l.y(), p.x(), p.y());
    };

    constexpr auto do_column = [](StringView name)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        text(name);
        ImGui::TableNextColumn();
    };

    if (auto b1 = begin_table("##search_results", 2, table_flags))
    {
        ImGui::TableSetupColumn("##name", colflags_0);
        ImGui::TableSetupColumn("##value", colflags_1 | ImGuiTableColumnFlags_WidthStretch);

        do_column("from");
        print_coord(buf, from_c, from_l, from_p);
        text(buf);

        do_column("to");
        print_coord(buf, to_c, to_l, to_p);
        text(buf);

        do_column("found?");
        if (res.found)
        {
            auto b = push_style_color(ImGuiCol_Text, 0x00ff00ff_rgbaf);
            text("yes");
        }
        else
        {
            {
                auto b = push_style_color(ImGuiCol_Text, 0xff0000ff_rgbaf);
                text("no");
            }
            {
                auto b = push_style_color(ImGuiCol_Text, 0xffff00ff_rgbaf);
                do_column("dist");
                std::snprintf(buf, std::size(buf), "%d", (int)res.distance);
                text(buf);
            }
        }

        do_column("cost");
        std::snprintf(buf, std::size(buf), "%d", (int)res.cost);
        text(buf);

        do_column("length");
        std::snprintf(buf, std::size(buf), "%d", (int)res.path.size());
        text(buf);
    }
}

std::unique_ptr<base_test> tests_data::make_test_path() { return std::make_unique<path_test>(); }

} // namespace floormat::tests
