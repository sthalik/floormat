#include "app.hpp"
#include "src/chunk.hpp"
#include "src/tile-atlas.hpp"
#include "main/floormat-events.hpp"
#include "main/floormat-main.hpp"
#include <Magnum/Platform/Sdl2Application.h>

namespace floormat {

//#define FM_NO_BINDINGS

void app::make_test_chunk(chunk& c)
{
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

void app::do_mouse_click(const global_coords pos, int button)
{
    if (button == mouse_button_left)
        _editor.on_click(M->world(), pos);
    else
        _editor.on_release();
}

void app::do_mouse_release(int button)
{
    (void)button;
    _editor.on_release();
}

void app::do_mouse_move(global_coords pos)
{
    //_editor.on_mouse_move(_world, pos);
}

void app::update(float dt)
{
    do_camera(dt);
    draw_ui();
    if (_keys[key::quit])
        M->quit(0);
}

} // namespace floormat
