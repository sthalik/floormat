#include "app.hpp"
#include "src/world.hpp"
#include "src/tile-atlas.hpp"
#include "main/clickable.hpp"
#include "floormat/events.hpp"
#include "floormat/main.hpp"

namespace floormat {

//#define FM_NO_BINDINGS

void app::maybe_initialize_chunk_(const chunk_coords& pos, chunk& c)
{
    (void)pos; (void)c;

    constexpr auto N = TILE_MAX_DIM;
    for (auto [x, k, pt] : c) {
#if defined FM_NO_BINDINGS
        const auto& atlas = floor1;
#else
        const auto& atlas = pt.x == N/2 || pt.y == N/2 ? _floor2 : _floor1;
#endif
        x.ground() = { atlas, variant_t(k % atlas->num_tiles()) };
    }
#ifdef FM_NO_BINDINGS
    const auto& wall1 = floor1, wall2 = floor1;
#endif
    constexpr auto K = N/2;
    c[{K,   K  }].wall_north() = { _wall1, 0 };
    c[{K,   K  }].wall_west()  = { _wall2, 0 };
    c[{K,   K+1}].wall_north() = { _wall1, 0 };
    c[{K+1, K  }].wall_west()  = { _wall2, 0 };
    c[{K+3, K+1}].scenery()    = { scenery::door, rotation::N, _door, false };
    c.mark_modified();
}

void app::maybe_initialize_chunk([[maybe_unused]] const chunk_coords& pos, [[maybe_unused]] chunk& c)
{
    //maybe_initialize_chunk_(pos, c);
}

void app::do_mouse_move(int mods)
{
    if (cursor.tile && !cursor.in_imgui)
        _editor.on_mouse_move(M->world(), *cursor.tile, mods);
}

void app::do_mouse_up_down(std::uint8_t button, bool is_down, int mods)
{
    update_cursor_tile(cursor.pixel);
    if (!_editor.current_tile_editor())
    {
        if (cursor.tile)
            if (auto* s = find_clickable_scenery(*cursor.pixel))
                s->item.activate(s->atlas);
    }
    else if (cursor.tile && !cursor.in_imgui && is_down)
    {
        auto& w = M->world();
        auto pos = *cursor.tile;
        switch (button)
        {
        case mouse_button_left:
            return _editor.on_click(w, pos, mods, editor::button::place);
        case mouse_button_middle:
            return _editor.on_click(w, pos, mods, editor::button::remove);
        default: break;
        }
    }
    _editor.on_release();
}

void app::do_key(key k, int mods)
{
    (void)mods;
    switch (k)
    {
    default:
        fm_warn("unhandled key: '%zu'", std::size_t(k));
        return;
    case key_rotate_tile:
        if (auto* ed = _editor.current_tile_editor(); ed)
            ed->toggle_rotation();
        return;
    case key_mode_none:
        return _editor.set_mode(editor_mode::none);
    case key_mode_floor:
        return _editor.set_mode(editor_mode::floor);
    case key_mode_walls:
        return _editor.set_mode(editor_mode::walls);
    case key_quicksave:
        return do_quicksave();
    case key_quickload:
        return do_quickload();
    case key_quit:
        return M->quit(0);
    }
}

void app::apply_commands(const key_set& keys)
{
    using value_type = key_set::value_type;
    for (value_type i = key_NO_REPEAT; i < key_COUNT; i++)
        if (const auto k = key(i); keys[k])
            do_key(k, key_modifiers[i]);
}

void app::update_world(float dt)
{
    auto& world = M->world();
    auto [minx, maxx, miny, maxy] = M->get_draw_bounds();
    minx--; miny--; maxx++; maxy++;
    for (std::int16_t y = miny; y <= maxy; y++)
        for (std::int16_t x = minx; x <= maxx; x++)
            for (chunk_coords c{x, y}; auto [x, k, pt] : world[c])
                if (auto [atlas, scenery] = x.scenery(); atlas != nullptr)
                    scenery.update(dt, *atlas);
}

void app::update(float dt)
{
    update_world(dt);
    apply_commands(keys);
    do_camera(dt, keys, get_key_modifiers());
    clear_non_repeated_keys();

    if (!_editor.current_tile_editor() && cursor.tile && find_clickable_scenery(*cursor.pixel))
        M->set_cursor(std::uint32_t(Cursor::Hand));
    else
        M->set_cursor(std::uint32_t(Cursor::Arrow));
}

} // namespace floormat
