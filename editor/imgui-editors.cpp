#include "app.hpp"
#include "src/tile-constants.hpp"
#include "compat/format.hpp"
#include "imgui-raii.hpp"
#include "ground-editor.hpp"
#include "wall-editor.hpp"
#include "scenery-editor.hpp"
#include "vobj-editor.hpp"
#include "src/anim-atlas.hpp"
#include "src/ground-atlas.hpp"
#include "src/wall-atlas.hpp"
#include "editor.hpp"
#include "loader/loader.hpp"
#include "floormat/main.hpp"
#include <Magnum/Math/Color.h>

namespace floormat {

using namespace floormat::imgui;

namespace {

template<typename T> constexpr inline bool do_group_column = false;
template<> constexpr inline bool do_group_column<scenery_editor> = true;
template<typename T> constexpr inline bool do_path_column = false;
template<> constexpr inline bool do_path_column<wall_editor> = true;

using scenery_ = scenery_editor::scenery_;
using vobj_ = vobj_editor::vobj_;

StringView scenery_type_to_string(const scenery_& sc)
{
    switch (sc.proto.scenery_type())
    {
    case scenery_type::none:    return "none"_s;
    case scenery_type::generic: return "generic"_s;
    case scenery_type::door:    return "door"_s;
    default:                    return "unknown"_s;
    }
}

StringView scenery_path(const wall_cell* wa) { return wa->atlas->name(); }
StringView scenery_name(StringView, const scenery_& sc) { return sc.name; }
StringView scenery_name(StringView, const vobj_& vobj) { return vobj.descr; }
StringView scenery_name(StringView, const wall_cell* w) { return w->name; }
std::shared_ptr<anim_atlas> get_atlas(const scenery_& sc) { return sc.proto.atlas; }
std::shared_ptr<anim_atlas> get_atlas(const vobj_& vobj) { return vobj.factory->atlas(); }
std::shared_ptr<wall_atlas> get_atlas(const wall_cell* w) { return w->atlas; }
Vector2ui get_size(const auto&, anim_atlas& atlas) { return atlas.frame(atlas.first_rotation(), 0).size; }
Vector2ui get_size(const auto&, wall_atlas& atlas) { auto sz = atlas.image_size(); return { std::max(1u, std::min(sz.y()*3/2, sz.x())), sz.y() }; }
bool is_selected(const scenery_editor& ed, const scenery_& sc) { return ed.is_item_selected(sc); }
bool is_selected(const vobj_editor& vo, const vobj_& sc) { return vo.is_item_selected(sc); }
bool is_selected(const wall_editor& wa, const wall_cell* sc) {  return wa.is_atlas_selected(sc->atlas); }
void select_tile(scenery_editor& ed, const scenery_& sc) { ed.select_tile(sc); }
void select_tile(vobj_editor& vo, const vobj_& sc) { vo.select_tile(sc); }
void select_tile(wall_editor& wa, const wall_cell* sc) { wa.select_atlas(sc->atlas); }
auto get_texcoords(const auto&, anim_atlas& atlas) { return atlas.texcoords_for_frame(atlas.first_rotation(), 0, !atlas.group(atlas.first_rotation()).mirror_from.isEmpty()); }
auto get_texcoords(const wall_cell* w, wall_atlas& atlas) { auto sz = get_size(w, atlas); return Quads::texcoords_at({}, sz, atlas.image_size()); }

void draw_editor_tile_pane_atlas(ground_editor& ed, StringView name, const std::shared_ptr<ground_atlas>& atlas, Vector2 dpi)
{
    const auto b = push_id("tile-pane");

    constexpr Color4 color_perm_selected{1, 1, 1, .7f},
                     color_selected{1, 0.843f, 0, .8f},
                     color_hover{0, .8f, 1, .7f};
    const float window_width = ImGui::GetWindowWidth() - 32 * dpi[0];
    char buf[128];
    const auto& style = ImGui::GetStyle();
    const auto N = atlas->num_tiles();

    const auto click_event = [&] {
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            ed.select_tile_permutation(atlas);
        else if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
            ed.clear_selection();
    };
    const auto do_caption = [&] {
        click_event();
        if (ed.is_atlas_selected(atlas))
        {
            ImGui::SameLine();
            text(" (selected)");
        }
        const auto len = snformat(buf, "{:d}"_cf, N);
        fm_assert(len < std::size(buf));
        ImGui::SameLine(window_width - ImGui::CalcTextSize(buf).x - style.FramePadding.x - 4*dpi[0]);
        text({buf, len});
    };
    if (const auto flags = ImGuiTreeNodeFlags_(ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Framed);
        auto b = tree_node(name.data(), flags))
    {
        do_caption();
        [[maybe_unused]] const raii_wrapper vars[] = {
            push_style_var(ImGuiStyleVar_FramePadding, {2*dpi[0], 2*dpi[1]}),
            push_style_color(ImGuiCol_ButtonHovered, color_hover),
        };
        const bool perm_selected = ed.is_permutation_selected(atlas);
        constexpr size_t per_row = 8;
        for (auto i = 0uz; i < N; i++)
        {
            const bool selected = ed.is_tile_selected(atlas, i);
            if (i > 0 && i % per_row == 0)
                ImGui::NewLine();

            [[maybe_unused]] const raii_wrapper vars[] = {
                selected ? push_style_color(ImGuiCol_Button, color_selected) : raii_wrapper{},
                selected ? push_style_color(ImGuiCol_ButtonHovered, color_selected) : raii_wrapper{},
                perm_selected ? push_style_color(ImGuiCol_Button, color_perm_selected) : raii_wrapper{},
                perm_selected ? push_style_color(ImGuiCol_ButtonHovered, color_perm_selected) : raii_wrapper{},
            };

            snformat(buf, "##item_{}"_cf, i);
            const auto uv = atlas->texcoords_for_id(i);
            constexpr ImVec2 size_2 = { TILE_SIZE[0]*.5f, TILE_SIZE[1]*.5f };
            ImGui::ImageButton(buf, (void*)&atlas->texture(), ImVec2(size_2.x * dpi[0], size_2.y * dpi[1]),
                               { uv[3][0], uv[3][1] }, { uv[0][0], uv[0][1] });
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                ed.select_tile(atlas, i);
            else
                click_event();
            ImGui::SameLine();
        }
        ImGui::NewLine();
    }
    else
        do_caption();
}

template<typename T>
void impl_draw_editor_scenery_pane(T& ed, Vector2 dpi)
{
    const auto b1 = push_id("scenery-pane");

    const auto& style = ImGui::GetStyle();
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
    constexpr int ncolumns = do_group_column<T> ? 4 : do_path_column<T> ? 3 : 2;
    const auto size = ImGui::GetWindowSize();
    auto b2 = imgui::begin_table("scenery-table", ncolumns, flags, size);
    const auto row_height = ImGui::GetCurrentContext()->FontSize + 10*dpi[1];
    constexpr auto thumbnail_width = 50;
    ImGui::TableSetupScrollFreeze(1, 1);
    constexpr auto colflags_ = ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoSort;
    constexpr auto colflags = colflags_ | ImGuiTableColumnFlags_WidthFixed;
    ImGui::TableSetupColumn("##thumbnail", colflags, thumbnail_width);
    ImGui::TableSetupColumn("Name", colflags_ | ImGuiTableColumnFlags_WidthStretch);
    if constexpr(do_group_column<T>)
    {
        const auto colwidth_type = ImGui::CalcTextSize("generic").x;
        const auto colwidth_group = ImGui::CalcTextSize("MMMMMMMMMMMMMMM").x;
        ImGui::TableSetupColumn("Type", colflags, colwidth_type);
        ImGui::TableSetupColumn("Group", colflags, colwidth_group);
    }
    else if constexpr(do_path_column<T>)
    {
        const auto colwidth_path = ImGui::CalcTextSize("/MMMMMMMMMMMMMMM").x;
        ImGui::TableSetupColumn("Path", colflags, colwidth_path);
    }
    ImGui::TableHeadersRow();

    const auto click_event = [&] {
        if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
            ed.clear_selection();
    };
    click_event();

    for (const auto& [name, scenery] : ed)
    {
        fm_debug_assert(get_atlas(scenery));
        ImGui::TableNextRow(ImGuiTableRowFlags_None, row_height);

        if (ImGui::TableSetColumnIndex(0))
        {
            auto atlas_ = get_atlas(scenery);
            auto& atlas = *atlas_;
            const auto size = Vector2(get_size(scenery, atlas));
            const float c = std::min(thumbnail_width / size[0], row_height / size[1]);
            const auto texcoords = get_texcoords(scenery, atlas);
            const ImVec2 img_size = {size[0]*c, size[1]*c+style.CellPadding.y + 0.5f};
            const ImVec2 uv0 {texcoords[3][0], texcoords[3][1]}, uv1 {texcoords[0][0], texcoords[0][1]};
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.f, .5f*(thumbnail_width - img_size.x)));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + .5f*std::max(0.f, row_height - img_size.y));
            ImGui::Image((void*)&atlas.texture(), img_size, uv0, uv1);
            click_event();
        }
        if (ImGui::TableSetColumnIndex(1))
        {
            constexpr ImGuiSelectableFlags flags = ImGuiSelectableFlags_SpanAllColumns;
            bool selected = is_selected(ed, scenery);
            auto name_ = scenery_name(name, scenery);
            if (ImGui::Selectable(name_.data(), &selected, flags, {0, row_height}) && selected)
                select_tile(ed, scenery);
            click_event();
        }

