#include "app.hpp"
#include "src/world.hpp"
#include "src/tile-atlas.hpp"
#include "src/anim-atlas.hpp"
#include "main/clickable.hpp"
#include "floormat/events.hpp"
#include "floormat/main.hpp"

namespace floormat {

//#define FM_NO_BINDINGS

void app::maybe_initialize_chunk_(const chunk_coords& pos, chunk& c)
{
    (void)pos; (void)c;

    [[maybe_unused]] constexpr auto N = TILE_MAX_DIM;
    for (auto [x, k, pt] : c) {
#if 1
        const auto& atlas = _floor1;
#else
        const auto& atlas = pt.x == N/2 || pt.y == N/2 ? _floor2 : _floor1;
#endif
        x.ground() = { atlas, variant_t(k % atlas->num_tiles()) };
    }
#if 0
    constexpr auto K = N/2;
    c[{K,   K  }].wall_north() = { _wall1, 0 };
    c[{K,   K  }].wall_west()  = { _wall2, 0 };
    c[{K,   K+1}].wall_north() = { _wall1, 0 };
    c[{K+1, K  }].wall_west()  = { _wall2, 0 };
    c[{K+3, K+1}].scenery()    = { scenery::door, _door, rotation::N, };
    c[{ 3,   4 }].scenery()    = { scenery::generic, _table, rotation::W, };
    c[{K,   K+1}].scenery()    = { scenery::generic, _control_panel, rotation::N, scenery::frame_t{0}, pass_mode::pass };
#endif
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
    auto& w = M->world();
    update_cursor_tile(cursor.pixel);

    if (is_down && cursor.tile && !cursor.in_imgui)
    {
        switch (_editor.mode())
        {
        default:
            break;
        case editor_mode::none:
            if (button == mouse_button_left)
            {
                if (auto* cl = find_clickable_scenery(*cursor.pixel))
                {
                    auto [c, t] = w[{cl->chunk, cl->pos}];
                    if (auto s = t.scenery())
                        return (void)s.activate();
                }
            }
            break;
        case editor_mode::floor:
        case editor_mode::walls:
        case editor_mode::scenery:
            auto pos = *cursor.tile;
            switch (button)
            {
            case mouse_button_left:
                return _editor.on_click(w, pos, mods, editor::button::place);
            case mouse_button_middle:
                return _editor.on_click(w, pos, mods, editor::button::remove);
            case mouse_button_right:
                return _editor.clear_selection();
            default: break;
            }
            break;
        }
    }
    _editor.on_release();
}

void app::do_rotate(bool backward)
{
    if (auto* ed = _editor.current_tile_editor())
        ed->toggle_rotation();
    else if (auto* ed = _editor.current_scenery_editor())
    {
        if (ed->is_anything_selected())
            backward ? ed->prev_rotation() : ed->next_rotation();
        else if (cursor.tile)
        {
            auto [c, t] = M->world()[*cursor.tile];
            if (auto sc = t.scenery())
            {
                auto [atlas, s] = sc;
                auto r = backward ? atlas->prev_rotation_from(s.r) : atlas->next_rotation_from(s.r);
                if (r != s.r)
                {
                    sc.rotate(r);
                    c.mark_scenery_modified();
                }
            }
        }
    }
}

void app::do_key(key k, int mods)
{
    (void)mods;
    switch (k)
    {
    default:
        if (k >= key_NO_REPEAT)
            fm_warn("unhandled key: '%zu'", std::size_t(k));
        return;
    case key_rotate_tile:
        return do_rotate(false);
    case key_mode_none:
        return _editor.set_mode(editor_mode::none);
    case key_mode_floor:
        return _editor.set_mode(editor_mode::floor);
    case key_mode_walls:
        return _editor.set_mode(editor_mode::walls);
    case key_mode_scenery:
        return _editor.set_mode(editor_mode::scenery);
    case key_mode_collisions:
        return void(_enable_render_bboxes = !_enable_render_bboxes);
    case key_quicksave:
        return do_quicksave();
    case key_quickload:
        return do_quickload();
    case key_new_file:
        return do_new_file();
    case key_escape:
        return do_escape();
    case key_quit:
        return M->quit(0);
    }
}

void app::apply_commands(const key_set& keys)
{
    using value_type = key_set::value_type;
    for (value_type i = key_MIN; i < key_NO_REPEAT; i++)
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
            for (auto& c = world[chunk_coords{x, y}]; auto [x, k, pt] : c)
                if (auto sc = x.scenery())
                {
                    auto [atlas, s] = x.scenery();
                    auto pass0 = s.passability;
                    auto offset0 = s.offset;
                    auto bb_offset0 = s.bbox_offset;
                    auto bb_size0 = s.bbox_size;
                    sc.update(dt);
                    if (pass0 != s.passability || offset0 != s.offset ||
                        bb_offset0 != s.bbox_offset || bb_size0 != s.bbox_size)
                        c.mark_scenery_modified();
                }
}

void app::update(float dt)
{
    apply_commands(keys);
    update_world(dt);
    do_camera(dt, keys, get_key_modifiers());
    clear_non_repeated_keys();

    if (!cursor.in_imgui)
    {
        if (auto* cl = find_clickable_scenery(cursor.pixel))
        {
            auto& w = M->world();
            auto [c, t] = w[{cl->chunk, cl->pos}];
            if (auto sc = t.scenery())
            {
                auto [atlas, s] = sc;
                if (sc && sc.can_activate())
                    M->set_cursor(std::uint32_t(Cursor::Hand));
                else
                    set_cursor_from_imgui();
            }
        }
    }

    M->world().maybe_collect();
}

} // namespace floormat
