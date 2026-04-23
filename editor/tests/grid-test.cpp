#include "../tests-private.hpp"
#include "compat/array-size.hpp"
#include "src/tile-constants.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/grid-pass.hpp"
#include "src/point.hpp"
#include "../app.hpp"
#include "../imgui-raii.hpp"
#include "floormat/main.hpp"
#include <cstdio>
#include <cr/BitArray.h>

namespace floormat::tests {

namespace {

using namespace floormat::imgui;

struct pending_s
{
    chunk_coords_ c;
    bool exists : 1 = false;
};

struct result_s
{
    BitArray bits;
    uint32_t div_count = 0;
    uint32_t div_size = 0;
    chunk_coords_ c;
    bool exists : 1 = false;
};

void do_column(StringView name)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    text(name);
    ImGui::TableNextColumn();
}

struct grid_test final : base_test
{
    Pass::Pool pool;
    result_s result;
    pending_s pending;

    grid_test();
    ~grid_test() noexcept override = default;

    void extract(app& a, chunk_coords_ coord);

    bool handle_key(app&, const key_event&, bool) override { return {}; }
    bool handle_mouse_click(app& a, const mouse_button_event& e, bool is_down) override;
    bool handle_mouse_move(app&, const mouse_move_event&) override { return {}; }
    void draw_overlay(app& a) override;
    void draw_ui(app&, float) override;
    void update_pre(app&, const Ns&) override {}
    void update_post(app& a, const Ns&) override;
};

grid_test::grid_test(): pool{Pass::Params{8u, 8u}} {}

void grid_test::draw_overlay(app& a)
{
    if (!result.exists)
        return;

    constexpr float dot_radius = 3;
    const auto dot_color = ImGui::ColorConvertFloat4ToU32({1, 0, 1, 1});
    ImDrawList& draw = *ImGui::GetForegroundDrawList();
    auto start = point{result.c, {0, 0}, {0, 0}};

    const auto dc = (int)result.div_count;
    const auto ds = (int)result.div_size;
    for (int j = 0; j < dc; j++)
        for (int i = 0; i < dc; i++)
        {
            auto index = (uint32_t)j * (uint32_t)dc + (uint32_t)i;
            if (result.bits[index])
                continue;
            auto pos = -iTILE_SIZE2/2 + ds * Vector2i{i, j} + Vector2i{ds/2};
            auto pt = point::normalize_coords(start, pos);
            auto px = a.point_screen_pos(pt);
            draw.AddCircleFilled({px.x(), px.y()}, dot_radius, dot_color);
        }
}

void grid_test::draw_ui(app&, float)
{
    if (!result.exists)
        return;

    char buf[128];
    constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
    constexpr auto colflags_1 = ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder |
                                ImGuiTableColumnFlags_NoSort;
    constexpr auto colflags_0 = colflags_1 | ImGuiTableColumnFlags_WidthFixed;

    if (auto b1 = begin_table("##grid-results", 2, table_flags))
    {
        ImGui::TableSetupColumn("##name", colflags_0);
        ImGui::TableSetupColumn("##value", colflags_1 | ImGuiTableColumnFlags_WidthStretch);

        do_column("chunk");
        if (result.c.z != 0)
            std::snprintf(buf, sizeof buf, "%d x %d x %d", (int)result.c.x, (int)result.c.y, (int)result.c.z);
        else
            std::snprintf(buf, sizeof buf, "%d x %d", (int)result.c.x, (int)result.c.y);
        text(buf);

        do_column("passable");
        std::snprintf(buf, sizeof buf, "%zu", result.bits.count());
        text(buf);

        do_column("blocked");
        std::snprintf(buf, sizeof buf, "%zu", result.bits.size() - result.bits.count());
        text(buf);

        do_column("div_size");
        std::snprintf(buf, sizeof buf, "%u", result.div_size);
        text(buf);
    }
}

bool grid_test::handle_mouse_click(app& a, const mouse_button_event& e, bool is_down)
{
    if (e.button == mouse_button_left && is_down)
    {
        if (auto ptʹ = a.cursor_state().point())
        {
            pending = {
                .c = ptʹ->chunk3(),
                .exists = true,
            };
            return true;
        }
    }
    else if (e.button == mouse_button_right && is_down)
    {
        pending.exists = false;
        result.exists = false;
    }
    return false;
}

void grid_test::update_post(app& a, const Ns&)
{
    if (pending.exists)
    {
        pending.exists = false;
        extract(a, pending.c);
    }
}

void grid_test::extract(app& a, chunk_coords_ coord)
{
    auto& M = a.main();
    auto& w = M.world();
    auto* c = w.at(coord);
    if (!c)
        return;

    pool.maybe_mark_stale_all(w.frame_no());
    pool.build_if_stale_all();
    Pass::Grid g = pool[*c];
    g.build_if_stale();

    const auto dc = g.div_count();
    BitArray bits{ValueInit, dc * dc};
    for (auto k = 0u; k < dc * dc; k++)
        bits.set(k, g.bit(k));

    result = {
        .bits = Utility::move(bits),
        .div_count = dc,
        .div_size = pool.params().div_size,
        .c = coord,
        .exists = true,
    };
}

} // namespace

Pointer<base_test> tests_data::make_test_grid() { return Pointer<grid_test>{InPlaceInit}; }

} // namespace floormat::tests
