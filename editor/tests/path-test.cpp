#include "../tests-private.hpp"
#include "../app.hpp"
#include "compat/shared-ptr-wrapper.hpp"
#include "compat/vector-wrapper.hpp"
#include "floormat/main.hpp"
#include "src/search-astar.hpp"
#include "src/search-result.hpp"
#include "src/critter.hpp"
#include "shaders/shader.hpp"
#include "../imgui-raii.hpp"
#include <cr/Optional.h>
#include <mg/Functions.h>
#include <mg/Color.h>

namespace floormat::tests {

using namespace floormat::imgui;

struct path_test final : base_test
{
    bool handle_key(app& a, const key_event& e, bool is_down) override;
    bool handle_mouse_click(app& a, const mouse_button_event& e, bool is_down) override;
    bool handle_mouse_move(app& a, const mouse_move_event& e) override;
    void draw_overlay(app& a) override;
    void draw_ui(app& a, float width) override;
    void update_pre(app& a, const Ns& dt) override;
    void update_post(app&, const Ns&) override {}

    struct pending_s
    {
        point from, to;
        object_id own_id;
        uint32_t max_dist;
        Vector2ui own_size;
    } pending = {};

    struct result_s
    {
        point from, to;
        Optional<path_search_result> res;
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
                        .max_dist = dist,
                        .own_size = Vector2ui(C->bbox_size) + Vector2ui(2), };
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
    fm_assert(result.res);
    const auto& res = *result.res;

    const auto line_color = ImGui::ColorConvertFloat4ToU32({0, 0, 1, 1});
    const auto dot_color = ImGui::ColorConvertFloat4ToU32({1, 0, 0, 1});
    constexpr float line_thickness = 3, dot_radius = 5;
    ImDrawList& draw = *ImGui::GetForegroundDrawList();

    auto last = a.point_screen_pos(result.from);
    draw.AddCircleFilled({last.x(), last.y()}, dot_radius, dot_color);



    for (auto pt : res.path())
    {
        auto pos = a.point_screen_pos(pt);
        draw.AddLine({pos.x(), pos.y()}, {last.x(), last.y()}, line_color, line_thickness);
        draw.AddCircleFilled({pos.x(), pos.y()}, dot_radius, dot_color);
        last = pos;
    }

    if (!res.is_found() && !res.path().isEmpty())
    {
        auto pos = a.point_screen_pos(res.path().back());
        constexpr float spacing = 12, size1 = 7, size2 = 3, spacing2 = spacing + size2;

        draw.AddLine({pos.x() - spacing2, pos.y() - spacing2},
                     {pos.x() + spacing2, pos.y() + spacing2},
                     line_color, size1);
        draw.AddLine({pos.x() + spacing2, pos.y() - spacing2},
                     {pos.x() - spacing2, pos.y() + spacing2},
                     line_color, size1);
        draw.AddLine({pos.x() + spacing,  pos.y() - spacing},
                     {pos.x() - spacing,  pos.y() + spacing},
                     dot_color, size2);
        draw.AddLine({pos.x() - spacing,  pos.y() - spacing},
                     {pos.x() + spacing,  pos.y() + spacing},
                     dot_color, size2);
    }
}

void path_test::update_pre(app& a, const Ns&)
{
    if (!has_pending)
        return;

    has_result = false;
    has_pending = false;

    auto& M = a.main();
    auto& w = M.world();
    auto& astar = M.astar();

    auto res = astar.Dijkstra(w, pending.from, pending.to, pending.own_id, pending.max_dist, pending.own_size, 1);
    has_result = !!res;
    result = {
        .from = pending.from,
        .to = pending.to,
        .res = has_result ? move(res) : Optional<path_search_result>{},
    };
}

void path_test::draw_ui(app&, float)
{
    constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
    constexpr auto colflags_1 = ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoSort;
    constexpr auto colflags_0 = colflags_1 | ImGuiTableColumnFlags_WidthFixed;

    char buf[128];

    if (!has_result)
        return;

    fm_assert(result.res);
    const auto& res = *result.res;

    auto from_c = Vector3i(result.from.chunk3()), to_c = Vector3i(result.to.chunk3());
    auto from_l = Vector2i(result.from.local()), to_l = Vector2i(result.to.local());
    auto from_p = Vector2i(result.from.offset()), to_p = Vector2i(result.to.offset());

    constexpr auto print_coord = [](auto&& buf, Vector3i c, Vector2i l, Vector2i p)
    {
        std::snprintf(buf, std::size(buf), "(ch %dx%d) <%dx%d> {%dx%d px}", c.x(), c.y(), l.x(), l.y(), p.x(), p.y());
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
        if (res.is_found())
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
                std::snprintf(buf, std::size(buf), "%d", (int)res.distance());
                text(buf);
            }
        }

        do_column("cost");
        std::snprintf(buf, std::size(buf), "%d", (int)res.cost());
        text(buf);

        do_column("length");
        std::snprintf(buf, std::size(buf), "%d", (int)res.path().size());
        text(buf);

        do_column("time");
        std::snprintf(buf, std::size(buf), "%.1f ms", (double)(1000 * res.time()));
        text(buf);
    }
}

Pointer<base_test> tests_data::make_test_path() { return Pointer<path_test>{InPlaceInit}; }

} // namespace floormat::tests
