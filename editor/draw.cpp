#include "app.hpp"
#include "floormat/main.hpp"
#include "floormat/settings.hpp"
#include "shaders/shader.hpp"
#include "main/clickable.hpp"
#include "src/anim-atlas.hpp"
#include "draw/anim.hpp"
#include "src/camera-offset.hpp"
#include "src/world.hpp"
#include "character.hpp"
#include "rotation.inl"
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/GL/Renderer.h>
#include "src/RTree-search.hpp"

namespace floormat {

void app::draw_cursor()
{
    constexpr float LINE_WIDTH = 2;
    auto& shader = M->shader();
    const auto inactive_color = 0xff00ffff_rgbaf;

    if (cursor.tile && !cursor.in_imgui)
    {
        const auto draw = [&, pos = *cursor.tile](auto& mesh, const auto& size) {
            const auto center = Vector3(pos.to_signed3() * iTILE_SIZE);
            mesh.draw(shader, {center, size, LINE_WIDTH});
        };

        shader.set_tint({1, 0, 0, 1});

        if (const auto* ed = _editor.current_tile_editor())
        {
            if (!ed->is_anything_selected())
                shader.set_tint(inactive_color);
            if (ed->mode() == editor_mode::walls)
                switch (ed->rotation())
                {
                case editor_wall_rotation::N: draw(_wireframe_wall_n, TILE_SIZE); break;
                case editor_wall_rotation::W: draw(_wireframe_wall_w, TILE_SIZE); break;
                }
            else if (ed->mode() == editor_mode::floor)
                draw(_wireframe_quad, TILE_SIZE2);
        }
        else if (const auto* ed = _editor.current_scenery_editor())
        {
            if (!ed->is_anything_selected())
                shader.set_tint(inactive_color);
            const auto& sel = ed->get_selected().proto;
            draw(_wireframe_quad, TILE_SIZE2);
            if (ed->is_anything_selected())
            {
                shader.set_tint({1, 1, 1, 0.75f});
                auto [_f, _w, anim_mesh] = M->meshes();
                const auto offset = Vector3i(Vector2i(sel.offset), 0);
                const auto pos = cursor.tile->to_signed3()*iTILE_SIZE + offset;
                anim_mesh.draw(shader, *sel.atlas, sel.r, sel.frame, Vector3(pos), 1);
            }
        }
        else if (const auto* ed = _editor.current_vobj_editor())
        {
            if (!ed->is_anything_selected())
                shader.set_tint(inactive_color);
            if (ed->is_anything_selected())
            {
                const auto& atlas = ed->get_selected()->factory->atlas();
                draw(_wireframe_quad, TILE_SIZE2);
                shader.set_tint({1, 1, 1, 0.75f});
                auto [_f, _w, anim_mesh] = M->meshes();
                const auto pos = cursor.tile->to_signed3()*iTILE_SIZE;
                anim_mesh.draw(shader, *atlas, rotation::N, 0, Vector3(pos), 1);
            }
        }

        shader.set_tint({1, 1, 1, 1});
    }
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
                auto* c_ = world.at(pos);
                if (!c_)
                    continue;
                auto& c = *c_;
                c.ensure_passability();
                const with_shifted_camera_offset o{shader, pos, {minx, miny}, {maxx, maxy}};
                if (floormat_main::check_chunk_visible(shader.camera_offset(), sz))
                {
                    constexpr float maxf = 1 << 24, max2f[] = { maxf, maxf }, min2f[] = { -maxf, -maxf };
                    const auto* rtree = c.rtree();
                    rtree->Search(min2f, max2f, [&](object_id data, const rect_type& rect) {
                        [[maybe_unused]] auto x = std::bit_cast<collision_data>(data);
                        if (x.tag == (uint64_t)collision_type::geometry)
                            return true;
                        Vector2 start(rect.m_min[0], rect.m_min[1]), end(rect.m_max[0], rect.m_max[1]);
                        auto size = (end - start);
                        auto center = Vector3(start + size*.5f, 0.f);
                        shader.set_tint(x.pass == (uint64_t)pass_mode::pass ? pass_tint : tint);
                        _wireframe_rect.draw(shader, { center, size, 3 });
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
        auto coord = M->pixel_to_tile(pixel);
        auto tile = global_coords{coord.chunk(), coord.local(), 0};

        constexpr auto eps = 1e-6f;
        constexpr auto m = TILE_SIZE2 * Vector2(1- eps, 1- eps);
        const auto tile_ = Vector2(M->pixel_to_tile_(Vector2d(pixel)));
        const auto curchunk = Vector2(tile.chunk()), curtile = Vector2(tile.local());
        const auto subpixel_ = Vector2(std::fmod(tile_[0], 1.f), std::fmod(tile_[1], 1.f));
        const auto subpixel = m * Vector2(curchunk[0] < 0 ? 1 + subpixel_[0] : subpixel_[0],
                                          curchunk[1] < 0 ? 1 + subpixel_[1] : subpixel_[1]);
        for (int16_t y = miny; y <= maxy; y++)
            for (int16_t x = minx; x <= maxx; x++)
            {
                const chunk_coords_ c_pos{x, y, _z_level};
                auto* c_ = world.at(c_pos);
                if (!c_)
                    continue;
                auto& c = *c_;
                c.ensure_passability();
                const with_shifted_camera_offset o{shader, c_pos, {minx, miny}, {maxx, maxy}};
                if (floormat_main::check_chunk_visible(shader.camera_offset(), sz))
                {
                    constexpr auto half_tile = TILE_SIZE2/2;
                    constexpr auto chunk_size = TILE_SIZE2 * TILE_MAX_DIM;
                    auto chunk_dist = (curchunk - Vector2(c_pos.x, c_pos.y))*chunk_size;
                    auto t0 = chunk_dist + curtile*TILE_SIZE2 + subpixel - half_tile;
                    auto t1 = t0+Vector2(1e-4f);
                    const auto* rtree = c.rtree();
                    rtree->Search(t0.data(), t1.data(), [&](uint64_t data, const rect_type& rect) {
                        [[maybe_unused]] auto x = std::bit_cast<collision_data>(data);
                        if (x.tag == (uint64_t)collision_type::geometry)
                            return true;
                        Vector2 start(rect.m_min[0], rect.m_min[1]), end(rect.m_max[0], rect.m_max[1]);
                        auto size = end - start;
                        auto center = Vector3(start + size*.5f, 0.f);
                        _wireframe_rect.draw(shader, { center, size, 3 });
                        return true;
                    });
                }
            }
    }

    shader.set_tint({1, 1, 1, 1});
}

void app::draw()
{
    if (_render_bboxes)
        draw_collision_boxes();
    if (_editor.current_tile_editor() ||
        _editor.current_scenery_editor() && _editor.current_scenery_editor()->is_anything_selected() ||
        _editor.current_vobj_editor())
        draw_cursor();
    draw_ui();
    render_menu();
}

clickable* app::find_clickable_scenery(const Optional<Vector2i>& pixel)
{
    if (!pixel || _editor.mode() != editor_mode::none)
        return nullptr;

    clickable* item = nullptr;
    float depth = -(1 << 24);

    const auto array = M->clickable_scenery();
    const auto p = *pixel;
    for (clickable& c : array)
        if (c.depth > depth && c.dest.contains(p))
        {
            const auto pos_ = *pixel - c.dest.min() + Vector2i(c.src.min());
            const auto pos = !c.mirrored ? pos_ : Vector2i(int(c.src.sizeX()) - 1 - pos_[0], pos_[1]);
            size_t idx = unsigned(pos.y()) * c.stride + unsigned(pos.x());
            fm_assert(idx < c.bitmask.size());
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
