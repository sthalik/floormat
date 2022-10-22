#include "app.hpp"

namespace floormat {

//#define TEST_NO_BINDINGS

void app::make_test_chunk(chunk& c)
{
    constexpr auto N = TILE_MAX_DIM;
    for (auto [x, k, pt] : c) {
#ifdef TEST_NO_BINDINGS
        const auto& atlas = floor1;
#else
        const auto& atlas = pt.x != pt.y && (pt.x == N/2 || pt.y == N/2) ? floor2 : floor1;
#endif
        x.ground_image = { atlas, k % atlas->num_tiles() };
    }
#ifdef TEST_NO_BINDINGS
    const auto& wall1 = floor1, wall2 = floor1;
#endif
    constexpr auto K = N/2;
    c[{K,   K  }].wall_north = { wall1, 0 };
    c[{K,   K  }].wall_west  = { wall2, 0 };
    c[{K,   K+1}].wall_north = { wall1, 0 };
    c[{K+1, K  }].wall_west  = { wall2, 0 };
}

void app::do_mouse_click(const global_coords pos, int button)
{
    if (button == SDL_BUTTON_LEFT)
        _editor.on_click(_world, pos);
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
    _editor.on_mouse_move(_world, pos);
}

void app::update(double dt)
{
    do_camera(dt);
    draw_ui();
    if (keys[key::quit])
        Platform::Sdl2Application::exit(0);
}

} // namespace floormat
