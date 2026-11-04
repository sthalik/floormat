#include "app.hpp"
#include "src/tile-constants.hpp"
#include "compat/array-size.hpp"
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
#include "loader/ground-cell.hpp"
#include "src/sprite-atlas.hpp"
#include "floormat/main.hpp"
#include <mg/Color.h>
#include <imgui_internal.h>

namespace floormat {

using namespace floormat::imgui;

namespace {

template<typename T> constexpr inline bool has_type_and_group_columns = false;
template<> constexpr inline bool has_type_and_group_columns<scenery_editor> = true;
template<typename T> constexpr inline bool has_path_column = false;
template<> constexpr inline bool has_path_column<wall_editor> = true;

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

StringView item_path(const wall_cell& wa) { return wa.atlas->name(); }
StringView item_name(StringView, const scenery_& sc) { return sc.name; }
StringView item_name(StringView, const vobj_& vobj) { return vobj.descr; }
StringView item_name(StringView, const wall_cell& w) { return w.name; }
bptr<anim_atlas> get_atlas(const scenery_& sc) { return sc.proto.atlas; }
bptr<anim_atlas> get_atlas(const vobj_& vobj) { return vobj.factory->atlas(); }
bptr<wall_atlas> get_atlas(const wall_cell& w) { return w.atlas; }
Vector2ui get_size(const auto&, anim_atlas& atlas) { return atlas.frame(atlas.first_rotation(), 0).size; }

const sprite* canonical_wall_sprite(wall_atlas& atlas)
{
    using namespace floormat::Wall;
    for (auto dir : { Direction_::N, Direction_::W })
        if (const auto* g = atlas.group(dir, Group_::wall); g && g->count > 0)
        {
            auto sprites = atlas.sprites(*g);
            if (!sprites.isEmpty())
                return &sprites[0];
        }
    return nullptr;
}

Vector2ui get_size(const auto&, wall_atlas& atlas)
{
    if (const sprite* s = canonical_wall_sprite(atlas))
        return { s->width(), s->height() };
    return {1, 1};
}

void palette_image(editor& e, sprite s, ImVec2 size)
{
    ImGui::Dummy(size);
    if (!ImGui::IsItemVisible())
        return;
    const auto t = e.palette_texture(s);
    const auto& uv = t.uv;
    const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddImageQuad((ImTextureID)t.id, min, {max.x, min.y}, max, {min.x, max.y},
                                             {uv[0].x(), uv[0].y()}, {uv[1].x(), uv[1].y()},
                                             {uv[2].x(), uv[2].y()}, {uv[3].x(), uv[3].y()});
}

// Behaves like ImGui::ImageButtonEx but draws the image into channel 1 of `splitter`, which the caller must have
// split, so frames and images each merge into a few draw commands. Leaves channel 0 current.
bool palette_image_button(editor& e, sprite s, const char* str_id, ImVec2 image_size, ImDrawListSplitter& splitter)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    const auto& style = ImGui::GetStyle();
    const ImVec2 padding = style.FramePadding, pos = window->DC.CursorPos;
    const ImRect bb(pos, {pos.x + image_size.x + padding.x * 2, pos.y + image_size.y + padding.y * 2});
    const ImGuiID id = window->GetID(str_id);
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered, held;
    const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    const ImU32 col = ImGui::GetColorU32(held && hovered ? ImGuiCol_ButtonActive
                                         : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
    ImGui::RenderNavCursor(bb, id);
    ImGui::RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);

