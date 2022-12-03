#include "app.hpp"
#include "floormat/main.hpp"
#include "floormat/settings.hpp"
#include "shaders/tile.hpp"
#include "main/clickable.hpp"
#include "src/anim-atlas.hpp"
#include "draw/anim.hpp"
#include "src/camera-offset.hpp"
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector3.h>

namespace floormat {

void app::draw_cursor()
{
    constexpr float LINE_WIDTH = 2;
    auto& shader = M->shader();
    shader.set_tint({1, 0, 0, 1});
    const auto inactive_color = 0xff00ffff_rgbaf;

    if (cursor.tile && !cursor.in_imgui)
    {
        const auto draw = [&, pos = *cursor.tile](auto& mesh, const auto& size) {
            const auto pt = pos.to_signed();
            const Vector3 center{Vector3i(pt[0], pt[1], 0) * iTILE_SIZE};
            mesh.draw(shader, {center, size, LINE_WIDTH});
        };

        if (const auto* ed = _editor.current_tile_editor(); ed)
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
        else if (const auto* ed = _editor.current_scenery_editor(); ed)
        {
            if (!ed->is_anything_selected())
                shader.set_tint(inactive_color);
            const auto& sel = ed->get_selected().proto;
            draw(_wireframe_quad, TILE_SIZE2);
            if (ed->is_anything_selected())
            {
                shader.set_tint({1, 1, 1, 0.75f});
                auto [_f, _w, anim_mesh] = M->meshes();
                const auto pos = Vector3i(cursor.tile->to_signed(), 0)*iTILE_SIZE;
                anim_mesh.draw(shader, *sel.atlas, sel.frame.r, sel.frame.frame, Vector3(pos), 1);
            }
        }
    }
}

void app::draw()
{
    if (_editor.current_tile_editor() || _editor.current_scenery_editor())
        draw_cursor();
    draw_ui();
    render_menu();
}

clickable_scenery* app::find_clickable_scenery(const Optional<Vector2i>& pixel_)
{
    if (!pixel_ || _editor.mode() != editor_mode::none)
        return nullptr;

    const auto pixel = Vector2ui(*pixel_);
    clickable_scenery* item = nullptr;
    float depth = -1;

    const auto array = M->clickable_scenery();
    for (clickable_scenery& c : array)
        if (c.depth > depth && c.dest.contains(pixel))
        {
            const auto pos_ = pixel - c.dest.min() + c.src.min();
            const auto pos = c.atlas.group(c.item.r).mirror_from.isEmpty()
                             ? pos_
                             : Vector2ui(c.src.sizeX() - 1 - pos_[0], pos_[1]);
            const auto stride = c.atlas.info().pixel_size[0];
            std::size_t idx = pos.y() * stride + pos.x();
            fm_debug_assert(idx < c.bitmask.size());
            if (c.bitmask[idx])
            {
                depth = c.depth;
                item = &c;
            }
        }
    if (item && item->item.can_activate(item->atlas))
        return item;
    else
        return nullptr;
}

} // namespace floormat
