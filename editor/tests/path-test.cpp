#include "../tests-private.hpp"
#include "../app.hpp"
#include "../imgui-raii.hpp"
#include "compat/format.inl"
#include "compat/nanosecond.hpp"
#include "compat/timer.hpp"
#include "floormat/main.hpp"
#include "src/search-astar.hpp"
#include "src/search-result.hpp"
#include "src/grid-pass.hpp"
#include "src/critter.hpp"
#include "src/point.inl"
#include "shaders/shader.hpp"
#include <cr/Optional.h>
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
        u32 max_dist;
        Vector2ui own_size;
    } pending = {};

    struct result_s
    {
        point from, to;
        Optional<path_search_result> res;
        astar::string_pull_result pull;
        float search_time = 0, pull_time = 0;
        u32 nodes = 0;
        bool pulled = false;
    } result;
    bool has_result : 1 = false, has_pending : 1 = false;
    bool smoothing = true, closest_on_failure = true;
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
        auto C = a.ensure_player_character(w);
        if (auto pt = a.cursor_state().point())
        {
            auto pt0 = C->position();
            auto vec = Math::abs((pt->coord() - pt0.coord())) * iTILE_SIZE2 * 2 + chunk_size<Vector2i> * 1;
            auto dist = (u32)vec.length();

            has_pending = true;
            pending = { .from = pt0, .to = *pt,
                        .max_dist = dist,
                        .own_size = C->circumscribed_bbox_size(), };
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

    const bool smoothed = !res.smoothings().isEmpty();
    const float raw_alpha = smoothed ? .35f : 1;
    const auto line_color = ImGui::ColorConvertFloat4ToU32({0, 0, 1, 1});
    const auto dot_color = ImGui::ColorConvertFloat4ToU32({1, 0, 0, 1});
    const auto raw_line_color = ImGui::ColorConvertFloat4ToU32({0, 0, 1, raw_alpha});
    const auto raw_dot_color = ImGui::ColorConvertFloat4ToU32({1, 0, 0, raw_alpha});
    constexpr float line_thickness = 3, raw_line_thickness = 5, dot_radius = 5;
    ImDrawList& draw = *ImGui::GetForegroundDrawList();

    auto last = a.point_to_pixel(result.from);
    draw.AddCircleFilled(ImVec2(last), dot_radius, dot_color);



    for (auto pt : res.path())
    {
        auto pos = a.point_to_pixel(pt);
        draw.AddLine(ImVec2(pos), ImVec2(last), raw_line_color, raw_line_thickness);
        draw.AddCircleFilled(ImVec2(pos), dot_radius, raw_dot_color);
        last = pos;
    }

    if (const auto sp = res.smoothings(); !sp.isEmpty())
    {
        const auto sp_dot_color = ImGui::ColorConvertFloat4ToU32({1, 1, 0, 1});
        auto prev = a.point_to_pixel(sp[0].pt);
        for (const auto& x : sp)
        {
            auto pos = a.point_to_pixel(x.pt);
            draw.AddLine(ImVec2(pos), ImVec2(prev), line_color, line_thickness);
            draw.AddCircleFilled(ImVec2(pos), dot_radius, sp_dot_color);
            prev = pos;
        }
    }

    if (!res.is_found() && !res.path().isEmpty())
    {
        auto pos = a.point_to_pixel(res.path().back());
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

    result.res = {}; // return back to the pool to preserve cpu cache
    const auto& p = Search::without_critters();
    const auto t0 = Time::now();
    auto res = closest_on_failure
             ? astar.Dijkstra<1, true>(w, pending.from, pending.to, pending.max_dist, pending.own_size, p)
             : astar.Dijkstra<1, false>(w, pending.from, pending.to, pending.max_dist, pending.own_size, p);
    const auto search_time = Time::to_seconds(Time::now() - t0);
    const auto nodes = astar.last_node_count();
    const bool pulled = smoothing && !!res;
    astar::string_pull_result pull;
    float pull_time = 0;
    if (pulled)
    {
        const auto t1 = Time::now();
        pull = astar.string_pull(w, res, pending.own_size, p);
        pull_time = Time::to_seconds(Time::now() - t1);
    }
    has_result = true;
    result = {
        .from = pending.from,
        .to = pending.to,
        .res = move(res),
        .pull = pull,
        .search_time = search_time,
        .pull_time = pull_time,
        .nodes = nodes,
        .pulled = pulled,
    };
}

void path_test::draw_ui(app&, float)
{
    constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg;
    constexpr auto colflags_1 = ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoSort;
    constexpr auto colflags_0 = colflags_1 | ImGuiTableColumnFlags_WidthFixed;
    constexpr auto green = 0x00ff00ff_rgbaf, yellow = 0xffff00ff_rgbaf, red = 0xff6060ff_rgbaf;

    char buf[128];

    ImGui::TextWrapped("Left click searches from the player to the cursor. Right click clears.");

    if (ImGui::Checkbox("Smoothing", &smoothing) && has_result)
        has_pending = true;
    if (ImGui::Checkbox("Path to the closest point on failure", &closest_on_failure) && has_result)
        has_pending = true;
    ImGui::SetItemTooltip("Unticked, a failed search returns no path, and can stop early on a goal it proves walled off.");

    if (!has_result)
        return;

    fm_assert(result.res);
    const auto& res = *result.res;

    constexpr auto print_coord = [](auto&& buf, point pt)
    {
        const auto c = Vector3i(pt.chunk3());
        const auto l = Vector2i(pt.local());
        const auto o = Vector2i(pt.offset());
        snformat(buf, "chunk ({}, {}), tile ({}, {})\noffset ({}, {}) px", c.x(), c.y(), l.x(), l.y(), o.x(), o.y());
    };

    constexpr auto do_column = [](StringView name)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        text(name);
        ImGui::TableNextColumn();
    };

    constexpr auto colored_text = [](StringView str, const Color4& color)
    {
        auto b = push_style_color(ImGuiCol_Text, color);
        text(str);
    };

    // Both tables' first columns take the widest name, so their values line up.
    const float name_width = Math::max(ImGui::CalcTextSize("corners added").x, ImGui::CalcTextSize("closest point").x);

    ImGui::SeparatorText("Search");
    if (auto b1 = begin_table("##search_results", 2, table_flags))
    {
        ImGui::TableSetupColumn("##name", colflags_0, name_width);
        ImGui::TableSetupColumn("##value", colflags_1 | ImGuiTableColumnFlags_WidthStretch);

        do_column("from");
        print_coord(buf, result.from);
        text(buf);

        do_column("to");
        print_coord(buf, result.to);
        text(buf);

        do_column("result");
        if (res.is_found())
            colored_text("found", green);
        else
        {
            colored_text("not found", red);
            if (res)
            {
                do_column("closest point");
                snformat(buf, "{} px from the goal", res.distance());
                colored_text(buf, yellow);
            }
        }

        if (res)
        {
            do_column("cost");
            snformat(buf, "{}", res.cost());
            text(buf);

            const auto raw = res.path();
            float raw_len = 0;
            for (auto i = 0uz; i + 1 < raw.size(); i++)
                raw_len += Vector2(raw[i+1] - raw[i]).length();
            do_column("length");
            snformat(buf, "{:.1f} px, {} vertices", raw_len, raw.size());
            text(buf);
        }

        do_column("nodes");
        snformat(buf, "{}", result.nodes);
        text(buf);

        do_column("time");
        snformat(buf, "{:.1f} µs", (double)(1e6f * result.search_time));
        text(buf);
    }

    if (!result.pulled)
        return;

    ImGui::SeparatorText("Smoothing");
    if (auto b1 = begin_table("##smoothing_results", 2, table_flags))
    {
        ImGui::TableSetupColumn("##name", colflags_0, name_width);
        ImGui::TableSetupColumn("##value", colflags_1 | ImGuiTableColumnFlags_WidthStretch);

        do_column("result");
        if (result.pull)
            colored_text("smoothed", green);
        else
            colored_text("gave up", red);

        const auto sp = res.smoothings();
        float sp_len = 0;
        for (auto i = 0uz; i + 1 < sp.size(); i++)
            sp_len += Vector2(sp[i+1].pt - sp[i].pt).length();
        do_column("length");
        snformat(buf, "{:.1f} px, {} vertices", sp_len, sp.size());
        text(buf);

        do_column("corners added");
        snformat(buf, "{}", result.pull.corners_added);
        text(buf);

        do_column("LOS checks");
        snformat(buf, "{}", result.pull.los_checks);
        text(buf);

        do_column("time");
        snformat(buf, "{:.1f} µs", (double)(1e6f * result.pull_time));
        text(buf);
    }
}

Pointer<base_test> tests_data::make_test_path() { return Pointer<path_test>{InPlaceInit}; }

} // namespace floormat::tests
