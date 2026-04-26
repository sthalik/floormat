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
#include "imgui-text.hpp"
#include "src/light.hpp"
#include "loader/loader.hpp"
#include "src/point.inl"
#include <mg/Renderer.h>
#include <mg/ImGuiIntegration/Context.h>
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
        if (auto b = begin_menu("Tests"))
        {
            if (ImGui::MenuItem("Text painter test", nullptr, _test_text_painter))
                _test_text_painter = !_test_text_painter;
            if (ImGui::MenuItem("Wall + hole scene with UV bug"))
                populate_dev_test_world_2();
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
    if (_test_text_painter)
        draw_text_painter_test();
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
    const float pad_x = style.FramePadding.x * .5f;

    const auto yellow = ImGui::ColorConvertFloat4ToU32({1, 1, 0, 1});
    const auto bg_col = ImGui::ColorConvertFloat4ToU32({0, 0, 0, 1});
    const float font_size = ImGui::GetCurrentContext()->FontSize;

    for (const auto& x : M->clickable_scenery())
    {
        if (x.e->type() != object_type::light)
            continue;
        const auto& e = static_cast<const light&>(*x.e);
        const auto dest = Math::Range2D<float>(x.dest);

        StringView falloff;
        switch (e.falloff)
        {
        default: falloff = "?"_s; break;
        case light_falloff::constant: falloff = "constant"_s; break;
        case light_falloff::linear: falloff = "linear"_s; break;
        case light_falloff::quadratic: falloff = "quadratic"_s; break;
        }

        constexpr auto inv_255 = 1.f/255;
        const auto col = Vector4(e.color)*inv_255 * float(e.color.a())*inv_255;
        const auto swatch_col = ImGui::ColorConvertFloat4ToU32({col.x(), col.y(), col.z(), 1});

        auto p = _text_pool->make(font_size);
        p.with_background(bg_col, {pad_x, 0});
        p.rect(swatch_col, 5*dpi.x(), 3*dpi.y());
        p.gap(pad_x);
        p.text(yellow, falloff);
        if (e.max_distance > 0)
            p.text(yellow, " "_s).format(yellow, "range={}", e.max_distance);
        if (e.radius > 0)
            p.text(yellow, " "_s).format(yellow, "radius={}", e.radius);

        const auto sz = p.size();
        const float offx = (dest.min().x() + dest.max().x())*.5f - sz.x()*.5f;
        const float offy = dest.max().y() + 5 * dpi.y();

        p.render(draw, {offx, offy});
    }
}

