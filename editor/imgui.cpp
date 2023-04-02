#include "app.hpp"
#include "floormat/main.hpp"
#include "compat/format.hpp"
#include "src/world.hpp"
#include "src/anim-atlas.hpp"
#include "shaders/tile.hpp"
#include "main/clickable.hpp"
#include "imgui-raii.hpp"

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
            bool b_none = mode == m::none, b_floor = mode == m::floor, b_walls = mode == m::walls,
                 b_scenery = mode == m::scenery, b_collisions = _render_bboxes,
                 b_clickables = _render_clickables;
            const bool b_rotate = ed_sc && ed_sc->is_anything_selected() ||
                                  mode == editor_mode::walls && ed_w && ed_w->is_anything_selected();
            ImGui::SeparatorText("Mode");
            if (ImGui::MenuItem("Select",  "1", b_none))
                do_key(key_mode_none);
            if (ImGui::MenuItem("Floor",   "2", b_floor))
                do_key(key_mode_floor);
            if (ImGui::MenuItem("Walls",   "3", b_walls))
                do_key(key_mode_walls);
            if (ImGui::MenuItem("Scenery", "4", b_scenery))
                do_key(key_mode_scenery);
            ImGui::SeparatorText("Modify");
            if (ImGui::MenuItem("Rotate", "R", false, b_rotate))
                do_key(key_rotate_tile);
            ImGui::SeparatorText("View");
            if (ImGui::MenuItem("Show collisions", "Alt+C", b_collisions))
                do_key(key_render_collision_boxes);
            if (ImGui::MenuItem("Show clickables", "Alt+L", b_clickables))
                do_key(key_render_clickables);
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

    const float main_menu_height = draw_main_menu();
    [[maybe_unused]] auto font = font_saver{ctx.FontSize*dpi};
    if (_editor.current_tile_editor() || _editor.current_scenery_editor())
        draw_editor_pane(main_menu_height);
    draw_fps();
    draw_tile_under_cursor();
    if (_editor.mode() == editor_mode::none)
        draw_inspector();
    do_popup_menu();
    ImGui::EndFrame();
}

void app::draw_clickables()
{
    ImDrawList& draw = *ImGui::GetForegroundDrawList();
    const auto color = ImGui::ColorConvertFloat4ToU32({0, .8f, .8f, .95f});
    constexpr float thickness = 2.5f;
    const auto& shader = M->shader();
    const auto win_size = M->window_size();

    for (const auto& x : M->clickable_scenery())
    {
        auto dest = Math::Range2D<float>(x.dest);
        auto min = dest.min(), max = dest.max();
        draw.AddRect({ min.x(), min.y() }, { max.x(), max.y() },
                     color, 0, ImDrawFlags_None, thickness);
        if (x.slope != 0.f)
        {
            const auto& e = *x.e;
            const auto bb_min_ = -tile_shader::project(Vector3(Vector2(e.bbox_size/2), 0));
            const auto bb_max_ = bb_min_ + tile_shader::project(Vector3(Vector2(e.bbox_size), 0));
            const auto bb_min = min + tile_shader::project(Vector3(bb_min_, 0));
            const auto bb_max = min + tile_shader::project(Vector3(bb_max_, 0));
            draw.AddLine({ bb_min[0], bb_min[1] }, { bb_max[0], bb_max[1] }, color, thickness);
        }
    }
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
    const auto [sc, target] = _popup_target;
    if (target == popup_target_type::none || sc == nullptr)
    {
        _popup_target = {};
        _pending_popup = {};
        return;
    }

    auto b0 = push_id(SCENERY_POPUP_NAME);
    //if (_popup_target.target != popup_target_type::scenery) {...}

    if (_pending_popup)
    {
        _pending_popup = false;
        fm_assert(target != popup_target_type::none && sc != nullptr);
        //if (type != popup_target_type::scenery) {...}
        ImGui::OpenPopup(SCENERY_POPUP_NAME.data());
    }

    if (auto b1 = begin_popup(SCENERY_POPUP_NAME))
    {
        ImGui::SeparatorText("Setup");
        const auto i = sc->index();
        if (ImGui::MenuItem("Activate", nullptr, false, sc->can_activate(i)))
            sc->activate(i);
        if (bool b_ins = sc && !check_inspector_exists(_popup_target);
            ImGui::MenuItem("Inspect", nullptr, false, b_ins))
            inspectors.push_back(std::exchange(_popup_target, {}));
        ImGui::SeparatorText("Modify");
        if (auto next_rot = sc->atlas->next_rotation_from(sc->r);
            ImGui::MenuItem("Rotate", nullptr, false, next_rot != sc->r && sc->can_rotate(next_rot)))
            sc->rotate(i, next_rot);
        if (ImGui::MenuItem("Delete", nullptr, false))
            sc->chunk().remove_entity(sc->index());
    }
}

void app::kill_popups(bool hard)
{
    const bool imgui = _imgui.context() != nullptr;

    _pending_popup = false;
    _popup_target = {};

    if (imgui)
        ImGui::CloseCurrentPopup();

    if (hard)
        inspectors.clear();

    if (imgui)
        ImGui::FocusWindow(nullptr);
}

} // namespace floormat
