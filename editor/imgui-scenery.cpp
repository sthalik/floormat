#include "app.hpp"
#include "scenery-editor.hpp"
#include "imgui-raii.hpp"
#include "anim-atlas.hpp"
#include "loader/loader.hpp"
#include "floormat/main.hpp"

namespace floormat {

using namespace floormat::imgui;

void app::draw_editor_scenery_pane(scenery_editor& ed)
{
    const auto b1 = push_id("scenery-pane");

    const auto& style = ImGui::GetStyle();
    const auto dpi = M->dpi_scale();
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
    constexpr int ncolumns = 4;
    const auto size = ImGui::GetWindowSize();
    auto b2 = imgui::begin_table("scenery-table", ncolumns, flags, size);
    const auto row_height = ImGui::GetCurrentContext()->FontSize + 10*dpi[1];
    constexpr auto thumbnail_width = 50;
    const auto colwidth_type = ImGui::CalcTextSize("generic").x;
    const auto colwidth_group = ImGui::CalcTextSize("MMMMMMMMMMMMMMM").x;
    ImGui::TableSetupScrollFreeze(1, 1);
    constexpr auto colflags_ = ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoSort;
    constexpr auto colflags = colflags_ | ImGuiTableColumnFlags_WidthFixed;
    ImGui::TableSetupColumn("##thumbnail", colflags, thumbnail_width);
    ImGui::TableSetupColumn("Name", colflags_ | ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Type", colflags, colwidth_type);
    ImGui::TableSetupColumn("Group", colflags, colwidth_group);
    ImGui::TableHeadersRow();

    const auto click_event = [&] {
        if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
            ed.clear_selection();
    };
    click_event();

    for (const auto& [name, scenery] : ed)
    {
        fm_debug_assert(scenery.proto.atlas != nullptr);
        ImGui::TableNextRow(ImGuiTableRowFlags_None, row_height);

        if (ImGui::TableSetColumnIndex(0))
        {
            auto& atlas = *scenery.proto.atlas;
            const auto r = atlas.next_rotation_from(rotation_COUNT);
            const auto& frame = atlas.frame(r, 0);
            const auto size = Vector2(frame.size);
            const float c = std::min(thumbnail_width / size[0], row_height / size[1]);
            const auto texcoords = atlas.texcoords_for_frame(r, 0, !atlas.group(r).mirror_from.isEmpty());
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
            bool selected = ed.is_item_selected(scenery);
            if (ImGui::Selectable(name.data(), &selected, flags, {0, row_height}) && selected)
                ed.select_tile(scenery);
            click_event();
        }
        if (ImGui::TableSetColumnIndex(2))
        {
            switch (scenery.proto.frame.type)
            {
            case scenery_type::none: text("none"); break;
            case scenery_type::generic: text("generic"); break;
            case scenery_type::door: text("door"); break;
            default: text("unknown"); break;
            }
            click_event();
        }
        if (ImGui::TableSetColumnIndex(3))
        {
            StringView name = loader.strip_prefix(scenery.proto.atlas->name());
            if (auto last = name.findLast('/'))
                name = name.prefix(last.data());
            else
                name = {};
            text(name);
            click_event();
        }
    }
}

} // namespace floormat
