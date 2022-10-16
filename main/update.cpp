#include "app.hpp"

namespace floormat {

chunk app::make_test_chunk()
{
    constexpr auto N = TILE_MAX_DIM;
    chunk c;
    for (auto [x, k, pt] : c) {
        const auto& atlas = pt.x > N/2 && pt.y >= N/2 ? floor2 : floor1;
        x.ground_image = { atlas, (std::uint8_t)(k % atlas->num_tiles().product()) };
    }
    constexpr auto K = N/2;
    c[{K,   K  }].wall_north = { wall1, 0 };
    c[{K,   K  }].wall_west  = { wall2, 0 };
    c[{K,   K+1}].wall_north = { wall1, 0 };
    c[{K+1, K  }].wall_west  = { wall2, 0 };
    return c;
}

void app::do_mouse_click(const Vector2 pos, int button)
{
    _editor.click_at_tile(pos, button);
}

void app::update(float dt)
{
    do_camera(dt);
    do_menu();
    if (keys[key::quit])
        Platform::Sdl2Application::exit(0);
}

Vector2 app::pixel_to_tile(Vector2 position) const
{
    const auto px = position - Vector2{windowSize()}*.5f - camera_offset;
    return unproject(px) / Vector2{TILE_SIZE[0]*.5f, TILE_SIZE[1]*.5f} + Vector2{.5f, .5f};
}

void app::draw_cursor_tile()
{
    if (_cursor_pos)
    {
        const auto tile = pixel_to_tile(Vector2(*_cursor_pos));
        if (std::min(tile[0], tile[1]) >= 0 && std::max(tile[0], tile[1]) < (int)TILE_MAX_DIM)
        {
            const auto x = std::uint8_t(tile[0]), y = std::uint8_t(tile[1]);
            draw_wireframe_quad({x, y});
        }
    }
}

} // namespace floormat
