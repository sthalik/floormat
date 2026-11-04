#include "../tests-private.hpp"
#include "../app.hpp"
#include "../imgui-raii.hpp"
#include "compat/format.hpp"
#include "src/point.hpp"
#include "src/intra-coord.inl"
#include "src/grid-cover.hpp"
#include "src/grid-pass.hpp"
#include "src/world.hpp"
#include "floormat/main.hpp"
#include <mg/Color.h>
#include <mg/Functions.h>

namespace floormat::tests {
namespace {

using namespace floormat::imgui;

constexpr inline u32 default_div_size = 8;
constexpr u32 div_sizes[] = { 1, 2, 4, 8, 16, 32, 64 };
// odd, so the clicked chunk is in the middle
constexpr u32 chunk_spans[] = { 1, 3, 5, 7 };

struct dir_button { const char* name; u32 k; bool large; };
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
    Pointer<Pass::Pool> pass_pool;
    Pointer<Cover::Pool> pool;
    u32 chunk_span = 1;

    cover_test();
    ~cover_test() noexcept override = default;

    bool handle_key(app& a, const key_event& e, bool is_down) override;
    bool handle_mouse_click(app& a, const mouse_button_event& e, bool is_down) override;
    bool handle_mouse_move(app& a, const mouse_move_event& e) override;
    void draw_overlay(app& a) override;
    void draw_ui(app& a, float width) override;
    void update_pre(app&, const Ns&) override {}
    void update_post(app& a, const Ns&) override;
    Value advance(app& a, Value) override;

    void extract(app& a, point pt);
    void set_div_size(app& a, u32 div_size);
    chunk* chunk_at(world& w, i32 dx, i32 dy) const;
    void draw_chunk(app& a, chunk& c, u32 sk);

    struct pending_s {
        point from;
    } pending = {};

    struct result_s
    {
        point from{};
        u32 cell_idx = 0;
    } result = {};

