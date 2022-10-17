#include "app.hpp"

namespace floormat {

void app::make_test_chunk(chunk& c)
{
    constexpr auto N = TILE_MAX_DIM;
    for (auto [x, k, pt] : c) {
        const auto& atlas = pt.x > N/2 && pt.y >= N/2 ? floor2 : floor1;
        x.ground_image = { atlas, (std::uint8_t)(k % atlas->num_tiles().product()) };
    }
    constexpr auto K = N/2;
    c[{K,   K  }].wall_north = { wall1, 0 };
    c[{K,   K  }].wall_west  = { wall2, 0 };
    c[{K,   K+1}].wall_north = { wall1, 0 };
    c[{K+1, K  }].wall_west  = { wall2, 0 };
}

void app::do_mouse_click(const global_coords pos, int button)
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

global_coords app::pixel_to_tile(Vector2 position) const
{
    const Vector2 px = position - Vector2{windowSize()}*.5f - camera_offset;
    const Vector2 vec = unproject(px) / Vector2{TILE_SIZE[0]*.5f, TILE_SIZE[1]*.5f} + Vector2{.5f, .5f};
    const auto x = (std::int32_t)std::floor(vec[0]), y = (std::int32_t)std::floor(vec[1]);
    return { x, y };
}

void app::draw_cursor_tile()
{
    if (_cursor_pos)
        draw_wireframe_quad(pixel_to_tile(Vector2(*_cursor_pos)));
}

} // namespace floormat