void app::draw_text_painter_test()
{
    ImDrawList& draw = *ImGui::GetForegroundDrawList();
    const auto dpi = M->dpi_scale();
    const auto& style = ImGui::GetStyle();
    const float pad_x = style.FramePadding.x * .5f;
    const float font_size = ImGui::GetCurrentContext()->FontSize;
    const float line_h = font_size + 6 * dpi.y();

    const auto white   = ImGui::ColorConvertFloat4ToU32({ 1.0f, 1.0f, 1.0f, 1.0f });
    const auto gray    = ImGui::ColorConvertFloat4ToU32({ 0.55f, 0.55f, 0.55f, 1.0f });
    const auto yellow  = ImGui::ColorConvertFloat4ToU32({ 1.0f, 1.0f, 0.2f, 1.0f });
    const auto red     = ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.3f, 0.3f, 1.0f });
    const auto green   = ImGui::ColorConvertFloat4ToU32({ 0.3f, 1.0f, 0.3f, 1.0f });
    const auto blue    = ImGui::ColorConvertFloat4ToU32({ 0.4f, 0.6f, 1.0f, 1.0f });
    const auto cyan    = ImGui::ColorConvertFloat4ToU32({ 0.3f, 1.0f, 1.0f, 1.0f });
    const auto magenta = ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.3f, 1.0f, 1.0f });
    const auto black   = ImGui::ColorConvertFloat4ToU32({ 0.0f, 0.0f, 0.0f, 1.0f });
    const auto darkbg  = ImGui::ColorConvertFloat4ToU32({ 0.10f, 0.10f, 0.20f, 0.85f });
    const auto frame_c = ImGui::ColorConvertFloat4ToU32({ 0.0f, 0.7f, 0.7f, 1.0f });

    Vector2 cursor{ 60 * dpi.x(), 70 * dpi.y() };

    {
        auto p = _text_pool->make(font_size);
        p.text(yellow,  "[1] "_s);
        p.text(white,   "multi-color text: "_s);
        p.text(red,     "red "_s);
        p.text(green,   "green "_s);
        p.text(blue,    "blue "_s);
        p.text(cyan,    "cyan "_s);
        p.text(magenta, "magenta"_s);
        p.render(draw, cursor);
        cursor.y() += line_h;
    }

    {
        auto p = _text_pool->make(font_size);
        p.text(yellow, "[2] "_s);
        p.text(white,  "rect, full height: "_s);
        p.rect(red,     14*dpi.x(), 0);
        p.gap(3*dpi.x());
        p.rect(green,   14*dpi.x(), 0);
        p.gap(3*dpi.x());
        p.rect(blue,    14*dpi.x(), 0);
        p.gap(3*dpi.x());
        p.rect(magenta, 14*dpi.x(), 0);
        p.render(draw, cursor);
        cursor.y() += line_h;
    }

    {
        auto p = _text_pool->make(font_size);
        p.text(yellow, "[3] "_s);
        p.text(white,  "rect, vert_pad=3*dpi: "_s);
        p.rect(red,   14*dpi.x(), 3*dpi.y());
        p.gap(3*dpi.x());
        p.rect(green, 14*dpi.x(), 3*dpi.y());
        p.gap(3*dpi.x());
        p.rect(blue,  14*dpi.x(), 3*dpi.y());
        p.render(draw, cursor);
        cursor.y() += line_h;
    }

    {
        auto p = _text_pool->make(font_size);
        p.text(yellow, "[4] "_s);
        p.text(white,  "gap + bounds_of: "_s);
        const auto x_slot = p.gap(20*dpi.x());
        p.gap(3*dpi.x());
        p.text(white,  "(slot rendered by caller)"_s);
        p.render(draw, cursor);
        const auto r = p.bounds_of(x_slot);
        draw.AddLine({r.min().x(), r.min().y()}, {r.max().x(), r.max().y()}, magenta, 1.5f);
        draw.AddLine({r.min().x(), r.max().y()}, {r.max().x(), r.min().y()}, magenta, 1.5f);
        draw.AddRect({r.min().x(), r.min().y()}, {r.max().x(), r.max().y()}, magenta, 0, 0, 1);
        cursor.y() += line_h;
    }

    {
        auto p = _text_pool->make(font_size);
        p.text(yellow, "[5] "_s);
        p.text(white,  "three slots: "_s);
        const auto s_circle = p.gap(20*dpi.x());
        p.gap(3*dpi.x());
        const auto s_tri    = p.gap(20*dpi.x());
        p.gap(3*dpi.x());
        const auto s_sqr    = p.gap(20*dpi.x());
        p.text(gray,   "  (circle, triangle, square)"_s);
        p.render(draw, cursor);
        const auto rc = p.bounds_of(s_circle);
        const auto cc = (rc.min() + rc.max()) * 0.5f;
        draw.AddCircleFilled({cc.x(), cc.y()}, 7*dpi.x(), red);
        const auto rt = p.bounds_of(s_tri);
        const auto ct = (rt.min() + rt.max()) * 0.5f;
        const float t = 7*dpi.x();
        draw.AddTriangleFilled({ct.x(), ct.y() - t},
                               {ct.x() - t, ct.y() + t},
                               {ct.x() + t, ct.y() + t}, green);
        const auto rs = p.bounds_of(s_sqr);
        const auto cs = (rs.min() + rs.max()) * 0.5f;
        draw.AddRectFilled({cs.x() - 7*dpi.x(), cs.y() - 7*dpi.y()},
                           {cs.x() + 7*dpi.x(), cs.y() + 7*dpi.y()}, blue);
        cursor.y() += line_h;
    }

    {
        auto p = _text_pool->make(font_size);
        p.with_background(darkbg, {pad_x, 1*dpi.y()});
        p.text(yellow, "[6] "_s);
        p.text(white,  "with_background, small pad"_s);
        p.render(draw, cursor);
        cursor.y() += line_h;
    }

    {
        auto p = _text_pool->make(font_size);
        p.with_background(darkbg, {8*dpi.x(), 4*dpi.y()});
        p.text(yellow, "[7] "_s);
        p.text(white,  "with_background, large pad (8, 4)"_s);
        p.render(draw, cursor);
        cursor.y() += line_h * 1.7f;
    }

    {
        auto p = _text_pool->make(font_size);
        p.text(yellow, "[8] "_s);
        p.text(white,  "format(): "_s);
        p.format(cyan, "ts={}, fps={:.1f}, dpi=({:.2f},{:.2f})",
                 _timestamp, M->smoothed_fps(), dpi.x(), dpi.y());
        p.render(draw, cursor);
        cursor.y() += line_h;
    }

    {
        auto p = _text_pool->make(font_size);
        p.text(yellow, "[9] "_s);
        p.text(gray,   "frame="_s);
        p.format(green, "{}", _timestamp);
        p.text(gray,   "  fps="_s);
        p.format(green, "{:.1f}", M->smoothed_fps());
        p.text(gray,   "  cycle="_s);
        p.format(green, "{}", _timestamp % 60);
        p.text(gray,   "  hex="_s);
        p.format(magenta, "{:x}", _timestamp);
        p.render(draw, cursor);
        cursor.y() += line_h;
    }

    {
        auto p = _text_pool->make(font_size);
        p.with_background(black, {pad_x, 2*dpi.y()});
        p.text(yellow, "[10] "_s);
        p.text(white,  "bounds() draws cyan frame around me"_s);
        p.render(draw, cursor);
        const auto bb = p.bounds();
        draw.AddRect({bb.min().x(), bb.min().y()}, {bb.max().x(), bb.max().y()},
                     frame_c, 0, 0, 2);
        cursor.y() += line_h * 1.4f;
    }

    {
        Vector2 sz, tsz;
        {
            auto p = _text_pool->make(font_size);
            p.with_background(black, {6*dpi.x(), 3*dpi.y()});
            p.rect(red,   12*dpi.x(), 2*dpi.y());
            p.gap(pad_x);
            p.text(white, "[11] some content"_s);
            p.render(draw, cursor);
            sz = p.size();
            tsz = p.total_size();
        }
        cursor.y() += line_h * 1.4f;
        auto p2 = _text_pool->make(font_size);
        p2.text(gray,   "      "_s);
        p2.format(white, "size()=({:.0f}, {:.0f})  total_size()=({:.0f}, {:.0f})",
                  sz.x(), sz.y(), tsz.x(), tsz.y());
        p2.render(draw, cursor);
        cursor.y() += line_h;
    }

    {
        auto p = _text_pool->make(font_size);
        p.with_background(darkbg, {pad_x, 2*dpi.y()});
        p.text(yellow, "[12] "_s);
        p.rect(red,   10*dpi.x(), 3*dpi.y());
        p.gap(pad_x);
        p.text(white, "combined: "_s);
        const auto dot_slot = p.gap(8*dpi.x());
        p.gap(pad_x);
        p.format(green, "(frame={})", _timestamp);
        p.render(draw, cursor);
        const auto rd = p.bounds_of(dot_slot);
        const auto cd = (rd.min() + rd.max()) * 0.5f;
        draw.AddCircleFilled({cd.x(), cd.y()}, 3*dpi.x(), magenta);
        cursor.y() += line_h * 1.4f;
    }

    {
        auto p = _text_pool->make(font_size);
        p.with_background(darkbg, {pad_x, 4*dpi.y()});
        p.text(yellow, "[13] "_s);
        p.text(white,  "filled diamond drawn ON TOP of bg, inside gap: "_s);
        const auto diamond_slot = p.gap(32*dpi.x());
        p.gap(pad_x);
        p.text(gray,   "(magenta diamond, white outline)"_s);
        p.render(draw, cursor);
        const auto rd = p.bounds_of(diamond_slot);
        const auto cd = (rd.min() + rd.max()) * 0.5f;
        const float s = 9*dpi.x();
        draw.AddQuadFilled({cd.x(),     cd.y() - s}, {cd.x() + s, cd.y()    },
                           {cd.x(),     cd.y() + s}, {cd.x() - s, cd.y()    }, magenta);
        draw.AddQuad      ({cd.x(),     cd.y() - s}, {cd.x() + s, cd.y()    },
                           {cd.x(),     cd.y() + s}, {cd.x() - s, cd.y()    }, white, 1.5f);
        cursor.y() += line_h * 1.4f;
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