    const auto t = e.palette_texture(s);
    const auto& uv = t.uv;
    const ImVec2 min{bb.Min.x + padding.x, bb.Min.y + padding.y}, max{bb.Max.x - padding.x, bb.Max.y - padding.y};
    splitter.SetCurrentChannel(window->DrawList, 1);
    window->DrawList->AddImageQuad((ImTextureID)t.id, min, {max.x, min.y}, max, {min.x, max.y},
                                   {uv[0].x(), uv[0].y()}, {uv[1].x(), uv[1].y()},
                                   {uv[2].x(), uv[2].y()}, {uv[3].x(), uv[3].y()});
    splitter.SetCurrentChannel(window->DrawList, 0);
    return pressed;
}

void palette_thumbnail(editor& e, ImVec2 img_size, const auto&, anim_atlas& atlas)
{
    const auto r = atlas.first_rotation();
    const auto& g = atlas.group(r);
    const auto* sp = g.sprites.isEmpty() ? nullptr : g.sprites[0];
    if (!sp)
        return;
    palette_image(e, sprite{sp}, img_size);
}

void palette_thumbnail(editor& e, ImVec2 img_size, const wall_cell&, wall_atlas& atlas)
{
    if (const sprite* s = canonical_wall_sprite(atlas)) [[likely]]
        palette_image(e, *s, img_size);
}


void draw_ground_palette_atlas(editor& e, ground_editor& ed, StringView name, const bptr<ground_atlas>& atlas,
                               Vector2 dpi, ImDrawListSplitter& splitter)
{
    const auto b = push_id("tile-pane");

    constexpr Color4 color_perm_selected{1, 1, 1, .7f},
                     color_selected{1, 0.843f, 0, .8f},
                     color_hover{0, .8f, 1, .7f};
    const float window_width = ImGui::GetWindowWidth() - 32 * dpi[0];
    char buf[128];
    const auto& style = ImGui::GetStyle();
    const auto N = atlas->num_tiles();

    const auto handle_click = [&] {
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            ed.select_tile_permutation(atlas);
        else if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
            ed.clear_selection();
    };
    const auto do_caption = [&] {
        handle_click();
        if (ed.is_atlas_selected(atlas))
        {
            ImGui::SameLine();
            text(" (selected)");
        }
        const auto len = snformat(buf, "{:d}", N);
        fm_assert(len < array_size(buf));
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
        const auto sprites = atlas->raw_sprite_array();
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

            snformat(buf, "##item_{}", i);
            constexpr ImVec2 size_2 = { TILE_SIZE[0]*.5f, TILE_SIZE[1]*.5f };
            palette_image_button(e, sprites[i], buf, ImVec2(size_2.x * dpi[0], size_2.y * dpi[1]), splitter);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                ed.select_tile(atlas, i);
            else
                handle_click();
            ImGui::SameLine();
        }
        ImGui::NewLine();
    }
    else
        do_caption();
}

