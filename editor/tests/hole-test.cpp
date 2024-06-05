#include "../tests-private.hpp"
#include "compat/shared-ptr-wrapper.hpp"
#include "src/tile-constants.hpp"
#include "src/chunk-region.hpp"
#include "src/hole.hpp"
#include "src/object.hpp"
#include "src/world.hpp"
#include "../app.hpp"
#include "../imgui-raii.hpp"
#include "floormat/main.hpp"
#include "src/critter.hpp"

namespace floormat::tests {
namespace {

using namespace floormat::imgui;

struct State
{
    Vector2i pos;
    Vector2ub size{tile_size_xy/4};
};

struct hole_test final : base_test
{
    ~hole_test() noexcept override = default;

    bool handle_key(app& a, const key_event& e, bool is_down) override;
    bool handle_mouse_click(app& a, const mouse_button_event& e, bool is_down) override;
    bool handle_mouse_move(app& a, const mouse_move_event& e) override;
    void draw_overlay(app& a) override;
    void draw_ui(app& a, float menu_bar_height) override;
    void update_pre(app& a, const Ns& dt) override;
    void update_post(app&, const Ns&) override {}

    State st;
};

bool hole_test::handle_key(app& a, const key_event& e, bool is_down)
{
    return false;
}

bool hole_test::handle_mouse_click(app& a, const mouse_button_event& e, bool is_down)
{
    return false;
}

bool hole_test::handle_mouse_move(app& a, const mouse_move_event& e)
{
    return false;
}

void hole_test::draw_overlay(app& a)
{
}

constexpr ImVec2 to_imvec2(Vector2 val)
{
    return {val.x(), val.y()};
}

void hole_test::draw_ui(app& a, float menu_bar_height)
{
    const auto& m = a.main();
    const auto width = Math::min(ImGui::GetWindowSize().x, 400.f);
    const auto window_size = ImVec2{width, width};
    const auto bgcolor = ImGui::ColorConvertFloat4ToU32({0, 0, 0, 1});
    const auto bgrect = ImGui::ColorConvertFloat4ToU32({.25f, .25f, .25f, 1.f});
    const auto blue = ImGui::ColorConvertFloat4ToU32({0, .5f, 1, 1});
    const auto red = ImGui::ColorConvertFloat4ToU32({1, 0, 0, 1});
    const auto gray = ImGui::ColorConvertFloat4ToU32({.7f, .7f, .7f, .6f});
    const auto& style = ImGui::GetStyle();
    //const auto dpi = m.dpi_scale();
    constexpr auto igcf = ImGuiChildFlags_None;
    constexpr auto igwf = 0;//ImGuiWindowFlags_NoDecoration;
    constexpr auto imdf = ImDrawFlags_None;
    char buf[32];

    ImGui::NewLine();

    //ImGui::LabelText("##test-area", "Test area");
    //ImGui::NewLine();

    ImGui::SetNextWindowSize({width, width});
    if (auto b1 = imgui::begin_child("Test area"_s, window_size, igcf, igwf))
    {
        const auto& win = *ImGui::GetCurrentWindow();
        const auto min  = Vector2{win.Pos.x, win.Pos.y};
        const auto max  = min + Vector2{width};
        const auto maxʹ = max - Vector2{1};

        ImDrawList& draw = *win.DrawList;
        draw.AddRectFilled(to_imvec2(min), to_imvec2(maxʹ), bgcolor, 0, imdf);

        const auto center = Vector2{width*.5f};
        constexpr auto size = TILE_SIZE2;
        draw.AddRect(to_imvec2(min + center - size*.5f), to_imvec2(min + center + size*.5f), gray, 0, imdf);

        cut_rectangle_result::bbox rect{{}, Vector2ub{tile_size_xy}};
        cut_rectangle_result res = cut_rectangle(rect, {st.pos, st.size});

        for (auto i = 0u; i < res.size; i++)
        {
            auto r = res.array[i];
            draw.AddRectFilled(to_imvec2(min + center + Vector2(r.min)), to_imvec2(min + center + Vector2(r.max)), bgrect);
        }

        for (auto i = 0u; i < res.size; i++)
        {
            auto r = res.array[i];
            draw.AddRect(to_imvec2(min + center + Vector2(r.min)), to_imvec2(min + center + Vector2(r.max)), blue);
        }

        draw.AddRect(to_imvec2(min + center + Vector2(st.pos) - Vector2(st.size/2)),
                     to_imvec2(min + center + Vector2(st.pos) + Vector2(st.size / 2)), red);
    }
    //ImGui::NewLine();
    const auto label_width = ImGui::CalcTextSize("MMMMMMMMM").x;

    ImGui::Indent(style.FramePadding.x);

    {
        constexpr auto step_1 = Vector2i{1};
        constexpr auto step_2 = Vector2i{tile_size_xy/4};
        label_left("pos", buf, label_width);
        ImGui::InputScalarN("##pos", ImGuiDataType_S32, st.pos.data(), 2, step_1.data(), step_2.data());
    }
    {
        constexpr auto step_1 = Vector2i{1};
        constexpr auto step_2 = Vector2i{4};
        label_left("size", buf, label_width);
        ImGui::InputScalarN("##size", ImGuiDataType_U8, st.size.data(), 2, step_1.data(), step_2.data());
    }

    ImGui::Unindent(style.FramePadding.x);
}

void hole_test::update_pre(app& a, const Ns& dt)
{
}

} // namespace

Pointer<base_test> tests_data::make_test_hole() { return Pointer<hole_test>{InPlaceInit}; }

} // namespace floormat::tests
