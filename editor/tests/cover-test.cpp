#include "../tests-private.hpp"
#include "../app.hpp"
#include "../imgui-raii.hpp"
#include "compat/format.hpp"
#include "src/point.hpp"
#include "src/grid-cover.hpp"
#include "src/world.hpp"
#include "floormat/main.hpp"
#include <mg/Color.h>
#include <mg/Functions.h>

namespace floormat::tests {
namespace {

using namespace floormat::imgui;

constexpr inline uint32_t div_size = 8;

struct dir_button { const char* name; uint32_t k; bool large; };
constexpr dir_button compass_rose[3][3] = {
    {{"NW", 20, false}, {"N",  24, true},  {"NE", 28, false}},
    {{"W",  16, true},  {nullptr, 0, false}, {"E",  0,  true}},
    {{"SW", 12, false}, {"S",  8,  true},  {"SE", 4,  false}},
};

constexpr const char* compass_names_16[16] = {
    "E",   "ESE", "SE",  "SSE", "S",   "SSW", "SW",  "WSW",
    "W",   "WNW", "NW",  "NNW", "N",   "NNE", "NE",  "ENE",
};

constexpr Color4 color_selected{1, 0.843f, 0, 0.8f};

struct cover_test final : base_test
{
    Cover::Pool pool;

    cover_test();
    ~cover_test() noexcept override = default;

    bool handle_key(app& a, const key_event& e, bool is_down) override;
    bool handle_mouse_click(app& a, const mouse_button_event& e, bool is_down) override;
    bool handle_mouse_move(app& a, const mouse_move_event& e) override;
    void draw_overlay(app& a) override;
    void draw_ui(app& a, float width) override;
    void update_pre(app&, const Ns&) override {}
    void update_post(app& a, const Ns&) override;

    void extract(app& a, point pt);

    struct pending_s {
        point from;
    } pending = {};

    struct result_s
    {
        point from{};
        uint32_t cell_idx = 0;
    } result = {};