    i32 selected_octant{};
    bool has_result : 1 = false, has_pending : 1 = false;
};

cover_test::cover_test():
    pass_pool{InPlaceInit, Pass::Params{default_div_size, pass_mode::shoot_through}, Search::without_critters()},
    pool{InPlaceInit, Cover::Params{default_div_size}, *pass_pool}
{}

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

chunk* cover_test::chunk_at(world& w, i32 dx, i32 dy) const
{
    const auto ch = result.from.chunk3();
    return w.at(chunk_coords_{(i16)(ch.x + dx), (i16)(ch.y + dy), ch.z});
}

void cover_test::draw_chunk(app& a, chunk& c, u32 sk)
{
    Cover::Grid cg = (*pool)[c];
    cg.build_if_stale();
    if (!(cg.built_octants() & (1u << sk)))
        return;

    ImDrawList& draw = *ImGui::GetBackgroundDrawList();
    const auto ds = (int)pool->params().div_size;
    const u32 dc = cg.div_count();
    const u32 max_d = chunk_size_xy / pool->params().div_size;

    const auto chunk_nw = point{c.coord(), intra_coord{}};
    const auto p00 = a.point_to_pixel(chunk_nw);
    const auto pX  = a.point_to_pixel(chunk_nw + Vector2i{ds, 0});
    const auto pY  = a.point_to_pixel(chunk_nw + Vector2i{0, ds});
    const Vector2 dx = pX - p00;
    const Vector2 dy = pY - p00;

    const auto rgb = [](u16 d, u32 md) -> ImU32 {
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
    const auto blocked_color = ImGui::ColorConvertFloat4ToU32({0.5f, 0.5f, 0.5f, 0.45f});

    for (u32 cy = 0; cy < dc; cy++)
        for (u32 cx = 0; cx < dc; cx++)
        {
            const u32 idx = Cover::Grid::get_cell_index(cx, cy, dc);
            const u16 d = cg.distance(idx, sk);
            const auto color = d == Cover::blocked ? blocked_color : rgb(d, max_d);
            const Vector2 base = p00 + dx * float(cx) + dy * float(cy);
            draw.AddQuadFilled(ImVec2(base), ImVec2(base + dx), ImVec2(base + dx + dy), ImVec2(base + dy), color);
        }
}

void cover_test::draw_overlay(app& a)
{
    if (!has_result)
        return;

    auto& w = a.main().world();
    auto* c = w.at(result.from.chunk3());
    if (!c)
        return;

    pool->maybe_mark_stale_all(w);

    const auto sk = (u32)selected_octant;
    const auto r = (i32)chunk_span / 2;
    for (i32 dy = -r; dy <= r; dy++)
        for (i32 dx = -r; dx <= r; dx++)
            if (auto* ch = chunk_at(w, dx, dy))
                draw_chunk(a, *ch, sk);

    Cover::Grid cg = (*pool)[*c];
    cg.build_if_stale();

    ImDrawList& draw = *ImGui::GetBackgroundDrawList();
    const auto pos = a.point_to_pixel(result.from);

    const auto ds = (int)pool->params().div_size;
    const auto line_color = ImGui::ColorConvertFloat4ToU32({0, 1, 0, 0.6f});

    const auto built = cg.built_octants();
    for (u32 k = 0; k < Cover::octant_count; k++)
    {
        if (k == sk || !(built & 1u << k) || cg.distance(result.cell_idx, k) == Cover::blocked)
            continue;
        const auto dir = Cover::octant_direction(k);
        const auto len_px = (int)cg.distance(result.cell_idx, k) * ds;
        const auto end_pt = point::normalize_coords(result.from, Vector2i(dir * (float)len_px));
        const auto end_px = a.point_to_pixel(end_pt);
        draw.AddLine(ImVec2(pos), ImVec2(end_px), line_color);
    }

    if (built & 1u << sk && cg.distance(result.cell_idx, sk) != Cover::blocked)
    {
        const auto dir = Cover::octant_direction(sk);
        const auto len_px = (int)cg.distance(result.cell_idx, sk) * ds;
        const auto end_pt = point::normalize_coords(result.from, Vector2i(dir * (float)len_px));
        const auto end_px = a.point_to_pixel(end_pt);
        const auto hi_color = ImGui::ColorConvertFloat4ToU32({1, 0.2f, 0.2f, 0.95f});
        constexpr float hi_thickness = 2.5f;
        draw.AddLine(ImVec2(pos), ImVec2(end_px), hi_color, hi_thickness);
        constexpr float marker_radius = 4;
        draw.AddCircleFilled(ImVec2(end_px), marker_radius, hi_color);
    }

    const auto dot_color = ImGui::ColorConvertFloat4ToU32({1, 0.5f, 0, 1});
    constexpr float dot_radius = 5;
    draw.AddCircleFilled(ImVec2(pos), dot_radius, dot_color);
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
    auto* c = w.at(result.from.chunk3());
    if (!c)
        return;
    pool->maybe_mark_stale_all(w);
    const auto sk = (u32)selected_octant;
    if ((*pool)[*c].ensure_octant(sk))
        return;
    const auto r = (i32)chunk_span / 2;
    for (i32 dy = -r; dy <= r; dy++)
        for (i32 dx = -r; dx <= r; dx++)
            if (auto* ch = chunk_at(w, dx, dy); ch && ch != c)
                if ((*pool)[*ch].ensure_octant(sk))
                    return;
    (*pool)[*c].fill_next_unfilled();
}

base_test::Value cover_test::advance(app& a, Value)
{
    Value ret;
    ret.type = ValueType::u64;
    ret.u64 = 0;

    selected_octant = (i32)(((u32)selected_octant + 1u) % Cover::octant_count);
    if (!has_result)
        return ret;
    auto& w = a.main().world();
    auto* c = w.at(result.from.chunk3());
    if (!c)
        return ret;
    // The handle has to come after the stale sweep -- a pooled grid can be recycled by it.
    pool->maybe_mark_stale_all(w);
    ret.u32 = (*pool)[*c].built_octants();
    ret.type = ValueType::u32;
    return ret;
}

void cover_test::extract(app& a, point pt)
{
    auto& w = a.main().world();
    auto* c = w.at(pt.chunk3());
    if (!c)
        return;

    pool->maybe_mark_stale_all(w);
    Cover::Grid g = (*pool)[*c];
    g.build_if_stale();

    const auto idx = g.get_cell_index_from_coord(intra_coord{pt});

    has_result = false;
    result = {
        .from = pt,
        .cell_idx = idx,
    };
    has_result = true;
}

void cover_test::set_div_size(app& a, u32 div_size)
{
    if (div_size == pool->params().div_size)
        return;
    pool = nullptr;
    pass_pool.emplace(Pass::Params{div_size, pass_mode::shoot_through}, Search::without_critters());
    pool.emplace(Cover::Params{div_size}, *pass_pool);
    if (has_result)
    {
        has_result = false;
        extract(a, result.from);
    }
}

void cover_test::draw_ui(app& a, float)
{
    constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
    constexpr auto col1 = ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoSort;
    constexpr auto col0 = col1 | ImGuiTableColumnFlags_WidthFixed;

    char buf[128];

    if (!has_result)
        return;

    auto from_c = Vector3i(result.from.chunk3());
    auto from_l = Vector2i(result.from.local());
    auto from_p = Vector2i(result.from.offset());

    constexpr auto print_coord = [](auto&& buf, Vector3i c, Vector2i l, Vector2i p)
    {
        snformat(buf, "(ch {}x{}) <{}x{}> {{{}x{} px}}", c.x(), c.y(), l.x(), l.y(), p.x(), p.y());
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
        ImGui::TableSetupColumn("##name", col0);
        ImGui::TableSetupColumn("##value", col1 | ImGuiTableColumnFlags_WidthStretch);

        do_column("from");
        print_coord(buf, from_c, from_l, from_p);
        text(buf);

        do_column("div_size");
        {
            const auto ds = pool->params().div_size;
            snformat(buf, "{}x{}", ds, ds);
            ImGui::SetNextItemWidth(100);
            if (auto b2 = begin_combo("##div_size", buf))
                for (auto n : div_sizes)
                {
                    char label[16];
                    snformat(label, "{}x{}", n, n);
                    if (ImGui::Selectable(label, n == ds))
                        set_div_size(a, n);
                }
        }

        do_column("chunks");
        {
            snformat(buf, "{}x{}", chunk_span, chunk_span);
            ImGui::SetNextItemWidth(100);
            if (auto b2 = begin_combo("##chunk_span", buf))
                for (auto n : chunk_spans)
                {
                    char label[16];
                    snformat(label, "{}x{}", n, n);
                    if (ImGui::Selectable(label, n == chunk_span))
                        chunk_span = n;
                }
        }

        do_column("octant");

        constexpr auto fmt_octant_label = [](auto&& out, u32 kk) {
            const auto dir = Cover::octant_direction(kk);
            const float deg = float(Deg(Rad(std::atan2(dir.y(), dir.x()))));
            const float dd = deg < 0 ? deg + 360 : deg;
            snformat(out, "{} (k={}, {:.1f}°)", compass_names_16[kk / 2], kk, dd);
        };

        fmt_octant_label(buf, (u32)selected_octant);
        text(buf);

        static const float label_max_w = [&] {
            char tmp[64];
            float w = 0;
            for (u32 kk = 0; kk < Cover::octant_count; kk++)
            {
                fmt_octant_label(tmp, kk);
                w = Math::max(w, ImGui::CalcTextSize(tmp).x);
            }
            return w;
        }();
        ImGui::SameLine(label_max_w + ImGui::GetStyle().ItemSpacing.x);

        {
            auto b2 = push_style_var(ImGuiStyleVar_FramePadding, {ImGui::GetStyle().FramePadding.x, 0});

            if (ImGui::ArrowButton("##oct_dec", ImGuiDir_Left))
                selected_octant = (i32)(((u32)selected_octant + Cover::octant_count - 1) % Cover::octant_count);
            ImGui::SameLine();

            int sel = selected_octant;
            ImGui::SetNextItemWidth(180);
            {
                [[maybe_unused]] const raii_wrapper slider_colors[] = {
                    push_style_color(ImGuiCol_SliderGrab,       Color4{1.0f,  0.55f, 0.0f,  1.0f}),
                    push_style_color(ImGuiCol_SliderGrabActive, Color4{1.0f,  0.85f, 0.2f,  1.0f}),
                    push_style_color(ImGuiCol_FrameBg,          Color4{0.10f, 0.12f, 0.18f, 1.0f}),
                    push_style_color(ImGuiCol_FrameBgHovered,   Color4{0.16f, 0.20f, 0.30f, 1.0f}),
                };
                ImGui::SliderInt("##sel_octant", &sel, 0, (int)Cover::octant_count - 1);
            }
            selected_octant = sel;

            ImGui::SameLine();
            if (ImGui::ArrowButton("##oct_inc", ImGuiDir_Right))
                selected_octant = (i32)(((u32)selected_octant + 1u) % Cover::octant_count);
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
                        const bool is_selected = (u32)selected_octant == cell.k;
                        [[maybe_unused]] const raii_wrapper colors[] = {
                            is_selected ? push_style_color(ImGuiCol_Button, color_selected) : raii_wrapper{},
                            is_selected ? push_style_color(ImGuiCol_ButtonHovered, color_selected) : raii_wrapper{},
                        };
                        if (cell.large)
                        {
                            if (ImGui::Button(cell.name, big_sz))
                                selected_octant = (i32)cell.k;
                        }
                        else
                        {
                            if (ImGui::SmallButton(cell.name))
                                selected_octant = (i32)cell.k;
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
