#include "app.hpp"
#include "floormat/main.hpp"
#include "floormat/settings.hpp"
#include "shaders/tile.hpp"
#include "main/clickable.hpp"
#include "src/anim-atlas.hpp"
#include "draw/anim.hpp"
#include "src/camera-offset.hpp"
#include "src/world.hpp"
#include "character.hpp"
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/GL/Renderer.h>
#include "src/RTree.hpp"

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
                const auto pos = cursor.tile->to_signed3()*iTILE_SIZE;
                anim_mesh.draw(shader, *sel.atlas, sel.r, sel.frame, Vector3(pos), 1);
            }
        }

        shader.set_tint({1, 1, 1, 1});
    }
}

void app::draw_collision_boxes()
{
    const auto [minx, maxx, miny, maxy] = M->get_draw_bounds();
    const auto sz = M->window_size();
    auto& world = M->world();
    auto& shader = M->shader();

    shader.set_tint({0, .5f, 1, 1});

    using rtree_type = std::decay_t<decltype(*world[chunk_coords{}].rtree())>;
    using rect_type = typename rtree_type::Rect;

    for (std::int16_t y = miny; y <= maxy; y++)
        for (std::int16_t x = minx; x <= maxx; x++)
        {
            const chunk_coords pos{x, y};
            auto& c = world[pos];
            if (c.empty())
                continue;
            c.ensure_passability();
            const with_shifted_camera_offset o{shader, pos, {minx, miny}, {maxx, maxy}};
            if (floormat_main::check_chunk_visible(shader.camera_offset(), sz))
            {
                constexpr float maxf = 1 << 24, max2f[] = { maxf, maxf }, min2f[] = { -maxf, -maxf };
                const auto* rtree = c.rtree();
                rtree->Search(min2f, max2f, [&](std::uint64_t data, const rect_type& rect) {
                    [[maybe_unused]] auto x = std::bit_cast<collision_data>(data);
                    Vector2 start(rect.m_min[0], rect.m_min[1]), end(rect.m_max[0], rect.m_max[1]);
                    auto size = (end - start);
                    auto center = Vector3(start + size*.5f, 0.f);
                    _wireframe_rect.draw(shader, { center, size, 3 });
                    return true;
                });
            }
        }

    shader.set_tint({1, 0, 1, 1});

    if (cursor.tile)
    {
        constexpr auto eps = 1e-6f;
        constexpr auto m = TILE_SIZE2 * Vector2(1- eps, 1- eps);
        const auto tile_ = Vector2(M->pixel_to_tile_(Vector2d(*cursor.pixel)));
        const auto tile = *cursor.tile;
        const auto curchunk = Vector2(tile.chunk()), curtile = Vector2(tile.local());
        const auto subpixel_ = Vector2(std::fmod(tile_[0], 1.f), std::fmod(tile_[1], 1.f));
        const auto subpixel = m * Vector2(curchunk[0] < 0 ? 1 + subpixel_[0] : subpixel_[0],
                                          curchunk[1] < 0 ? 1 + subpixel_[1] : subpixel_[1]);
        for (std::int16_t y = miny; y <= maxy; y++)
            for (std::int16_t x = minx; x <= maxx; x++)
            {
                const chunk_coords c_pos{x, y};
                auto& c = world[c_pos];
                if (c.empty())
                    continue;
                c.ensure_passability();
                const with_shifted_camera_offset o{shader, c_pos, {minx, miny}, {maxx, maxy}};
                if (floormat_main::check_chunk_visible(shader.camera_offset(), sz))
                {
                    constexpr auto half_tile = TILE_SIZE2/2;
                    constexpr auto chunk_size = TILE_SIZE2 * TILE_MAX_DIM;
                    auto chunk_dist = (curchunk - Vector2(c_pos))*chunk_size;
                    auto t0 = chunk_dist + curtile*TILE_SIZE2 + subpixel - half_tile;
                    auto t1 = t0+Vector2(1e-4f);
                    const auto* rtree = c.rtree();
                    rtree->Search(t0.data(), t1.data(), [&](std::uint64_t data, const rect_type& rect) {
                        [[maybe_unused]] auto x = std::bit_cast<collision_data>(data);
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
    //draw_character();
    if (_render_bboxes)
        draw_collision_boxes();
    if (_editor.current_tile_editor() || _editor.current_scenery_editor())
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
            std::size_t idx = unsigned(pos.y()) * c.stride + unsigned(pos.x());
            fm_assert(idx < c.bitmask.size());
            if (c.bitmask[idx])
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
