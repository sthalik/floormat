#include "editor.hpp"
#include "loader/loader.hpp"
#include "src/world.hpp"
#include "keys.hpp"

#include <algorithm>
#include <Corrade/Containers/StringView.h>

namespace floormat {

void editor::on_release()
{
    _last_pos = NullOpt;
}

void editor::clear_selection()
{
    if (auto* ed = current_ground_editor())
        ed->clear_selection();
    else if (auto* wa = current_wall_editor())
        wa->clear_selection();
    else if (auto* ed = current_scenery_editor())
        ed->clear_selection();
    else if (auto* vo = current_vobj_editor())
        vo->clear_selection();
}

auto editor::get_snap_value(snap_mode snap, int mods) const -> snap_mode
{

    if (const auto* mode = current_ground_editor())
        return mode->check_snap(mods);
    else if (const auto* mode = current_wall_editor())
        return mode->check_snap(mods);
    else if (snap != snap_mode::none)
        return snap;
    else
        return snap_mode::none;
}

global_coords editor::apply_snap(global_coords pos, global_coords last, snap_mode snap) noexcept
{
    auto rpos = pos.raw();
    switch (snap)
    {
    default:
        break;
    case snap_mode::horizontal:
        rpos.y = last.raw().y;
        break;
    case snap_mode::vertical:
        rpos.x = last.raw().x;
        break;
    }
    return pos;
}

void editor::on_mouse_move(world& world, global_coords& pos, int mods)
{
    if (current_ground_editor() || current_wall_editor())
    {
        if (_last_pos && _last_pos->btn != button::none)
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
                    const auto drawc = draw_coord.raw();
                    auto lastc = last.draw_coord.raw();
                    const auto [minx, maxx] = std::minmax(drawc.x, lastc.x);
                    const auto [miny, maxy] = std::minmax(drawc.y, lastc.y);
                    if (draw_offset[0])
                        for (uint32_t i = minx; i <= maxx; i++)
                            on_click_(world, { i, lastc.y, nullptr }, last.btn);
                    else
                        for (uint32_t j = miny; j <= maxy; j++)
                            on_click_(world, { lastc.x, j, nullptr }, last.btn);
                }
                else
                    on_click_(world, draw_coord, last.btn);
                _last_pos = { InPlaceInit, pos, draw_coord, snap, last.btn };
            }
            pos = draw_coord;
        }
    }
    else
        on_release();
}

Optional<global_coords> editor::mouse_drag_pos()
{
    if (_last_pos)
        return _last_pos->draw_coord;
    else
        return NullOpt;
}

void editor::on_click_(world& world, global_coords pos, button b)
{
    if (auto* mode = current_ground_editor())
    {
        if (auto opt = mode->get_selected(); opt || b == button::remove)
        {
            switch (b)
            {
            case button::place: return mode->place_tile(world, pos, opt);
            case button::remove: return mode->place_tile(world, pos, {});
            default: break;
            }
        }
    }
    else if (auto* mode = current_wall_editor())
    {
        if (auto opt = mode->get_selected(); opt || b == button::remove)
        {
            switch (b)
            {
            case button::place: return mode->place_tile(world, pos, opt);
            case button::remove: return mode->place_tile(world, pos, {});
            default: break;
            }
        }
    }
    else if (auto* mode = current_scenery_editor())
    {
        if (const auto& opt = mode->get_selected(); opt || b == button::remove)
        {
            switch (b)
            {
            default: break;
            case button::place:
                if (const auto& sel = mode->get_selected())
                    if (auto [ch, t] = world[pos]; ch.can_place_object(sel.proto, pos.local()))
                        mode->place_tile(world, pos, sel, *_app);
                break;
            case button::remove:
                mode->place_tile(world, pos, {}, *_app);
                break;
            }
        }
    }
    else if (auto* mode = current_vobj_editor())
    {
        if (const auto* opt = mode->get_selected(); opt || b == button::remove)
        {
            switch (b)
            {
            default: break;
            case button::place:
                if (const auto* sel = mode->get_selected())
                    mode->place_tile(world, pos, sel, *_app);
                break;
            case button::remove:
                mode->place_tile(world, pos, {}, *_app);
                break;
            }
        }
    }
    on_release();
}

void editor::on_click(world& world, global_coords pos, int mods, button b)
{
    if (auto* mode = current_ground_editor())
    {
        _last_pos = { InPlaceInit, pos, pos, mode->check_snap(mods), b };
        on_click_(world, pos, b);
    }
    else if (auto* mode = current_wall_editor())
    {
        _last_pos = { InPlaceInit, pos, pos, mode->check_snap(mods), b };
        on_click_(world, pos, b);
    }
    else if (current_scenery_editor() || current_vobj_editor())
    {
        _last_pos = {};
        on_click_(world, pos, b);
    }
}

editor::editor(app* a) : _app{a} {}

void editor::set_mode(editor_mode mode)
{
    _mode = mode;
    on_release();
}

const ground_editor* editor::current_ground_editor() const noexcept
{
    switch (_mode)
    {
    case editor_mode::floor:
        return &_floor;
    default:
        return nullptr;
    }
}

const wall_editor* editor::current_wall_editor() const noexcept
{
    switch (_mode)
    {
    case editor_mode::walls:
        return &_wall;
    default:
        return nullptr;
    }
}

const scenery_editor* editor::current_scenery_editor() const noexcept
{
    if (_mode == editor_mode::scenery)
        return &_scenery;
    else
        return nullptr;
}

const vobj_editor* editor::current_vobj_editor() const noexcept
{
    if (_mode == editor_mode::vobj)
        return &_vobj;
    else
        return nullptr;
}

ground_editor* editor::current_ground_editor() noexcept
{
    return const_cast<ground_editor*>(static_cast<const editor&>(*this).current_ground_editor());
}

wall_editor* editor::current_wall_editor() noexcept
{
    return const_cast<wall_editor*>(static_cast<const editor&>(*this).current_wall_editor());
}

scenery_editor* editor::current_scenery_editor() noexcept
{
    return const_cast<scenery_editor*>(static_cast<const editor&>(*this).current_scenery_editor());
}

vobj_editor* editor::current_vobj_editor() noexcept
{
    return const_cast<vobj_editor*>(static_cast<const editor&>(*this).current_vobj_editor());
}

} // namespace floormat
