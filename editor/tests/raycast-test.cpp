#include "../tests-private.hpp"
#include "editor/app.hpp"
#include "floormat/main.hpp"
#include "compat/shared-ptr-wrapper.hpp"
#include "../imgui-raii.hpp"
#include "src/critter.hpp"
#include "src/world.hpp"
#include "src/raycast-diag.hpp"
#include <cinttypes>
#include <array>
#include <vector>
#include <mg/Color.h>
#include <mg/Timeline.h>

namespace floormat::tests {

namespace {

using namespace floormat::imgui;
using namespace floormat::rc;

struct pending_s
{
    point from, to;
    object_id self;
    bool exists : 1 = false;
};

void print_coord(auto&& buf, Vector3i c, Vector2i l, Vector2i p)
{
    std::snprintf(buf, std::size(buf), "(ch %dx%d) <%dx%d> {%dx%d px}", c.x(), c.y(), l.x(), l.y(), p.x(), p.y());
}

void print_coord_(auto&& buf, point pt)
{
    auto C_c = Vector3i(pt.chunk3());
    auto C_l = Vector2i(pt.local());
    auto C_p = Vector2i(pt.offset());
    print_coord(buf, C_c, C_l, C_p);
}

void print_vec2(auto&& buf, Vector2 vec)
{
    std::snprintf(buf, std::size(buf), "(%.2f x %.2f)", (double)vec.x(), (double)vec.y());
}

void do_column(StringView name)
{
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  text(name);
  ImGui::TableNextColumn();
}

struct raycast_test final : base_test
{
    raycast_result_s result;
    pending_s pending;
    raycast_diag_s diag;
    float time = 0;

    ~raycast_test() noexcept override;

    bool handle_key(app&, const key_event&, bool) override
    {
        return false;
    }

    bool handle_mouse_click(app& a, const mouse_button_event& e, bool is_down) override
    {
        if (e.button == mouse_button_left && is_down)
        {
            time = 0;
            auto& M = a.main();
            auto& w = M.world();
            if (auto pt_ = a.cursor_state().point())
            {
                auto C = a.ensure_player_character(w).ptr;
                auto pt0 = C->position();
                pending = { .from = pt0, .to = *pt_, .self = C->id, .exists = true, };
                return true;
            }
        }
        else if (e.button == mouse_button_right && is_down)
        {
            result.has_result = false;
            pending.exists = false;
        }
        return false;
    }

    bool handle_mouse_move(app& a, const mouse_move_event& e) override
    {
        if (e.buttons & mouse_button_left)
            return handle_mouse_click(a, {e.position, e.mods, mouse_button_left, 1}, true);
        return true;
    }

    void draw_overlay(app& a) override
    {
        if (!result.has_result)
            return;

        const auto color = ImGui::ColorConvertFloat4ToU32({1, 0, 0, 1}),
                   color2 = ImGui::ColorConvertFloat4ToU32({1, 0, 0.75, 1}),
                   color3 = ImGui::ColorConvertFloat4ToU32({0, 0, 1, 1});
        ImDrawList& draw = *ImGui::GetForegroundDrawList();

        {
            auto p0 = a.point_screen_pos(result.from),
                 p1 = a.point_screen_pos(result.success
                                         ? object::normalize_coords(result.from, Vector2i(diag.V))
                                         : result.collision);
            draw.AddLine({p0.x(), p0.y()}, {p1.x(), p1.y()}, color2, 2);
        }

        for (auto [center, size] : diag.path)
        {
            //auto c = a.point_screen_pos(center);
            //draw.AddCircleFilled({c.x(), c.y()}, 3, color);
            const auto hx = (int)(size.x()/2), hy = (int)(size.y()/2);
            auto p00 = a.point_screen_pos(object::normalize_coords(center, {-hx, -hy})),
                 p10 = a.point_screen_pos(object::normalize_coords(center, {hx, -hy})),
                 p01 = a.point_screen_pos(object::normalize_coords(center, {-hx, hy})),
                 p11 = a.point_screen_pos(object::normalize_coords(center, {hx, hy}));
            draw.AddLine({p00.x(), p00.y()}, {p01.x(), p01.y()}, color, 2);
            draw.AddLine({p00.x(), p00.y()}, {p10.x(), p10.y()}, color, 2);
            draw.AddLine({p01.x(), p01.y()}, {p11.x(), p11.y()}, color, 2);
            draw.AddLine({p10.x(), p10.y()}, {p11.x(), p11.y()}, color, 2);
        }

        if (!result.success)
        {
            auto p = a.point_screen_pos(result.collision);
            draw.AddCircleFilled({p.x(), p.y()}, 10, color3);
            draw.AddCircleFilled({p.x(), p.y()}, 7, color);
        }
        else
        {
            auto color4 = ImGui::ColorConvertFloat4ToU32({0, 1, 0, 1});
            auto p = a.point_screen_pos(result.to);
            draw.AddCircleFilled({p.x(), p.y()}, 10, color3);
            draw.AddCircleFilled({p.x(), p.y()}, 7, color4);
        }
    }

