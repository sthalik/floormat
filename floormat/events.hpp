#pragma once
#include <Corrade/Containers/StringView.h>
#include <Magnum/Math/Vector2.h>

namespace floormat {

enum mouse_button : unsigned char {
    mouse_button_none   = 0,
    mouse_button_left   = 1 << 0,
    mouse_button_middle = 1 << 1,
    mouse_button_right  = 1 << 2,
    mouse_button_x1     = 1 << 3,
    mouse_button_x2     = 1 << 4,
};

struct mouse_button_event final {
    Vector2 position;
    int mods = 0;
    mouse_button button = mouse_button_none;
    uint8_t click_count = 0;
    bool is_primary : 1 = false;
};

struct mouse_move_event final {
    Vector2 position;
    int mods = 0;
    mouse_button buttons = mouse_button_none;
    bool is_primary : 1 = false;
};

struct mouse_scroll_event final {
    Magnum::Vector2 offset;
    Vector2 position;
    int mods = 0;
    bool is_primary : 1 = false;
};

struct text_input_event final {
    StringView text;
};

struct text_editing_event final {
    StringView text;
    int32_t start = 0, length = 0;
};

struct key_event final {
    int key  = 0;
    int mods = 0;
    uint8_t is_repeated = false;
};

union alignas(alignof(void*)) any_event {
    char buf[64];
};

} // namespace floormat
