#include "app.hpp"
#include "scenery-editor.hpp"
#include "imgui-raii.hpp"
#include "anim-atlas.hpp"
#include "loader/loader.hpp"
#include "compat/format.hpp"
#include "floormat/main.hpp"

namespace floormat {

using namespace floormat::imgui;

void app::draw_editor_scenery_pane(scenery_editor& ed)
{
    const auto dpi = M->dpi_scale();
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_ScrollY;
    constexpr int ncolumns = 4;
    const auto size = ImGui::GetWindowSize();
    auto b = imgui::begin_table("scenery-table", ncolumns, flags, size);
    const auto font_size = ImGui::GetCurrentContext()->FontSize;
    const auto min_row_height = font_size;
    constexpr auto thumbnail_width = 40;
#if 0
    if (!_scenery_table)
    {
        _scenery_table = new ImGuiTable{};
        ImGui::TableBeginInitMemory(_scenery_table, ncolumns);
    }
#endif
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

    for (const auto& [name, scenery] : ed)
    {
        char buf[128];
        bool selected = false;
        fm_debug_assert(scenery.proto.atlas != nullptr);
        ImGui::TableNextRow(ImGuiTableRowFlags_None, min_row_height);

        {
            ImGui::TableSetColumnIndex(0);
            snformat(buf, "##sc_{}"_cf, name.data());
            ImGui::Selectable(buf, &selected, ImGuiSelectableFlags_SpanAllColumns);
        }
        {
            ImGui::TableNextColumn();
            text(name);
        }
        {
            ImGui::TableNextColumn();
            switch (scenery.proto.frame.type)
            {
            case scenery_type::none: text("none"); break;
            case scenery_type::generic: text("generic"); break;
            case scenery_type::door: text("door"); break;
            default: text("unknown"); break;
            }
        }
        {
            ImGui::TableNextColumn();
            StringView name = scenery.proto.atlas->name();
            if (name.hasPrefix(loader.SCENERY_PATH))
                name = name.exceptPrefix(loader.SCENERY_PATH.size());
            if (auto last = name.findLast('/'))
                name = name.prefix(last.data());
            else
                name = {};
            text(name);
        }
        if (selected)
            ed.select_tile(scenery);
    }
    //ImGui::TableUpdateColumnsWeightFromWidth(_scenery_table);
}

} // namespace floormat