    int32_t selected_octant{};
    bool has_result : 1 = false, has_pending : 1 = false;
};

cover_test::cover_test(): pool{Cover::Params{ .div_size = div_size }} {}

bool cover_test::handle_key(app& a, const key_event& e, bool is_down)
{
    (void)a; (void)e; (void)is_down;
    return false;
}

bool cover_test::handle_mouse_click(app& a, const mouse_button_event& e, bool is_down)
{
    if (is_down)
        return false;

    switch (e.button)
    {
    case mouse_button_left: {
            if (auto pt = a.cursor_state().point())
            {
                has_pending = true;
                pending = { .from = *pt };
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

bool cover_test::handle_mouse_move(app& a, const mouse_move_event& e)
{
    (void)a; (void)e;
    return false;
}

void cover_test::draw_overlay(app& a)
{
    if (!has_result)
        return;

    auto& w = a.main().world();
    auto* c = w.chunk_at_memo(result.from.chunk3());
    if (!c)
        return;

    pool.maybe_mark_stale_all(w.frame_no());
    Cover::Grid cg = pool[*c];
    cg.build_if_stale();

    ImDrawList& draw = *ImGui::GetBackgroundDrawList();
    const auto pos = a.point_screen_pos(result.from);

    constexpr float pi = Math::Constants<float>::pi();
    const auto ds = (int)pool.params().div_size;
    const auto line_color = ImGui::ColorConvertFloat4ToU32({0, 1, 0, 0.6f});
    const auto sk = (uint32_t)selected_octant;

    {
        const uint32_t dc = cg.div_count();
        const uint32_t max_d = chunk_size_xy / pool.params().div_size;

        const auto chunk_nw = point{result.from.chunk3(), local_coords{0, 0},
                                    Vector2b{(int8_t)(-tile_size_xy/2), (int8_t)(-tile_size_xy/2)}};
        const auto p00 = a.point_screen_pos(chunk_nw);
        const auto pX  = a.point_screen_pos(chunk_nw + Vector2i{ds, 0});
        const auto pY  = a.point_screen_pos(chunk_nw + Vector2i{0, ds});
        const Vector2 dx = pX - p00;
        const Vector2 dy = pY - p00;

        const auto rgb = [](uint8_t d, uint32_t md) -> ImU32 {
            const float t = md > 0 ? Math::min(float(d) / float(md), 1.f) : 0.f;
            float r, g, b;
            if (t < 0.5f)
            {
                const float u = t * 2.f;
                r = 1.f - u; g = u; b = 0.f;
            }
            else
            {
                const float u = (t - 0.5f) * 2.f;
                r = 0.f; g = 1.f - u; b = u;
            }
            return ImGui::ColorConvertFloat4ToU32({r, g, b, 0.45f});
        };

        for (uint32_t cy = 0; cy < dc; cy++)
            for (uint32_t cx = 0; cx < dc; cx++)
            {
                const uint32_t idx = Cover::Grid::get_cell_index(cx, cy, dc);
                const uint8_t d = cg.distance(idx, sk);
                const auto color = rgb(d, max_d);
                const Vector2 base = p00 + dx * float(cx) + dy * float(cy);
                const Vector2 q1 = base + dx;
                const Vector2 q2 = base + dx + dy;
                const Vector2 q3 = base + dy;
                draw.AddQuadFilled({base.x(), base.y()},
                                   {q1.x(),   q1.y()},
                                   {q2.x(),   q2.y()},
                                   {q3.x(),   q3.y()},
                                   color);
            }
    }

    for (uint32_t k = 0; k < Cover::octant_count; k++)
    {
        if (k == sk)
            continue;
        const auto theta = Rad{2.f * pi * float(k) / float(Cover::octant_count)};
        const Vector2 dir{Math::cos(theta), Math::sin(theta)};
        const auto len_px = (int)cg.distance(result.cell_idx, k) * ds;
        const auto end_pt = point::normalize_coords(result.from, Vector2i(dir * (float)len_px));
        const auto end_px = a.point_screen_pos(end_pt);
        draw.AddLine({pos.x(), pos.y()}, {end_px.x(), end_px.y()}, line_color);
    }

    {
        const auto theta = Rad{2.f * pi * float(sk) / float(Cover::octant_count)};
        const Vector2 dir{Math::cos(theta), Math::sin(theta)};
        const auto len_px = (int)cg.distance(result.cell_idx, sk) * ds;
        const auto end_pt = point::normalize_coords(result.from, Vector2i(dir * (float)len_px));
        const auto end_px = a.point_screen_pos(end_pt);
        const auto hi_color = ImGui::ColorConvertFloat4ToU32({1, 0.2f, 0.2f, 0.95f});
        constexpr float hi_thickness = 2.5f;
        draw.AddLine({pos.x(), pos.y()}, {end_px.x(), end_px.y()}, hi_color, hi_thickness);
        constexpr float marker_radius = 4;
        draw.AddCircleFilled({end_px.x(), end_px.y()}, marker_radius, hi_color);
    }

    const auto dot_color = ImGui::ColorConvertFloat4ToU32({1, 0.5f, 0, 1});
    constexpr float dot_radius = 5;
    draw.AddCircleFilled({pos.x(), pos.y()}, dot_radius, dot_color);
}

void cover_test::update_post(app& a, const Ns&)
{
    if (has_pending)
    {
        has_pending = false;
        extract(a, pending.from);
        return;
    }
    if (!has_result)
        return;
    auto& w = a.main().world();
    auto* c = w.chunk_at_memo(result.from.chunk3());
    if (!c)
        return;
    pool.maybe_mark_stale_all(w.frame_no());
    Cover::Grid cg = pool[*c];
    const auto sk = (uint32_t)selected_octant;
    if (cg.ensure_octant(sk))
        return;
    cg.fill_next_unfilled();
}

void cover_test::extract(app& a, point pt)
{
    auto& w = a.main().world();
    auto* c = w.chunk_at_memo(pt.chunk3());
    if (!c)
        return;

    pool.maybe_mark_stale_all(w.frame_no());
    Cover::Grid g = pool[*c];
    g.build_if_stale();

    const auto idx = g.get_cell_index_from_coord(pt.local(), pt.offset());

    has_result = false;
    result = {
        .from = pt,
        .cell_idx = idx,
    };
    has_result = true;
}

void cover_test::draw_ui(app&, float)
{
    constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
    constexpr auto colflags_1 = ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoSort;
    constexpr auto colflags_0 = colflags_1 | ImGuiTableColumnFlags_WidthFixed;

    char buf[128];

    if (!has_result)
        return;

    auto from_c = Vector3i(result.from.chunk3());
    auto from_l = Vector2i(result.from.local());
    auto from_p = Vector2i(result.from.offset());

    constexpr auto print_coord = [](auto&& buf, Vector3i c, Vector2i l, Vector2i p)
    {
        snformat(buf, "(ch {}x{}) <{}x{}> {{{}x{} px}}"_cf, c.x(), c.y(), l.x(), l.y(), p.x(), p.y());
    };

    constexpr auto do_column = [](StringView name)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        text(name);
        ImGui::TableNextColumn();
    };

    if (auto b1 = begin_table("##cover_results", 2, table_flags))
    {
        ImGui::TableSetupColumn("##name", colflags_0);
        ImGui::TableSetupColumn("##value", colflags_1 | ImGuiTableColumnFlags_WidthStretch);

        do_column("from");
        print_coord(buf, from_c, from_l, from_p);
        text(buf);

        do_column("div_size");
        snformat(buf, "{}"_cf, pool.params().div_size);
        text(buf);

        do_column("octant");

        constexpr auto fmt_octant_label = [](auto&& out, uint32_t kk) {
            const float dd = 360.f * float(kk) / float(Cover::octant_count);
            snformat(out, "{} (k={}, {:.1f}°)"_cf, compass_names_16[kk / 2], kk, dd);
        };

        fmt_octant_label(buf, (uint32_t)selected_octant);
        text(buf);

        static const float label_max_w = [&] {
            char tmp[64];
            float w = 0;
            for (uint32_t kk = 0; kk < Cover::octant_count; kk++)
            {
                fmt_octant_label(tmp, kk);
                w = Math::max(w, ImGui::CalcTextSize(tmp).x);
            }
            return w;
        }();
        ImGui::SameLine(label_max_w + ImGui::GetStyle().ItemSpacing.x);

        {
            int sel = selected_octant;
            ImGui::SetNextItemWidth(180);
            {
                auto b2 = push_style_var(ImGuiStyleVar_FramePadding, {ImGui::GetStyle().FramePadding.x, 0});
                ImGui::SliderInt("##sel_octant", &sel, 0, (int)Cover::octant_count - 1);
            }
            selected_octant = sel;
        }

        {
            ImGui::Dummy({0, 10});

            const float row_h = ImGui::GetFrameHeight();
            const ImVec2 big_sz{40, row_h};
            constexpr ImGuiTableFlags rose_flags = ImGuiTableFlags_SizingFixedFit;
            if (auto b2 = begin_table("##rose", 3, rose_flags))
            {
                for (auto& row : compass_rose)
                {
                    ImGui::TableNextRow();
                    for (auto& cell : row)
                    {
                        ImGui::TableNextColumn();
                        if (!cell.name)
                        {
                            ImGui::Dummy(big_sz);
                            continue;
                        }
                        const bool is_selected = (uint32_t)selected_octant == cell.k;
                        [[maybe_unused]] const raii_wrapper colors[] = {
                            is_selected ? push_style_color(ImGuiCol_Button, color_selected) : raii_wrapper{},
                            is_selected ? push_style_color(ImGuiCol_ButtonHovered, color_selected) : raii_wrapper{},
                        };
                        if (cell.large)
                        {
                            if (ImGui::Button(cell.name, big_sz))
                                selected_octant = (int32_t)cell.k;
                        }
                        else
                        {
                            if (ImGui::SmallButton(cell.name))
                                selected_octant = (int32_t)cell.k;
                        }
                    }
                }
            }
        }
    }
}

} // namespace

Pointer<base_test> tests_data::make_test_cover() { return Pointer<cover_test>{InPlaceInit}; }

} // namespace floormat::tests
