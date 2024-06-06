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
#include <mg/Color.h>

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

bool hole_test::handle_key(app&, const key_event&, bool) { return false; }
bool hole_test::handle_mouse_click(app&, const mouse_button_event&, bool) { return false; }
bool hole_test::handle_mouse_move(app&, const mouse_move_event&) { return false; }
void hole_test::draw_overlay(app&) {}

constexpr ImVec2 to_imvec2(Vector2 val) { return {val.x(), val.y()}; }
uint32_t to_color(Color4 val) { return ImGui::ColorConvertFloat4ToU32({ val.r(), val.g(), val.b(), val.a() }); }

constexpr auto colors = std::array{
    0x488f31_rgbf, // rect 1
    0x91ac56_rgbf, // rect 2
    0xcdca85_rgbf, // rect 3
    0xffebbc_rgbf, // rect 4
    0xf8d29d_rgbf, // rect 5
    0xec9c6d_rgbf, // rect 6
    0xda58de_rgbf, // rect 7
    0x6dd4ff_rgbf, // rect 8
};

void hole_test::draw_ui(app& a, float)
{
    using Cr = CutResult<Int>;
    using bbox = Cr::bbox;
    const auto& m = a.main();
    const auto width = Math::min(ImGui::GetWindowSize().x, 400.f);
    const auto window_size = ImVec2{width, width};
    const auto bgcolor = ImGui::ColorConvertFloat4ToU32({0, 0, 0, 1});
    const auto blue = ImGui::ColorConvertFloat4ToU32({0, .5f, 1, 1});
    const auto red = ImGui::ColorConvertFloat4ToU32({1, 0, 0, 1});
    const auto gray = ImGui::ColorConvertFloat4ToU32({.7f, .7f, .7f, 1});
    const auto& style = ImGui::GetStyle();
    //const auto dpi = m.dpi_scale();
    constexpr auto igcf = ImGuiChildFlags_None;
    constexpr auto igwf = 0;//ImGuiWindowFlags_NoDecoration;
    constexpr auto imdf = ImDrawFlags_None;
    char buf[32];
    ImGui::NewLine();

    bbox rect{{}, Vector2ub{tile_size_xy}};
    const auto res = Cr::cut(rect, {st.pos, st.size});

    ImGui::SetNextWindowSize({width, width});
    if (auto b1 = imgui::begin_child("Test area"_s, window_size, igcf, igwf))
    {
        const auto& win = *ImGui::GetCurrentWindow();
        const auto min  = Vector2{win.Pos.x, win.Pos.y};
        const auto max  = min + Vector2{width};
        const auto maxʹ = max - Vector2{1};
        constexpr float mult = 2;
        const auto center = min + Vector2{width*.5f};
        ImDrawList& draw = *win.DrawList;

        draw.AddRectFilled(to_imvec2(min), to_imvec2(maxʹ), bgcolor, 0, imdf); // black
        draw.AddRect(to_imvec2(center - TILE_SIZE2*.5f*mult), to_imvec2(center + TILE_SIZE2*.5f*mult), gray, 0, imdf); // standard tile
        draw.AddRectFilled(to_imvec2(center + (Vector2(st.pos) - Vector2(st.size)/2)*mult),
                           to_imvec2(center + (Vector2(st.pos) + Vector2(st.size)/2)*mult), red); // hole

        for (auto i = 0u; i < res.size; i++)
        {
            auto r = res.array[i];
            draw.AddRectFilled(to_imvec2(center + Vector2(r.min)*mult), to_imvec2(center + Vector2(r.max)*mult), to_color(colors[i])); // rects filled
        }

        for (auto i = 0u; i < res.size; i++)
        {
            auto r = res.array[i];
            draw.AddRect(to_imvec2(center + Vector2(r.min)*mult), to_imvec2(center + Vector2(r.max)*mult), blue, 0, 0, 3); // rects
        }

        draw.AddRect(to_imvec2(center + (Vector2(st.pos) - Vector2(st.size)*.5f)*mult),
                     to_imvec2(center + (Vector2(st.pos) + Vector2(st.size)*.5f)* mult), red, 0, 0, 1); // hole
    }

    const auto label_width = ImGui::CalcTextSize("MMMMMM").x;

    ImGui::NewLine();
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
    {
        label_left("count", buf, label_width);
        ImGui::Text("%zu", size_t{res.size});
    }
    {
        label_left("found", buf, label_width);
        ImGui::Text("%s", res.found ? "true" : "false");
    }

    ImGui::Unindent(style.FramePadding.x);
}

void hole_test::update_pre(app&, const Ns&)
{
}

} // namespace

Pointer<base_test> tests_data::make_test_hole() { return Pointer<hole_test>{InPlaceInit}; }

} // namespace floormat::tests
