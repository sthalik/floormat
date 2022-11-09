#include "editor.hpp"
#include "src/loader.hpp"
#include "tile-atlas.hpp"
#include "src/world.hpp"
#include "keys.hpp"

#include <Corrade/Containers/StringView.h>

#include <vector>
#include <algorithm>
#include <filesystem>

namespace floormat {

tile_editor* editor::current_tile_editor() noexcept
{
    return const_cast<tile_editor*>(static_cast<const editor&>(*this).current_tile_editor());
}

void editor::on_release()
{
    _last_pos = std::nullopt;
}

auto editor::get_snap_value(snap_mode snap, int mods) const -> snap_mode
{

    if (const auto* mode = current_tile_editor(); mode != nullptr)
        return mode->check_snap(mods);
    else if (snap != snap_mode::none)
        return snap;
    else
        return snap_mode::none;
}

global_coords editor::apply_snap(global_coords pos, global_coords last, snap_mode snap) noexcept
{
    switch (snap)
    {
    default:
        break;
    case snap_mode::horizontal:
        pos.y = last.y;
        break;
    case snap_mode::vertical:
        pos.x = last.x;
        break;
    }
    return pos;
}

void editor::on_mouse_move(world& world, global_coords& pos, int mods)
{
    if (auto* mode = current_tile_editor(); mode && _last_pos && _last_pos->btn != button::none)
    {
        auto& last = *_last_pos;
        const Vector2i offset = pos - last.coord;
        const snap_mode snap = get_snap_value(last.snap, mods);
        const global_coords draw_coord = apply_snap(last.draw_coord + offset, last.draw_coord, snap);
        if (draw_coord != last.draw_coord)
        {
            const auto draw_offset = draw_coord - last.draw_coord;
            if (!!draw_offset[0] ^ !!draw_offset[1] && std::abs(draw_offset.sum()) > 1)
            {
                const auto [minx, maxx] = std::minmax(draw_coord.x, last.draw_coord.x);
                const auto [miny, maxy] = std::minmax(draw_coord.y, last.draw_coord.y);
                if (draw_offset[0])
                    for (std::uint32_t i = minx; i <= maxx; i++)
                        on_click_(world, { i, draw_coord.y }, last.btn);
                else
                    for (std::uint32_t j = miny; j <= maxy; j++)
                        on_click_(world, { draw_coord.x, j }, last.btn);
            }
            else
                on_click_(world, draw_coord, last.btn);
            _last_pos = { pos, draw_coord, snap, last.btn };
        }
        pos = draw_coord;
    }
}

void editor::on_click_(world& world, global_coords pos, button b)
{
    if (auto* mode = current_tile_editor(); mode != nullptr)
        if (auto opt = mode->get_selected(); opt || b == button::remove)
        {
            switch (b)
            {
            case button::place: return mode->place_tile(world, pos, opt);
            case button::remove: return mode->place_tile(world, pos, {});
            default: break;
            }
        }
    on_release();
}

void editor::on_click(world& world, global_coords pos, int mods, button b)
{
    if (auto* mode = current_tile_editor(); mode != nullptr)
    {
        _last_pos = { pos, pos, mode->check_snap(mods), b };
        on_click_(world, pos, b);
    }
}

editor::editor()
{
}

void editor::set_mode(editor_mode mode)
{
    _mode = mode;
    on_release();
}

const tile_editor* editor::current_tile_editor() const noexcept
{
    switch (_mode)
    {
    case editor_mode::floor:
        return &_floor;
    case editor_mode::walls:
        return &_wall; // todo
    default:
        return nullptr;
    }
}

} // namespace floormat
