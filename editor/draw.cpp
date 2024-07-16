#include "app.hpp"
#include "compat/borrowed-ptr.inl"
#include "src/tile-constants.hpp"
#include "floormat/main.hpp"
#include "floormat/draw-bounds.hpp"
#include "shaders/shader.hpp"
#include "shaders/texture-unit-cache.hpp"
#include "main/clickable.hpp"
#include "editor.hpp"
#include "ground-editor.hpp"
#include "wall-editor.hpp"
#include "scenery-editor.hpp"
#include "vobj-editor.hpp"
#include "src/anim-atlas.hpp"
#include "draw/anim.hpp"
#include "draw/wireframe-meshes.hpp"
#include "src/camera-offset.hpp"
#include "src/world.hpp"
#include "src/critter.hpp"
#include "src/RTree-search.hpp"
#include <bit>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector3.h>

namespace floormat {

void app::draw_cursor()
{
    constexpr float LINE_WIDTH = 2;
    auto& shader = M->shader();
    auto& w = M->world();
    const auto inactive_color = 0xff00ffff_rgbaf;

    global_coords tile;
    if (auto pos = _editor->mouse_drag_pos())
        tile = *pos;
    else if (cursor.tile)
        tile = *cursor.tile;
    else
        return;

    shader.set_tint({1, 0, 0, 1});

    if (!cursor.in_imgui)
    {
        const auto draw = [&, pos = tile](auto& mesh, const auto& size) {
            const auto center = Vector3(pos) * TILE_SIZE;
            mesh.draw(shader, {center, size, LINE_WIDTH});
        };

        if (const auto* ed = _editor->current_ground_editor())
        {
            if (!ed->is_anything_selected())
                shader.set_tint(inactive_color);
            draw(_wireframe->quad, TILE_SIZE2);
        }
        else if (const auto* ed = _editor->current_wall_editor())
        {
            if (!ed->is_anything_selected())
                shader.set_tint(inactive_color);
            switch (ed->rotation())
            {
            case rotation::N: draw(_wireframe->wall_n, TILE_SIZE); break;
            case rotation::W: draw(_wireframe->wall_w, TILE_SIZE); break;
            default: fm_assert(false);
            }
        }
        else if (const auto* ed = _editor->current_scenery_editor())
        {
            if (!ed->is_anything_selected())
                shader.set_tint(inactive_color);
            const auto& sel = ed->get_selected().proto;
            draw(_wireframe->quad, TILE_SIZE2);
            if (ed->is_anything_selected())
            {
                shader.set_tint({1, 1, 1, 0.75f});
                auto [_g, _w, anim_mesh] = M->meshes();
                const auto offset = Vector3i(Vector2i(sel.offset), 0);
                const auto pos = Vector3i(tile)*iTILE_SIZE + offset;
                auto [ch, t] = w[tile];
                if (!ch.can_place_object(sel, tile.local()))
                    shader.set_tint({1, 0, 1, 0.5f});
                anim_mesh.draw(shader, *sel.atlas, sel.r, sel.frame, Vector3(pos), 1);
            }
        }
        else if (const auto* vo = _editor->current_vobj_editor())
        {
            if (!vo->is_anything_selected())
                shader.set_tint(inactive_color);
            if (vo->is_anything_selected())
            {
                const auto& atlas = vo->get_selected()->factory->atlas();
                draw(_wireframe->quad, TILE_SIZE2);
                shader.set_tint({1, 1, 1, 0.75f});
                auto [_g, _w, anim_mesh] = M->meshes();
                const auto pos = Vector3i(tile)*iTILE_SIZE;
                anim_mesh.draw(shader, *atlas, rotation::N, 0, Vector3(pos), 1);
            }
        }
    }

    shader.set_tint({1, 1, 1, 1});
}

void app::draw_collision_boxes()
{
    auto [z_min, z_max, z_cur, only] = get_z_bounds();
    if (only)
        z_min = z_max = z_cur;
    const auto [minx, maxx, miny, maxy] = M->get_draw_bounds();
    const auto sz = M->window_size();
    auto& world = M->world();
    auto& shader = M->shader();

    using rtree_type = std::decay_t<decltype(*world[chunk_coords_{}].rtree())>;
    using rect_type = typename rtree_type::Rect;

    for (int8_t z = z_min; z <= z_max; z++)
    {
        constexpr Vector4 pass_tint = {.7f, .7f, .7f, .6f};
        const auto tint = z == _z_level ? Vector4{0, .5f, 1, 1} : Vector4{.7f, .7f, .7f, .6f};

        for (int16_t y = miny; y <= maxy; y++)
            for (int16_t x = minx; x <= maxx; x++)
            {
                const chunk_coords_ pos{x, y, z};
                auto* cʹ = world.at(pos);
                if (!cʹ)
                    continue;
                auto& c = *cʹ;
                c.ensure_passability();
                const with_shifted_camera_offset o{shader, pos, {minx, miny}, {maxx, maxy}};
                if (floormat_main::check_chunk_visible(shader.camera_offset(), sz))
                {
                    constexpr float maxf = 1 << 24, max2f[] = { maxf, maxf }, min2f[] = { -maxf, -maxf };
                    const auto& rtree = *c.rtree();
                    rtree.Search(min2f, max2f, [&](object_id data, const rect_type& rect) {
                        [[maybe_unused]] auto x = std::bit_cast<collision_data>(data);
#if 0
                        if (x.tag == (uint64_t)collision_type::geometry)
                            return true;
#endif
                        if (x.tag == (uint64_t)collision_type::geometry)
                            if (x.pass == (uint64_t)pass_mode::pass)
                                if (x.data < TILE_COUNT*2+1)
                                    return true;
                        Vector2 start(rect.m_min[0], rect.m_min[1]), end(rect.m_max[0], rect.m_max[1]);
                        auto size = (end - start);
                        auto center = Vector3(start + size*.5f, 0.f);
                        shader.set_tint(x.pass == (uint64_t)pass_mode::pass ? pass_tint : tint);
                        _wireframe->rect.draw(shader, { center, size, 3 });
                        return true;
                    });
                }
            }
    }

    shader.set_tint({1, 0, 1, 1});

    if (cursor.pixel)
    {
        auto pos = tile_shader::project(Vector3d{0., 0., -_z_level*dTILE_SIZE[2]});
        auto pixel = Vector2d{*cursor.pixel} + pos;
        const auto [coord, subpixelʹ] = M->pixel_to_point(Vector2d(pixel));
        const auto curchunk = Vector2(coord.chunk()), curtile = Vector2(coord.local());
        const auto subpixel = Vector2(subpixelʹ);
        for (int16_t y = miny; y <= maxy; y++)
            for (int16_t x = minx; x <= maxx; x++)
            {
                const chunk_coords_ c_pos{x, y, _z_level};
                auto* cʹ = world.at(c_pos);
                if (!cʹ)
                    continue;
                auto& c = *cʹ;
                c.ensure_passability();
                const with_shifted_camera_offset o{shader, c_pos, {minx, miny}, {maxx, maxy}};
                if (floormat_main::check_chunk_visible(shader.camera_offset(), sz))
                {
                    constexpr auto chunk_size = TILE_SIZE2 * TILE_MAX_DIM;
                    auto chunk_dist = (curchunk - Vector2(c_pos.x, c_pos.y))*chunk_size;
                    auto t0 = chunk_dist + curtile*TILE_SIZE2 + subpixel;
                    auto t1 = t0+Vector2(1e-4f);
                    const auto* rtree = c.rtree();
                    rtree->Search(t0.data(), t1.data(), [&](uint64_t data, const rect_type& rect) {
                        [[maybe_unused]] auto x = std::bit_cast<collision_data>(data);
#if 0
                        if (x.tag == (uint64_t)collision_type::geometry)
                            return true;
#endif
                        if (x.tag == (uint64_t)collision_type::geometry)
                            if (x.pass == (uint64_t)pass_mode::pass)
                                if (x.data < TILE_COUNT*2+1)
                                    return true;
                        Vector2 start(rect.m_min[0], rect.m_min[1]), end(rect.m_max[0], rect.m_max[1]);
                        auto size = end - start;
                        auto center = Vector3(start + size*.5f, 0.f);
                        _wireframe->rect.draw(shader, { center, size, 3 });
                        return true;
                    });
                }
            }
    }

    shader.set_tint({1, 1, 1, 1});
}

void app::draw()
{
    do_lightmap_test();
    if (_render_bboxes)
        draw_collision_boxes();
    if (_editor->current_ground_editor() || _editor->current_wall_editor() ||
        _editor->current_scenery_editor() && _editor->current_scenery_editor()->is_anything_selected() ||
        _editor->current_vobj_editor() && _editor->current_vobj_editor()->is_anything_selected())
        draw_cursor();
    draw_ui();
    render_menu();

    M->texture_unit_cache().output_stats();
}

clickable* app::find_clickable_scenery(const Optional<Vector2i>& pixel)
{
    if (!pixel || _editor->mode() != editor_mode::none)
        return nullptr;

    clickable* item = nullptr;
    float depth = -(1 << 24);

    const auto array = M->clickable_scenery();
    const auto p = *pixel;
    for (clickable& c : array)
        if (c.depth > depth && c.dest.contains(p))
        {
            const auto posʹ = *pixel - c.dest.min() + Vector2i(c.src.min());
            const auto pos = !c.mirrored ? posʹ : Vector2i(int(c.src.sizeX()) - 1 - posʹ[0], posʹ[1]);
            size_t idx = unsigned(pos.y()) * c.stride + unsigned(pos.x());
            fm_assert(c.bitmask.isEmpty() || idx < c.bitmask.size());
            if (c.bitmask.isEmpty() || c.bitmask[idx])
            {
                depth = c.depth;
                item = &c;
            }
        }
    if (item)
        return item;
    else
        return nullptr;
}

} // namespace floormat
