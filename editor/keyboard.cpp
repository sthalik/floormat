#include "app.hpp"

namespace floormat {

void app::do_key(KeyEvent::Key k, KeyEvent::Modifiers m, bool pressed, bool repeated)
{
    //using Mods = KeyEvent::Modifiers;

    (void)m;
    (void)repeated;

    const key x = fm_begin(switch (k)
    {
    using enum KeyEvent::Key;
    using enum key;

    default:    return COUNT;
    case W:     return camera_up;
    case A:     return camera_left;
    case S:     return camera_down;
    case D:     return camera_right;
    case Home:  return camera_reset;
    case R:     return rotate_tile;
    case F5:    return quicksave;
    case F9:    return quickload;
    case Esc:   return quit;
    });

    if (x != key::COUNT)
        keys[x] = pressed;
}

app::~app()
{
    loader_::destroy();
}

} // namespace floormat
