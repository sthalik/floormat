#include "app.hpp"

namespace floormat {

void app::make_test_chunk(chunk& c)
{
    constexpr auto N = TILE_MAX_DIM;
    for (auto [x, k, pt] : c) {
        const auto& atlas = pt.x != pt.y && (pt.x == N/2 || pt.y == N/2) ? floor2 : floor1;
        x.ground_image = { atlas, (std::uint8_t)(k % atlas->num_tiles()) };
    }
    constexpr auto K = N/2;
    c[{K,   K  }].wall_north = { wall1, 0 };
    c[{K,   K  }].wall_west  = { wall2, 0 };
    c[{K,   K+1}].wall_north = { wall1, 0 };
    c[{K+1, K  }].wall_west  = { wall2, 0 };
}

void app::do_mouse_click(const global_coords pos, int button)
{
    _editor.maybe_place_tile(_world, pos, button);
}

void app::update(double dt)
{
    do_camera(dt);
    draw_ui();
    if (keys[key::quit])
        Platform::Sdl2Application::exit(0);
}

} // namespace floormat