    void draw_ui(app&, float) override
    {
        constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
        constexpr auto colflags_1 = ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder |
                                    ImGuiTableColumnFlags_NoSort;
        constexpr auto colflags_0 = colflags_1 | ImGuiTableColumnFlags_WidthFixed;

        if (!result.has_result)
            return;

        if (auto b1 = begin_table("##raycast-results", 2, table_flags))
        {
            ImGui::TableSetupColumn("##name", colflags_0);
            ImGui::TableSetupColumn("##value", colflags_1 | ImGuiTableColumnFlags_WidthStretch);

            char buf[128];

            do_column("from");
            print_coord_(buf, result.from);
            text(buf);

            do_column("to");
            print_coord_(buf, result.to);
            text(buf);

            ImGui::NewLine();

            if (result.success)
            {
                do_column("collision");
                text("-");
                do_column("collider");
                text("-");

            }
            else
            {
                const char* type;

                switch ((collision_type)result.collider.tag)
                {
                using enum collision_type;
                default: type = "unknown?!"; break;
                case none: type = "none?!"; break;
                case object: type = "object"; break;
                case scenery: type = "scenery"; break;
                case geometry: type = "geometry"; break;
                }

                do_column("collision");
                print_coord_(buf, result.collision);
                { auto b = push_style_color(ImGuiCol_Text, 0xffff00ff_rgbaf);
                  text(buf);
                }

                do_column("collider");
                std::snprintf(buf, std::size(buf), "%s @ %" PRIu64,
                              type, uint64_t{result.collider.data});
                { auto b = push_style_color(ImGuiCol_Text, 0xffff00ff_rgbaf);
                  text(buf);
                }
            }

            ImGui::NewLine();

            do_column("dir");
            std::snprintf(buf, std::size(buf), "%.4f x %.4f", (double)diag.dir.x(), (double)diag.dir.y());
            text(buf);

            if (!result.success)
            {
                do_column("tmin");
                std::snprintf(buf, std::size(buf), "%f / %f",
                              (double)diag.tmin,
                              (double)(diag.tmin / diag.V.length()));
                text(buf);
            }
            else
            {
                do_column("tmin");
                std::snprintf(buf, std::size(buf), "%f / %f",
                              (double)diag.V.length(), 1.0);
                text(buf);
            }

            do_column("vector");
            print_vec2(buf, diag.V);
            text(buf);

            do_column("||dir^-1||");
            std::snprintf(buf, std::size(buf), "%f x %f",
                          (double)diag.dir_inv_norm.x(),
                          (double)diag.dir_inv_norm.y());
            text(buf);

            ImGui::NewLine();

            do_column("bbox-size");
            std::snprintf(buf, std::size(buf), "(%u x %u)", diag.size.x(), diag.size.y());
            text(buf);

            do_column("path-len");
            std::snprintf(buf, std::size(buf), "%zu", diag.path.size());
            text(buf);

            do_column("time");
            std::snprintf(buf, std::size(buf), "%.3f ms", (double)(1000 * time));
            text(buf);
        }
    }

    void update_pre(app&) override
    {

    }

    void update_post(app& a) override
    {
        if (pending.exists)
        {
            pending.exists = false;
            if (pending.from.chunk3().z != pending.to.chunk3().z)
            {
                fm_warn("raycast: wrong Z value");
                return;
            }
            Timeline timeline;
            timeline.start();
            result = raycast_with_diag(diag, a.main().world(), pending.from, pending.to, pending.self);
            time = timeline.currentFrameDuration();
        }
    }
};

raycast_test::~raycast_test() noexcept = default;

} // namespace

Pointer<base_test> tests_data::make_test_raycast() { return Pointer<raycast_test>{InPlaceInit}; }

} // namespace floormat::tests
