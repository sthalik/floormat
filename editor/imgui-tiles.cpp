#include "app.hpp"
#include "src/tile-atlas.hpp"
#include "floormat/main.hpp"
#include "imgui-raii.hpp"
#include "compat/format.hpp"
#include <Magnum/Math/Color.h>

namespace floormat {

using namespace floormat::imgui;

void app::draw_editor_tile_pane_atlas(tile_editor& ed, StringView name, const std::shared_ptr<tile_atlas>& atlas)
{
    const auto b = push_id("tile-pane");

    const auto dpi = M->dpi_scale();
    constexpr Color4 color_perm_selected{1, 1, 1, .7f},
                     color_selected{1, 0.843f, 0, .8f},
                     color_hover{0, .8f, 1, .7f};
    const float window_width = ImGui::GetWindowWidth() - 32 * dpi;
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
        ImGui::SameLine(window_width - ImGui::CalcTextSize(buf).x - style.FramePadding.x - 4*dpi);
        text({buf, len});
    };
    if (const auto flags = ImGuiTreeNodeFlags_(ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Framed);
        auto b = tree_node(name.data(), flags))
    {
        do_caption();
        [[maybe_unused]] const raii_wrapper vars[] = {
            push_style_var(ImGuiStyleVar_FramePadding, {2*dpi, 2*dpi}),
            push_style_color(ImGuiCol_ButtonHovered, color_hover),
        };
        const bool perm_selected = ed.is_permutation_selected(atlas);
        constexpr std::size_t per_row = 8;
        for (std::size_t i = 0; i < N; i++)
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
            ImGui::ImageButton(buf, (void*)&atlas->texture(), ImVec2(size_2.x * dpi, size_2.y * dpi),
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

} // namespace floormat
