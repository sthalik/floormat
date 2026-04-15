#include "app.hpp"
#include "src/tile-constants.hpp"
#include "compat/format.hpp"
#include "editor.hpp"
#include "ground-editor.hpp"
#include "wall-editor.hpp"
#include "scenery-editor.hpp"
#include "floormat/main.hpp"
#include "src/world.hpp"
#include "src/anim-atlas.hpp"
#include "shaders/shader.hpp"
#include "shaders/lightmap.hpp"
#include "main/clickable.hpp"
#include "imgui-raii.hpp"
#include "src/light.hpp"
#include "loader/loader.hpp"
#include "src/point.inl"
#include <tuple>
#include <fmt/ranges.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/ImGuiIntegration/Context.h>
#include <imgui.h>

namespace floormat {

using namespace floormat::imgui;

bool popup_target::operator==(const popup_target&) const = default;

void app::init_imgui(Vector2i size)
{
    if (!_imgui->context()) [[unlikely]]
    {
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;

        {
            ImFontConfig config;
            config.OversampleH = 1;
            config.PixelSnapH = true;
            config.RasterizerMultiply = 1.0f;

            io.Fonts->Clear();
#ifdef IMGUI_DISABLE_DEFAULT_FONT
            auto res = loader.font();
            void* imgui_font_data = IM_ALLOC(res.size());
            memcpy(imgui_font_data, res.data(), res.size());
            io.Fonts->AddFontFromMemoryTTF(imgui_font_data, (int)res.size(), 16, &config);
#else
            io.Fonts->AddFontDefault(&config);
#endif
            io.Fonts->Build();
        }

        _imgui = safe_ptr<ImGuiIntegration::Context>{InPlaceInit, NoCreate};
        *_imgui = ImGuiIntegration::Context{*ImGui::GetCurrentContext(), Vector2{size}, size, size};
        fm_assert(_imgui->context());
    }

    _imgui->relayout(Vector2{size}, size, size);

    _imgui->atlasTexture().setMagnificationFilter(Magnum::GL::SamplerFilter::Nearest)
                          .setMinificationFilter(Magnum::GL::SamplerFilter::Nearest);
}

void app::render_menu()
{
    _imgui->drawFrame();
}

float app::draw_main_menu()
{
    float main_menu_height = 0;
    if (auto b = begin_main_menu())
    {
        if (auto b = begin_menu("File"))
        {
            bool do_new = false, do_quickload = false, do_quit = false;
            ImGui::MenuItem("New", nullptr, &do_new);
            ImGui::Separator();
            ImGui::MenuItem("Load quicksave", nullptr, &do_quickload);
            ImGui::Separator();
            ImGui::MenuItem("Quit", "Ctrl+Q", &do_quit);
            if (do_new)
                do_key(key_new_file);
            else if (do_quickload)
                do_key(key_quickload);
            else if (do_quit)
                do_key(key_quit);
        }
        if (auto b = begin_menu("Editor"))
        {
            auto mode = _editor->mode();
            using m = editor_mode;
            const auto* ed_sc = _editor->current_scenery_editor();
            const auto* ed_gr = _editor->current_ground_editor();
            const auto* ed_wa = _editor->current_wall_editor();
            const bool b_rotate = ed_sc && ed_sc->is_anything_selected() ||
                                  ed_gr && ed_gr->is_anything_selected() ||
                                  ed_wa && ed_wa->is_anything_selected();

            bool m_none    = mode == m::none, m_floor = mode == m::floor, m_walls = mode == m::walls,
                 m_scenery = mode == m::scenery, m_vobjs = mode == m::vobj, m_tests = mode == m::tests,
                 b_collisions = _render_bboxes, b_clickables = _render_clickables,
                 b_vobjs = _render_vobjs, b_all_z_levels = _render_all_z_levels;

            ImGui::SeparatorText("Mode");
            if (ImGui::MenuItem("Select",  "1", m_none))
                do_key(key_mode_none);
            if (ImGui::MenuItem("Floor",   "2", m_floor)) // todo rename to 'ground'
                do_key(key_mode_floor);
            if (ImGui::MenuItem("Walls",   "3", m_walls))
                do_key(key_mode_walls);
            if (ImGui::MenuItem("Scenery", "4", m_scenery))
                do_key(key_mode_scenery);
            if (ImGui::MenuItem("Virtual objects", "5", m_vobjs))
                do_key(key_mode_vobj);
            if (ImGui::MenuItem("Functional tests", "6", m_tests))
                do_key(key_mode_tests);
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

        main_menu_height = ImGui::GetContentRegionAvail().y;
    }
    return main_menu_height;
}

void app::configure_imgui(float scale)
{
    constexpr struct {
        float s, ret;
    } scale_table[] = {
        { 3.75f, 4 },
        { 2.75f, 3 },
        { 1.75f, 2 },
        { 0    , 1 },
    };

    for (auto [s, ret] : scale_table)
        if (scale >= s + 1e-6f)
        {
            scale = ret;
            break;
        }

    auto& style = ImGui::GetStyle();
    style = ImGuiStyle();
    ImGui::StyleColorsDark(&style);
    style.ScaleAllSizes(scale);

    auto& io = ImGui::GetIO();
    io.FontGlobalScale = scale;

    auto& ctx = *ImGui::GetCurrentContext();
    ctx.FontSize = 13;
    ctx.FontBaseSize = 13;
    ctx.FontScale = 1;
}

void app::draw_ui()
{
    configure_imgui( M->dpi_scale().x());
    _imgui->newFrame();

    if (_render_clickables)
        draw_clickables();
    if (_render_vobjs)
        draw_light_info();
    if (_editor->mode() == editor_mode::tests)
        draw_tests_overlay();
    const float main_menu_height = draw_main_menu();

    //[[maybe_unused]] auto font = font_saver{ctx.FontSize*dpi};

    draw_lightmap_test(main_menu_height);

    if (_editor->current_ground_editor() || _editor->current_wall_editor() ||
        _editor->current_scenery_editor() ||
        _editor->current_vobj_editor() || _editor->mode() == editor_mode::tests)
        draw_editor_pane(main_menu_height);
    draw_fps();

    draw_tile_under_cursor();
    if (_editor->mode() == editor_mode::none)
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
    }

    // draw slope lines on static scenery
    const auto& shader = M->shader();
    const auto win_size = M->window_size();
    for (const auto& ch : M->world().chunks())
    {
        for (const auto& eʹ : ch.objects())
        {
            const auto& e = *eʹ;

            if (e.is_dynamic())
                continue;

            constexpr auto f = tile_shader::foreshortening_factor;
            const auto& atlas = *e.atlas;
            const auto& frame = atlas.frame(e.r, e.frame);
            const auto& g = atlas.group(e.r);
            const auto bb_half = Vector2(e.bbox_size) * 0.5f;
            const float denom = bb_half.x() + bb_half.y();
            const float slope = denom > 0.f ? f * (bb_half.x() - bb_half.y()) / denom : 0.f;

            // bbox center screen offset from sprite's ground anchor
            const auto bbox_scr = tile_shader::project(Vector3(Vector2(eʹ->bbox_offset), 0.f) - Vector3(g.offset));

            // sprite screen extent (pixel offsets from ground anchor)
            const float left_x   = float(-frame.ground.x());
            const float right_x  = float(frame.size.x()) - float(frame.ground.x());

            // slope line y-value at left and right sprite edges
            const float y_at_left  = bbox_scr.y() + slope * (left_x - bbox_scr.x());
            const float y_at_right = bbox_scr.y() + slope * (right_x - bbox_scr.x());

            // sprite's ground anchor on screen
            const Vector2 center = Vector2(shader.camera_offset()) + Vector2(win_size)*.5f
                                 + shader.project(Vector3(e.position()) + Vector3(g.offset));
            const auto start = Vector2{center.x() + left_x,  center.y() + y_at_left};
            const auto end   = Vector2{center.x() + right_x, center.y() + y_at_right};
            draw.AddLine({start.x(), start.y()}, {end.x(), end.y()}, color, thickness);

            // vertical split midpoint (between slope line and top edge)
            const float mid_x = (left_x + right_x) * 0.5f;
            const float y_at_mid = bbox_scr.y() + slope * (mid_x - bbox_scr.x());
            const float top_y = -float(frame.ground.y());
            const auto vsplit_start = Vector2{center.x() + mid_x, center.y() + y_at_mid};
            const auto vsplit_end   = Vector2{center.x() + mid_x, center.y() + top_y};
            draw.AddLine({vsplit_start.x(), vsplit_start.y()}, {vsplit_end.x(), vsplit_end.y()}, color, thickness);
        }
    }
}

void app::draw_light_info()
{
    ImDrawList& draw = *ImGui::GetBackgroundDrawList();
    const auto dpi = M->dpi_scale();
    const auto& style = ImGui::GetStyle();
    const ImVec2 pad { style.FramePadding.x*.5f, 0 };

    for (const auto& x : M->clickable_scenery())
    {
        if (x.e->type() == object_type::light)
        {
            const auto dest = Math::Range2D<float>(x.dest);
            const auto& e = static_cast<const light&>(*x.e);

            StringView falloff;
            switch (e.falloff)
            {
            default: falloff = "?"_s; break;
            case light_falloff::constant: falloff = "constant"_s; break;
            case light_falloff::linear: falloff = "linear"_s; break;
            case light_falloff::quadratic: falloff = "quadratic"_s; break;
            }

            StringView s, s_falloff, s_range, s_radius;
            char buf[64], buf1[32], buf2[32], buf3[32];

            s_falloff    = { buf1, snformat(buf1, "{}"_cf, falloff) };
            if (e.max_distance > 0)
                s_range  = { buf2, snformat(buf2, "range={}"_cf, e.max_distance) };
            if (e.radius > 0)
                s_radius = { buf3, snformat(buf3, "radius={}"_cf, e.radius) };

            constexpr auto space = fmt::string_view{" "};
            s = { buf, snformat(buf, "{}"_cf, fmt::join(std::tuple{s_falloff, s_range, s_radius}, space)) };

            auto text_size = ImGui::CalcTextSize(s.begin(), s.end());

            float offy = dest.max().y() + 5 * dpi.y();
            float offx = dest.min().x() + (dest.max().x() - dest.min().x())*.5f - text_size.x*.5f + 9*dpi.x()*.5f;

            constexpr auto inv_255 = 1.f/255;
            auto color = Vector4(e.color)*inv_255 * e.color.a()*inv_255;
            auto color_pad_y = 3 * dpi.y();

            draw.AddRectFilled({offx-pad.x - 9 * dpi.x(), offy-pad.y},
                               {offx + text_size.x + pad.x, offy + text_size.y + pad.y},
                               ImGui::ColorConvertFloat4ToU32({0, 0, 0, 1}));
            draw.AddText({offx, offy}, ImGui::ColorConvertFloat4ToU32({1, 1, 0, 1}), s.begin(), s.end());
            draw.AddRectFilled({offx-pad.x - 5 * dpi.x(), offy-pad.y + color_pad_y},
                               {offx-pad.x + 0 * dpi.x(), offy-pad.y + text_size.y - color_pad_y + 1},
                               ImGui::ColorConvertFloat4ToU32({color.x(), color.y(), color.z(), 1}));
        }
    }
}

void app::do_lightmap_test()
{
    if (!tested_light_chunk)
        return;

    auto& w = M->world();

    if (!w.at(*tested_light_chunk))
    {
        tested_light_chunk = {};
        return;
    }

    auto coord = *tested_light_chunk;
    auto [x, y, z] = coord;

    auto& shader = M->lightmap_shader();
    const auto ns = shader.iter_bounds();

    shader.begin_occlusion();

    for (int j = y - ns; j < y + ns; j++)
        for (int i = x - ns; i < x + ns; i++)
        {
            auto c = chunk_coords_{(int16_t)i, (int16_t)j, z};
            if (auto* chunk = w.at(c))
            {
                auto offset = Vector2(Vector2i(c.x, c.y) - Vector2i(x, y));
                shader.add_chunk(offset, *chunk);
            }
        }

    shader.end_occlusion();
    shader.bind();

    for (int j = y - ns; j < y + ns; j++)
        for (int i = x - ns; i < x + ns; i++)
        {
            auto c = chunk_coords_{(int16_t)i, (int16_t)j, z};
            if (auto* chunk = w.at(c))
            {
                auto offset = Vector2(Vector2i(c.x, c.y) - Vector2i(x, y));
                for (const auto& eʹ : chunk->objects())
                {
                    if (eʹ->type() == object_type::light)
                    {
                        const auto& li = static_cast<const light&>(*eʹ);
                        if (li.max_distance < 1e-6f)
                            continue;
                        light_s L {
                            .center = Vector2(li.coord.local()) * TILE_SIZE2 + Vector2(li.offset),
                            .dist = li.max_distance,
                            .radius = li.radius,
                            .color = li.color,
                            .falloff = li.falloff,
                        };
                        shader.add_light(offset, L);
                    }
                }
            }
        }

    shader.finish();
    M->bind();
}

void app::draw_lightmap_test(float main_menu_height)
{
    if (!tested_light_chunk)
        return;

    auto dpi = M->dpi_scale();
    auto win_size = M->window_size();
    auto window_pos = ImVec2((float)win_size.x() - 512, (main_menu_height + 1) * dpi.y());

    auto& shader = M->lightmap_shader();
    bool is_open = true;
    constexpr auto preview_size = ImVec2{512, 512};

    ImGui::SetNextWindowSize(preview_size, ImGuiCond_Appearing);

    auto b1 = push_style_var(ImGuiStyleVar_WindowPadding, {0, 0});
    constexpr auto flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollbar;

    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Appearing);

