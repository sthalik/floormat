#include "app.hpp"
#include "main/floormat-events.hpp"

namespace floormat {

bool app::on_key_down(const key_event& event) noexcept
{
    if _imgui.

    const key x = fm_begin(switch (event.key)
    {
    default:            return key::COUNT;
    case SDLK_w:        return key::camera_up;
    case SDLK_a:        return key::camera_left;
    case SDLK_s:        return key::camera_down;
    case SDLK_d:        return key::camera_right;
    case SDLK_HOME:     return key::camera_reset;
    case SDLK_r:        return key::rotate_tile;
    case SDLK_F5:       return key::quicksave;
    case SDLK_F9:       return key::quickload;
    case SDLK_ESCAPE:   return key::quit;
    });

    if (x != key::COUNT)
        keys[x] = event.is_down && !event.is_repeated;
}

} // namespace floormat