template<typename T>
void draw_palette_pane(editor& e, T& ed, Vector2 dpi)
{
    const auto b1 = push_id("palette-pane");

    const auto& style = ImGui::GetStyle();
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
    constexpr int ncolumns = has_type_and_group_columns<T> ? 4 : has_path_column<T> ? 3 : 2;
    const auto size = ImGui::GetWindowSize();
    auto b2 = imgui::begin_table("palette-table", ncolumns, flags, size);
    if (!b2)
        return;
    const auto row_height = ImGui::GetCurrentContext()->FontSize + 10*dpi[1];
    constexpr auto thumbnail_width = 50;
    ImGui::TableSetupScrollFreeze(1, 1);
    constexpr auto colflags_ = ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoSort;
    constexpr auto colflags = colflags_ | ImGuiTableColumnFlags_WidthFixed;
    ImGui::TableSetupColumn("##thumbnail", colflags, thumbnail_width);
    ImGui::TableSetupColumn("Name", colflags_ | ImGuiTableColumnFlags_WidthStretch);
    if constexpr(has_type_and_group_columns<T>)
    {
        const auto colwidth_type = ImGui::CalcTextSize("generic").x;
        const auto colwidth_group = ImGui::CalcTextSize("MMMMMMMMMMMMMMM").x;
        ImGui::TableSetupColumn("Type", colflags, colwidth_type);
        ImGui::TableSetupColumn("Group", colflags, colwidth_group);
    }
    else if constexpr(has_path_column<T>)
    {
        const auto colwidth_path = ImGui::CalcTextSize("/MMMMMMMMMMMMMMM").x;
        ImGui::TableSetupColumn("Path", colflags, colwidth_path);
    }
    ImGui::TableHeadersRow();

    const auto clear_on_middle_click = [&] {
        if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
            ed.clear_selection();
    };
    clear_on_middle_click();

    for (const auto& [name, item] : ed)
    {
        fm_debug_assert(get_atlas(item));
        const auto b3 = push_id(name);
        ImGui::TableNextRow(ImGuiTableRowFlags_None, row_height);

        if (ImGui::TableSetColumnIndex(0))
        {
            auto atlasʹ = get_atlas(item);
            auto& atlas = *atlasʹ;
            const auto size = Vector2(get_size(item, atlas));
            const float c = Math::min(thumbnail_width / size[0], row_height / size[1]);
            const ImVec2 img_size = {size[0]*c, size[1]*c+style.CellPadding.y + 0.5f};
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + Math::max(0.f, .5f*(thumbnail_width - img_size.x)));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + .5f*Math::max(0.f, row_height - img_size.y));
            palette_thumbnail(e, img_size, item, atlas);
            clear_on_middle_click();
        }
        if (ImGui::TableSetColumnIndex(1))
        {
            constexpr ImGuiSelectableFlags flags = ImGuiSelectableFlags_SpanAllColumns;
            bool selected = ed.is_item_selected(item);
            auto nameʹ = item_name(name, item);
            if (ImGui::Selectable(nameʹ.data(), &selected, flags, {0, row_height}) && selected)
                ed.select_item(item);
            clear_on_middle_click();
        }

        if constexpr(has_type_and_group_columns<T>)
        {
            if (ImGui::TableSetColumnIndex(2))
            {
                text(scenery_type_to_string(item));
                clear_on_middle_click();
            }
        }
        else if constexpr (has_path_column<T>)
        {
            if (ImGui::TableSetColumnIndex(2))
            {
                text(item_path(item));
                clear_on_middle_click();
            }
        }

        if constexpr(has_type_and_group_columns<T>)
        {
            if (ImGui::TableSetColumnIndex(3))
            {
                auto& atlas = *get_atlas(item);
                StringView name = loader.strip_prefix(atlas.name());
                if (auto last = name.findLast('/'))
                    name = name.prefix(last.data());
                else
                    name = {};
                text(name);
                clear_on_middle_click();
            }
        }
    }
}

template void draw_palette_pane(editor&, scenery_editor&, Vector2);
template void draw_palette_pane(editor&, vobj_editor&, Vector2);
template void draw_palette_pane(editor&, wall_editor&, Vector2);

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

    const auto frame_bg = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
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
        if (_editor->mode() != editor_mode::tests)
            ImGui::SetNextFrameWantCaptureKeyboard(false);
        ImGui::SetNextWindowSize({width, window_size.y()-main_menu_height - style.WindowPadding.y});
        if (auto b = begin_window({}, nullptr, igwf))
        {
            const auto b2 = push_id("editor-pane");
            const auto b2ʹ = push_id((int)_editor->mode());
            if (auto b3 = begin_list_box("##atlases", {-FLT_MIN, -1}))
            {
                if (ed)
                {
                    static ImDrawListSplitter splitter;
                    auto* draw_list = ImGui::GetWindowDrawList();
                    splitter.Split(draw_list, 2);
                    for (const auto& [k, v] : *ed)
                        draw_ground_palette_atlas(*_editor, *ed, k, v.atlas, dpi, splitter);
                    splitter.Merge(draw_list);
                }
                else if (sc)
                    draw_palette_pane<scenery_editor>(*_editor, *sc, dpi);
                else if (vo)
                    draw_palette_pane<vobj_editor>(*_editor, *vo, dpi);
                else if (wa)
                    draw_palette_pane<wall_editor>(*_editor, *wa, dpi);
                else if (_editor->mode() == editor_mode::tests)
                {
                    // With no frame, an unchecked checkbox shows only its label.
                    const auto b4 = push_style_color(ImGuiCol_FrameBg, {frame_bg.x, frame_bg.y, frame_bg.z, frame_bg.w});
                    draw_tests_pane(width);
                }
            }
        }
    }
}

} // namespace floormat