        if constexpr(do_group_column<T>)
        {
            if (ImGui::TableSetColumnIndex(2))
            {
                text(scenery_type_to_string(scenery));
                click_event();
            }
        }
        else if constexpr (do_path_column<T>)
        {
            if (ImGui::TableSetColumnIndex(2))
            {
                text(scenery_path(scenery));
                click_event();
            }
        }

        if constexpr(do_group_column<T>)
        {
            if (ImGui::TableSetColumnIndex(3))
            {
                auto& atlas = *get_atlas(scenery);
                StringView name = loader.strip_prefix(atlas.name());
                if (auto last = name.findLast('/'))
                    name = name.prefix(last.data());
                else
                    name = {};
                text(name);
                click_event();
            }
        }
    }
}

template void impl_draw_editor_scenery_pane(scenery_editor&, Vector2);
template void impl_draw_editor_scenery_pane(vobj_editor&, Vector2);
template void impl_draw_editor_scenery_pane(wall_editor&, Vector2);

} // namespace

void app::draw_editor_pane(float main_menu_height)
{
    auto* ed = _editor->current_ground_editor();
    auto* wa = _editor->current_wall_editor();
    auto* sc = _editor->current_scenery_editor();
    auto* vo = _editor->current_vobj_editor();

    const auto window_size = M->window_size();
    const auto dpi = M->dpi_scale();

    if (const bool active = M->is_text_input_active();
        ImGui::GetIO().WantTextInput != active)
        active ? M->start_text_input() : M->stop_text_input();

    [[maybe_unused]] const raii_wrapper vars[] = {
        push_style_var(ImGuiStyleVar_WindowPadding, {8*dpi[0], 8*dpi[1]}),
        push_style_var(ImGuiStyleVar_WindowBorderSize, 0),
        push_style_var(ImGuiStyleVar_FramePadding, {4*dpi[0], 4*dpi[1]}),
        push_style_color(ImGuiCol_WindowBg, {0, 0, 0, .5}),
        push_style_color(ImGuiCol_FrameBg, {0, 0, 0, 0}),
    };

    const auto& style = ImGui::GetStyle();

    if (main_menu_height > 0)
    {
        constexpr auto igwf = ImGuiWindowFlags_(ImGuiWindowFlags_NoDecoration |
                                                ImGuiWindowFlags_NoMove |
                                                ImGuiWindowFlags_NoSavedSettings);
        const auto b = push_id("editor");

        float width = 425 * dpi.x();

        ImGui::SetNextWindowPos({0, main_menu_height+style.WindowPadding.y});
        ImGui::SetNextFrameWantCaptureKeyboard(false);
        ImGui::SetNextWindowSize({width, window_size.y()-main_menu_height - style.WindowPadding.y});
        if (auto b = begin_window({}, nullptr, igwf))
        {
            const auto b2 = push_id("editor-pane");
            if (auto b3 = begin_list_box("##atlases", {-FLT_MIN, -1}))
            {
                if (ed)
                    for (const auto& [k, v] : *ed)
                        draw_editor_tile_pane_atlas(*ed, k, v->atlas, dpi);
                else if (sc)
                    impl_draw_editor_scenery_pane<scenery_editor>(*sc, dpi);
                else if (vo)
                    impl_draw_editor_scenery_pane<vobj_editor>(*vo, dpi);
                else if (wa)
                    impl_draw_editor_scenery_pane<wall_editor>(*wa, dpi);
                else if (_editor->mode() == editor_mode::tests)
                    draw_tests_pane(width);
            }
        }
    }
}

} // namespace floormat
