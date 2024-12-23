#include "app.hpp"
#include "editor.hpp"
#include "ground-editor.hpp"
#include "wall-editor.hpp"
#include "scenery-editor.hpp"
#include "vobj-editor.hpp"
#include "src/world.hpp"
#include "src/ground-atlas.hpp"
#include "src/anim-atlas.hpp"
#include "main/clickable.hpp"
#include "floormat/events.hpp"
#include "floormat/main.hpp"
#include "floormat/draw-bounds.hpp"
#include "src/critter.hpp"
#include "src/nanosecond.hpp"
#include "src/timer.hpp"
#include "keys.hpp"
#include "loader/loader.hpp"
#include "compat/enum-bitset.hpp"
#include <cmath>

namespace floormat {

//#define FM_NO_BINDINGS

void app::maybe_initialize_chunk_([[maybe_unused]] const chunk_coords_& pos, chunk& c)
{
    auto floor1 = loader.ground_atlas("floor-tiles");

    for (auto k = 0u; k < TILE_COUNT; k++)
        c[k].ground() = { floor1, variant_t(k % floor1->num_tiles()) };
    c.mark_modified();
}

void app::maybe_initialize_chunk([[maybe_unused]] const chunk_coords_& pos, [[maybe_unused]] chunk& c)
{
    //maybe_initialize_chunk_(pos, c);
}

void app::do_mouse_move(int mods)
{
    if (cursor.tile && !cursor.in_imgui)
        _editor->on_mouse_move(M->world(), *cursor.tile, mods);
}

void app::do_mouse_up_down(uint8_t button, bool is_down, int mods)
{
    auto& w = M->world();
    update_cursor_tile(cursor.pixel);

    if (is_down && !cursor.in_imgui)
        _popup_target = {};

    if (is_down && cursor.tile && !cursor.in_imgui)
    {
        switch (_editor->mode())
        {
        case editor_mode::tests:
            break;
        case editor_mode::none:
            if (button == mouse_button_left)
            {
                if (auto* cl = find_clickable_scenery(*cursor.pixel))
                {
                    auto& e = *cl->e;
                    const auto i = e.index();
                    return e.can_activate(i) ? (void)e.activate(i) : void();
                }
            }
            // TODO it should open on mouseup if still on the same item as on mousedown
            else if (button == mouse_button_right)
                if (auto* cl = find_clickable_scenery(*cursor.pixel))
                {
                    _pending_popup = true;
                    _popup_target = { .id = cl->e->id, .target = popup_target_type::scenery, };
                }
            break;
        case editor_mode::floor:
        case editor_mode::walls:
        case editor_mode::scenery:
        case editor_mode::vobj: {
            const auto pos = *cursor.tile;
            switch (button)
            {
            case mouse_button_left:
                return _editor->on_click(w, pos, mods, editor::button::place);
            case mouse_button_middle:
                return _editor->on_click(w, pos, mods, editor::button::remove);
            case mouse_button_right:
                return _editor->clear_selection();
            default: break;
            }
            break;
        }
        }
    }
    _editor->on_release();
}

void app::do_mouse_scroll(int offset)
{
    auto mods = get_key_modifiers();
    if (!(mods & (kmod_ctrl|kmod_shift)))
        return;
    int min_z = mods & kmod_ctrl ? chunk_z_min : std::max(0, (int)chunk_z_min);
    _z_level = (int8_t)Math::clamp(_z_level + offset, min_z, (int)chunk_z_max);
}

void app::do_rotate(bool backward)
{
    if (auto* ed = _editor->current_wall_editor())
        ed->toggle_rotation();
    else if (auto* ed = _editor->current_scenery_editor())
    {
        if (ed->is_anything_selected())
            backward ? ed->prev_rotation() : ed->next_rotation();
        else if (auto* cl = find_clickable_scenery(*cursor.pixel))
        {
            auto& e = *cl->e;
            auto i = e.index();
            auto r = backward ? e.atlas->prev_rotation_from(e.r) : e.atlas->next_rotation_from(e.r);
            e.rotate(i, r);
        }
    }
}

void app::do_emit_timestamp()
{
    constexpr const char* prefix = " -- MARK -- ";

    static unsigned counter;
    const auto now = Time::now();
    auto time = (double)Time::to_milliseconds(now - Time{_timestamp});
    if (counter == 0)
        time = 0;
    char buf[fm_DATETIME_BUF_SIZE];
    format_datetime_to_string(buf);

    if (time >= 1e5)
        fm_debug("%s%s0x%08x %.1f" " s", buf, prefix, counter++, time*1e-3);
    else if (time >= 1e4)
        fm_debug("%s%s0x%08x %.2f" " s", buf, prefix, counter++, time*1e-3);
    else if (time >= 1e3)
        fm_debug("%s%s0x%08x %.2f" " ms", buf, prefix, counter++, time);
    else if (time > 0)
        fm_debug("%s%s0x%08x %.4f" " ms", buf, prefix, counter++, time);
    else
        fm_debug("%s%s0x%08x 0 ms", buf, prefix, counter++);
    _timestamp = now.stamp;
}

void app::do_set_mode(editor_mode mode)
{
    if (mode != _editor->mode())
        kill_popups(false);
    _editor->set_mode(mode);
}

void app::do_escape()
{
    if (auto* ed = _editor->current_ground_editor())
        ed->clear_selection();
    if (auto* ed = _editor->current_wall_editor())
        ed->clear_selection();
    if (auto* sc = _editor->current_scenery_editor())
        sc->clear_selection();
    if (auto* vo = _editor->current_vobj_editor())
        vo->clear_selection();
    kill_popups(false);
}

void app::do_key(key k, int mods, int keycode)
{
    (void)mods;
    switch (k)
    {
    default:
        if (k >= key_NO_REPEAT)
            fm_warn("unhandled key: '%d'", keycode);
        return;
    case key_noop:
        return;
    case key_rotate_tile:
        return do_rotate(false);
    case key_emit_timestamp:
        return do_emit_timestamp();
    case key_mode_none:
        return do_set_mode(editor_mode::none);
    case key_mode_floor:
        return do_set_mode(editor_mode::floor);
    case key_mode_walls:
        return do_set_mode(editor_mode::walls);
    case key_mode_scenery:
        return do_set_mode(editor_mode::scenery);
    case key_mode_vobj:
        return do_set_mode(editor_mode::vobj);
    case key_mode_tests:
        return do_set_mode(editor_mode::tests);
    case key_render_collision_boxes:
        return void(_render_bboxes = !_render_bboxes);
    case key_render_clickables:
        return void(_render_clickables = !_render_clickables);
    case key_render_vobjs:
        return M->set_render_vobjs(_render_vobjs = !_render_vobjs);
    case key_render_all_z_levels:
        return void(_render_all_z_levels = !_render_all_z_levels);
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
            do_key(k, key_modifiers.data[i], 0);
}

void app::update_world(Ns dt)
{
    auto& world = M->world();
    const auto frame_no = world.increment_frame_no();
    auto [minx, maxx, miny, maxy] = M->get_draw_bounds();
    minx--; miny--; maxx++; maxy++;
    for (int8_t z = chunk_z_min; z <= chunk_z_max; z++)
        for (int16_t y = miny; y <= maxy; y++)
            for (int16_t x = minx; x <= maxx; x++)
            {
                auto* const cʹ = world.at({x, y, z});
                if (!cʹ)
                    continue;
                auto& c = *cʹ;
                auto size = (uint32_t)c.objects().size();
                for (auto i = 0u; i < size; i++)
                {
                    auto index = size_t{i};
                    auto& e = *c.objects().data()[i].get();
                    if (e.last_frame_no == frame_no) [[unlikely]]
                        continue;
                    e.last_frame_no = frame_no;
                    e.update(c.objects().data()[i], index, dt); // objects can't delete themselves during update()
                    if (&e.chunk() != cʹ || index > i) [[unlikely]]
                    {
                        i--;
                        size = (uint32_t)c.objects().size();
                    }
                }
            }

#ifndef FM_NO_DEBUG
    for (int8_t z = chunk_z_min; z <= chunk_z_max; z++)
        for (int16_t y = miny; y <= maxy; y++)
            for (int16_t x = minx; x <= maxx; x++)
            {
                auto* cʹ = world.at({x, y, z});
                if (!cʹ)
                    continue;
                auto& c = *cʹ;
                const auto& es = c.objects();
                auto size = es.size();
                for (auto i = 0uz; i < size; i++)
                {
                    auto& e = *es[i];
                    fm_assert(e.last_frame_no == frame_no);
                }
            }
#endif
}

void app::update_character([[maybe_unused]] Ns dt)
{
    auto& keys = *keys_;
    if (_character_id)
    {
        auto& w = M->world();
        auto c = w.find_object<critter>(_character_id);
        if (c && c->playable)
            c->set_keys(keys[key_left], keys[key_right], keys[key_up], keys[key_down]);
    }
}

void app::set_cursor()
{
    if (!cursor.in_imgui)
    {
        if ([[maybe_unused]] auto* cl = find_clickable_scenery(cursor.pixel))
            M->set_cursor(uint32_t(Cursor::Hand));
        else
            M->set_cursor(uint32_t(Cursor::Arrow));
    }
    else
        set_cursor_from_imgui();
}

auto app::get_z_bounds() -> z_bounds
{
    return { chunk_z_min, chunk_z_max, _z_level, !_render_all_z_levels };
}

void app::update(Ns dt)
{
    auto& w = M->world();
    w.collect(true);
    update_cursor_tile(cursor.pixel);
    tests_pre_update(dt);
    apply_commands(*keys_);
    { auto status = w.script_status();
      fm_assert(status.initialized);
      fm_assert(!status.finalized);
    }
    update_character(dt);
    update_world(dt);
    do_camera(dt, *keys_, get_key_modifiers());
    clear_non_repeated_keys();
    set_cursor();
    tests_post_update(dt);
}

} // namespace floormat
