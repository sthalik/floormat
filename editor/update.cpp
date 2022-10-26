#include "app.hpp"
#include "src/chunk.hpp"
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
        x.ground_image = { atlas, k % atlas->num_tiles() };
    }
#ifdef FM_NO_BINDINGS
    const auto& wall1 = floor1, wall2 = floor1;
#endif
    constexpr auto K = N/2;
    c[{K,   K  }].wall_north = { _wall1, 0 };
    c[{K,   K  }].wall_west  = { _wall2, 0 };
    c[{K,   K+1}].wall_north = { _wall1, 0 };
    c[{K+1, K  }].wall_west  = { _wall2, 0 };
}

void app::maybe_initialize_chunk([[maybe_unused]] const chunk_coords& pos, [[maybe_unused]] chunk& c)
{
    //maybe_initialize_chunk_(pos, c);
}

void app::do_mouse_move()
{
    if (cursor.tile && !cursor.in_imgui)
        _editor.on_mouse_move(M->world(), *cursor.tile);
}

void app::do_mouse_up_down(std::uint8_t button, bool is_down)
{
    if (!cursor.in_imgui && button == mouse_button_left && is_down)
        _editor.on_click(M->world(), *cursor.tile);
    else
        _editor.on_release();
}

auto app::get_nonrepeat_keys() -> enum_bitset<key>
{
    enum_bitset<key> ret;
    using type = std::underlying_type_t<key>;
    for (auto i = (type)key::NO_REPEAT; i < (type)key::COUNT; i++)
    {
        auto idx = key{i};
        if (keys_repeat[idx])
            keys[idx] = false;
        else
            ret[idx] = keys[idx];
        keys[idx] = false;
        keys_repeat[idx] = false;
    }
    return ret;
}

void app::do_keys()
{
    auto k = get_nonrepeat_keys();
    using type = std::underlying_type_t<key>;

    for (auto i = (type)key::NO_REPEAT; i < (type)key::COUNT; i++)
    {
        auto idx = key{i};
        if (keys_repeat[idx])
            k[idx] = false;
        keys[idx] = false;
        keys_repeat[idx] = false;
    }

    if (k[key::quicksave])
        do_quicksave();
    if (k[key::quickload])
        do_quickload();
    if (auto* ed = _editor.current(); ed && k[key::rotate_tile])
        ed->rotate_tile();
}

void app::update(float dt)
{
    if (keys[key::quit])
        M->quit(0);
    else {
        do_camera(dt);
        draw_ui();
        do_keys();
    }
}

} // namespace floormat