    if (ImGui::Begin("Lightmap", &is_open, flags))
    {
        ImGui::Image(shader.accum_texture().id(), preview_size, {0, 0}, {1, 1});
        ImGui::End();
    }
    else
        is_open = false;

    if (!is_open)
        tested_light_chunk = {};
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
    auto eʹ = w.find_object(id);

    if (target == popup_target_type::none || !eʹ)
    {
        _popup_target = {};
        _pending_popup = {};
        return;
    }

    auto& e = *eʹ;

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
        if (bool exists = check_inspector_exists(_popup_target);
            ImGui::MenuItem("Inspect", nullptr, exists))
        {
            if (!exists)
                add_inspector(std::exchange(_popup_target, {}));
            {
                char buf2[10], buf3[128], buf[sizeof buf2 + sizeof buf3 + 3 - 1];
                entity_inspector_name(buf2, e.id);
                entity_friendly_name(buf3, sizeof buf3, e);
                std::snprintf(buf, sizeof buf, "%s###%s", buf3, buf2);
                ImGui::SetWindowFocus(buf);
                ImGui::SetWindowCollapsed(buf, false);
            }
        }
        if (bool b_testing = tested_light_chunk == chunk_coords_(e.coord);
            e.type() == object_type::light)
            if (ImGui::MenuItem("Lightmap test", nullptr, b_testing))
                tested_light_chunk = e.coord.chunk3();
        ImGui::SeparatorText("Modify");
        if (auto next_rot = e.atlas->next_rotation_from(e.r);
            ImGui::MenuItem("Rotate", nullptr, false, next_rot != e.r && e.can_rotate(next_rot)))
            e.rotate(i, next_rot);
        if (ImGui::MenuItem("Delete", nullptr, false))
        {
            e.destroy_script_pre(eʹ, script_destroy_reason::kill);
            e.chunk().remove_object(e.index());
            e.destroy_script_post();
            eʹ.destroy();
        }
    }
    else
        _popup_target = {};
}

void app::kill_popups(bool hard)
{
    const bool imgui = _imgui->context() != nullptr;

    if (imgui)
        fm_assert(_imgui->context());

    _pending_popup = false;
    _popup_target = {};

    if (hard)
        tested_light_chunk = {};

    if (_imgui->context())
        ImGui::CloseCurrentPopup();

    if (hard)
        kill_inspectors();

    if (_imgui->context())
        ImGui::FocusWindow(nullptr);
}

} // namespace floormat
