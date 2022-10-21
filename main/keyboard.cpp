#include "app.hpp"

namespace floormat {

void app::do_key(KeyEvent::Key k, KeyEvent::Modifiers m, bool pressed, bool repeated)
{
    //using Mods = KeyEvent::Modifiers;

    (void)m;
    (void)repeated;

    const key x = fm_begin(switch (k) {
        using enum KeyEvent::Key;
        using enum key;

    case W:     return camera_up;
    case A:     return camera_left;
    case S:     return camera_down;
    case D:     return camera_right;
    case Home:  return camera_reset;
    case Esc:   return quit;
    default:    return MAX;
    });

    if (x != key::MAX)
        keys[x] = pressed;
}

app::~app()
{
    loader_::destroy();
}

} // namespace floormat
