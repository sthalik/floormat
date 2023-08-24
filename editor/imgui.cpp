#include "app.hpp"
#include "floormat/main.hpp"
#include "compat/format.hpp"
#include "src/world.hpp"
#include "src/anim-atlas.hpp"
#include "shaders/shader.hpp"
#include "shaders/lightmap.hpp"
#include "main/clickable.hpp"
#include "imgui-raii.hpp"
#include "src/light.hpp"
#include <Magnum/GL/Renderer.h>

namespace floormat {

using namespace floormat::imgui;

bool popup_target::operator==(const popup_target&) const = default;

void app::init_imgui(Vector2i size)
{
    if (!_imgui.context())
        _imgui = ImGuiIntegration::Context(Vector2{size}, size, size);
    else
        _imgui.relayout(Vector2{size}, size, size);
}

void app::render_menu()
{
    _imgui.drawFrame();
}

float app::draw_main_menu()
{
    float main_menu_height = 0;
    if (auto b = begin_main_menu())
    {
        ImGui::SetWindowFontScale(M->dpi_scale().min());
        if (auto b = begin_menu("File"))
        {
            bool do_new = false, do_quickload = false, do_quit = false;
            ImGui::MenuItem("New", nullptr, &do_new);
            ImGui::Separator();
            ImGui::MenuItem("Load quicksave", nullptr, &do_quickload);
            ImGui::Separator();
            ImGui::MenuItem("Quit", "Ctrl+Q", &do_quit);
            if (do_new)
                do_key(key_new_file, kmod_none);
            else if (do_quickload)
                do_key(key_quickload, kmod_none);
            else if (do_quit)
                do_key(key_quit, kmod_none);
        }
        if (auto b = begin_menu("Editor"))
        {
            auto mode = _editor.mode();
            using m = editor_mode;
            const auto* ed_sc = _editor.current_scenery_editor();
            const auto* ed_w = _editor.current_tile_editor();
            const bool b_rotate = ed_sc && ed_sc->is_anything_selected() ||
                                  mode == editor_mode::walls && ed_w;

            bool m_none    = mode == m::none, m_floor = mode == m::floor, m_walls = mode == m::walls,
                 m_scenery = mode == m::scenery, m_vobjs = mode == m::vobj,
                 b_collisions = _render_bboxes, b_clickables = _render_clickables,
                 b_vobjs = _render_vobjs, b_all_z_levels = _render_all_z_levels;

            ImGui::SeparatorText("Mode");
            if (ImGui::MenuItem("Select",  "1", m_none))
                do_key(key_mode_none);
            if (ImGui::MenuItem("Floor",   "2", m_floor))
                do_key(key_mode_floor);
            if (ImGui::MenuItem("Walls",   "3", m_walls))
                do_key(key_mode_walls);
            if (ImGui::MenuItem("Scenery", "4", m_scenery))
                do_key(key_mode_scenery);
            if (ImGui::MenuItem("Virtual objects", "5", m_vobjs))
                do_key(key_mode_vobj);
            ImGui::SeparatorText("Modify");
            if (ImGui::MenuItem("Rotate", "R", false, b_rotate))
                do_key(key_rotate_tile);
            ImGui::SeparatorText("View");
            if (ImGui::MenuItem("Show collisions", "Alt+C", b_collisions))
                do_key(key_render_collision_boxes);
            if (ImGui::MenuItem("Show clickables", "Alt+L", b_clickables))
                do_key(key_render_clickables);
            if (ImGui::MenuItem("Render virtual objects", "Alt+V", b_vobjs))
                do_key(key_render_vobjs);
            if (ImGui::MenuItem("Show all Z levels", "T", b_all_z_levels))
                do_key(key_render_all_z_levels);
        }

        main_menu_height = ImGui::GetContentRegionMax().y;
    }
    return main_menu_height;
}

void app::draw_ui()
{
    const auto dpi = M->dpi_scale().min();
    [[maybe_unused]] const auto style_ = style_saver{};
    auto& style = ImGui::GetStyle();
    auto& ctx = *ImGui::GetCurrentContext();

    ImGui::StyleColorsDark(&style);
    style.ScaleAllSizes(dpi);

    ImGui::GetIO().IniFilename = nullptr;
    _imgui.newFrame();

    if (_render_clickables)
        draw_clickables();
    if (_render_vobjs)
        draw_light_info();
    const float main_menu_height = draw_main_menu();

    [[maybe_unused]] auto font = font_saver{ctx.FontSize*dpi};

    draw_lightmap_test();

    if (_editor.current_tile_editor() || _editor.current_scenery_editor() || _editor.current_vobj_editor())
        draw_editor_pane(main_menu_height);
    draw_fps();

    draw_tile_under_cursor();
    if (_editor.mode() == editor_mode::none)
        draw_inspector();
    draw_z_level();
    do_popup_menu();
    ImGui::EndFrame();
}

void app::draw_clickables()
{
    ImDrawList& draw = *ImGui::GetForegroundDrawList();
    const auto color = ImGui::ColorConvertFloat4ToU32({0, .8f, .8f, .95f});
    constexpr float thickness = 2.5f;

    for (const auto& x : M->clickable_scenery())
    {
        auto dest = Math::Range2D<float>(x.dest);
        auto min = dest.min(), max = dest.max();
        draw.AddRect({ min.x(), min.y() }, { max.x(), max.y() },
                     color, 0, ImDrawFlags_None, thickness);
        if (x.slope != 0.f)
        {
            const auto bb_min = min + Vector2(x.bb_min), bb_max = min + Vector2(x.bb_max);
            draw.AddLine({ bb_min[0], bb_min[1] }, { bb_max[0], bb_max[1] }, color, thickness);
        }
    }
}

void app::draw_light_info()
{
    ImDrawList& draw = *ImGui::GetForegroundDrawList();
    const auto dpi = M->dpi_scale();
    constexpr float font_size = 12;
    const auto& style = ImGui::GetStyle();
    const ImVec2 pad { style.FramePadding.x*.5f, 0 };
    const auto font_size_ = dpi.sum()*.5f * font_size;

    draw_list_font_saver saver2{font_size_};
    imgui::font_saver saver{font_size_};

    for (const auto& x : M->clickable_scenery())
    {
        if (x.e->type() == entity_type::light)
        {
            const auto dest = Math::Range2D<float>(x.dest);
            const auto& e = static_cast<const light&>(*x.e);

            if (e.id == _popup_target.id) // TODO use z order instead
                continue;

            StringView falloff;
            switch (e.falloff)
            {
            default: falloff = "?"_s; break;
            case light_falloff::constant: falloff = "constant"_s; break;
            case light_falloff::linear: falloff = "linear"_s; break;
            case light_falloff::quadratic: falloff = "quadratic"_s; break;
            }

            // todo add rendering color as part of the lightbulb icon
#if 0
            char color[8];
            snformat(color, "{:2X}{:2X}{:2X}"_cf, e.color.x(), e.color.y(), e.color.z());
#endif
            char buf[128];

            if (e.falloff == light_falloff::constant)
                snformat(buf, "{}"_cf, falloff);
            else
                snformat(buf, "{} range={}"_cf, falloff, e.max_distance);
            auto text_size = ImGui::CalcTextSize(buf);

            float offy = dest.max().y() + 5 * dpi.y();
            float offx = dest.min().x() + (dest.max().x() - dest.min().x())*.5f - text_size.x*.5f;

            draw.AddRectFilled({offx-pad.x, offy-pad.y}, {offx + text_size.x + pad.x, offy + text_size.y + pad.y}, ImGui::ColorConvertFloat4ToU32({0, 0, 0, 1}));
            draw.AddText({offx, offy}, ImGui::ColorConvertFloat4ToU32({1, 1, 0, 1}), buf);
        }
    }
}

void app::do_lightmap_test()
{
    if (!_tested_light)
        return;

    //GL::Renderer::setScissor({{}, M->window_size()}); // FIXME
    GL::Renderer::disable(GL::Renderer::Feature::ScissorTest);

    auto& w = M->world();
    auto e_ = w.find_entity(_tested_light);

    if (e_)
    {
        auto& e = *e_;
        fm_assert(e_->type() == entity_type::light);
        const auto& li = static_cast<const light&>(e);
        light_s L {
            .center = Vector2(li.coord.local()) * TILE_SIZE2 + Vector2(li.offset),
            .dist = li.max_distance,
            .color = li.color,
            .falloff = li.falloff,
        };
        auto& shader = M->lightmap_shader();
        auto ch = Vector2(e.coord.chunk());
        shader.begin_occlusion();
        shader.add_chunk(ch, e.chunk()); // todo add neighbors
        shader.end_occlusion();
        shader.bind();
        shader.add_light(ch, L);
        M->bind();
    }
}

void app::draw_lightmap_test()
{
    if (!_tested_light)
        return;

    auto& shader = M->lightmap_shader();
    bool is_open = true;
    constexpr auto preview_size = ImVec2{512, 512};

    ImGui::SetNextWindowSize(preview_size);

    auto b1 = push_style_var(ImGuiStyleVar_WindowPadding, {0, 0});
    constexpr auto flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollbar;

    //constexpr auto img_size = 1 / Vector2(lightmap_shader::max_chunks);
    if (ImGui::Begin("Lightmap", &is_open, flags))
    {
        ImGui::Image(&shader.accum_texture(), preview_size, {0, 0}, {1, 1});
        ImGui::End();
    }
    if (!is_open)
        _tested_light = 0;
}

static constexpr auto SCENERY_POPUP_NAME = "##scenery-popup"_s;

bool app::check_inspector_exists(const popup_target& p)
{
    if (p.target == popup_target_type::none) [[unlikely]]
        return true;
    for (const auto& p2 : inspectors)
        if (p2 == p)
            return true;
    return false;
}

void app::do_popup_menu()
{
    const auto [id, target] = _popup_target;
    auto& w = M->world();
    auto e_ = w.find_entity(id);

    if (target == popup_target_type::none || !e_)
    {
        _popup_target = {};
        _pending_popup = {};
        return;
    }

    auto& e = *e_;

    auto b0 = push_id(SCENERY_POPUP_NAME);
    //if (_popup_target.target != popup_target_type::scenery) {...}

    if (_pending_popup)
    {
        _pending_popup = false;
        //if (type != popup_target_type::scenery) {...}
        ImGui::OpenPopup(SCENERY_POPUP_NAME.data());
    }

    if (auto b1 = begin_popup(SCENERY_POPUP_NAME))
    {
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        ImGui::SeparatorText("Setup");
        const auto i = e.index();
        if (ImGui::MenuItem("Activate", nullptr, false, e.can_activate(i)))
            e.activate(i);
        if (bool b_ins = !check_inspector_exists(_popup_target);
            ImGui::MenuItem("Inspect", nullptr, !b_ins, b_ins))
            inspectors.push_back(std::exchange(_popup_target, {}));
        if (bool b_testing = e.id == _tested_light;
            e.type() == entity_type::light)
            if (ImGui::MenuItem("Test", nullptr, b_testing))
                _tested_light = e.id;
        ImGui::SeparatorText("Modify");
        if (auto next_rot = e.atlas->next_rotation_from(e.r);
            ImGui::MenuItem("Rotate", nullptr, false, next_rot != e.r && e.can_rotate(next_rot)))
            e.rotate(i, next_rot);
        if (ImGui::MenuItem("Delete", nullptr, false))
            e.chunk().remove_entity(e.index());
    }
    else
        _popup_target = {};
}

void app::kill_popups(bool hard)
{
    const bool imgui = _imgui.context() != nullptr;

    _pending_popup = false;
    _popup_target = {};

    if (hard)
        _tested_light = 0;

    if (imgui)
        ImGui::CloseCurrentPopup();

    if (hard)
        inspectors.clear();

    if (imgui)
        ImGui::FocusWindow(nullptr);
}

} // namespace floormat
