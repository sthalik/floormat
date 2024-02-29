#include "../tests-private.hpp"
#include "src/tile-constants.hpp"
#include "src/chunk-region.hpp"
#include "src/object.hpp"
#include "src/world.hpp"
#include "../app.hpp"
#include "../imgui-raii.hpp"
#include "floormat/main.hpp"
#include <mg/Vector2.h>

namespace floormat::tests {

namespace {

using namespace floormat::imgui;
using Search::div_count;
using Search::div_size;

constexpr auto div_min = -iTILE_SIZE2/2 + div_size/2;

struct pending_s
{
    chunk_coords_ c;
    bool exists : 1 = false;
};

struct result_s
{
    chunk::pass_region region;
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

struct region_test final : base_test
{
    result_s result;
    pending_s pending;

    void do_region_extraction(world& w, chunk_coords_ coord);
    ~region_test() noexcept override = default;

    bool handle_key(app&, const key_event&, bool) override { return {}; }
    bool handle_mouse_click(app& a, const mouse_button_event& e, bool is_down) override;
    bool handle_mouse_move(app&, const mouse_move_event&) override { return {}; }
    void draw_overlay(app& a) override;
    void draw_ui(app&, float) override;
    void update_pre(app&) override {}
    void update_post(app& a) override;
};

void region_test::draw_overlay(app& a)
{
    if (result.exists)
    {
        constexpr float dot_radius = 4;
        const auto dot_color = ImGui::ColorConvertFloat4ToU32({1, 0, 1, 1});
        ImDrawList& draw = *ImGui::GetForegroundDrawList();
        auto start = point{result.c, {0, 0}, {0, 0}};

        for (int j = 0; j < div_count.y(); j++)
            for (int i = 0; i < div_count.x(); i++)
            {
                auto index = (uint32_t)j * div_count.x() + (uint32_t)i;
                if (result.region.bits[index])
                    continue;
                auto pos = div_min + div_size * Vector2i{i, j};
                auto pt = object::normalize_coords(start, pos);
                auto px = a.point_screen_pos(pt);
                draw.AddCircleFilled({px.x(), px.y()}, dot_radius, dot_color);
            }
    }
}

void region_test::draw_ui(app&, float)
{
    if (!result.exists)
        return;

    char buf[128];
    constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
    constexpr auto colflags_1 = ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder |
                                ImGuiTableColumnFlags_NoSort;
    constexpr auto colflags_0 = colflags_1 | ImGuiTableColumnFlags_WidthFixed;

    if (auto b1 = begin_table("##region-results", 2, table_flags))
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
        std::snprintf(buf, sizeof buf, "%zu", result.region.bits.count());
        //{ auto b = push_style_color(ImGuiCol_Text, 0x00ff00ff_rgbaf); text(buf); }
        text(buf);

        do_column("blocked");
        std::snprintf(buf, sizeof buf, "%zu", result.region.bits.size() - result.region.bits.count());
        //{ auto b = push_style_color(ImGuiCol_Text, 0xffff00ff_rgbaf); text(buf); }
        text(buf);

        do_column("time");
        std::snprintf(buf, sizeof buf, "%.1f ms", (double)(1000 * result.region.time));
        text(buf);
    }
}

bool region_test::handle_mouse_click(app& a, const mouse_button_event& e, bool is_down)
{
    if (e.button == mouse_button_left && is_down)
    {
        if (auto pt_ = a.cursor_state().point())
        {
            pending = {
                .c = pt_->chunk3(),
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

void region_test::update_post(app& a)
{
    if (pending.exists)
    {
        pending.exists = false;
        auto& M = a.main();
        auto& w = M.world();
        do_region_extraction(w, pending.c);
    }
}

void region_test::do_region_extraction(world& w, chunk_coords_ coord)
{
    if (auto* c = w.at(coord))
        result = {
            .region = c->make_pass_region(true),
            .c = coord,
            .exists = true,
        };
}

} // namespace

Pointer<base_test> tests_data::make_test_region() { return Pointer<region_test>{InPlaceInit}; }

} // namespace floormat::tests
