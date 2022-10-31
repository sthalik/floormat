#pragma once
#include <Magnum/Math/Vector2.h>

namespace floormat {

enum mouse_button : std::uint8_t {
    mouse_button_none   = 0,
    mouse_button_left   = 1 << 0,
    mouse_button_middle = 1 << 1,
    mouse_button_right  = 1 << 3,
    mouse_button_x1     = 1 << 4,
    mouse_button_x2     = 1 << 5,
};

struct mouse_button_event final {
    Vector2i position;
    int mods = 0;
    mouse_button button = mouse_button_none;
    std::uint8_t click_count = 0;
};

struct mouse_move_event final {
    Vector2i position, relative_position;
    mouse_button buttons = mouse_button_none;
    int mods = 0;
};

struct mouse_scroll_event final {
    Magnum::Vector2 offset;
    Vector2i position;
    int mods = 0;
};

struct text_input_event final {
    Containers::StringView text;
};

struct text_editing_event final {
    Containers::StringView text;
    std::int32_t start = 0, length = 0;
};

struct key_event final {
    int key  = 0;
    int mods = 0;
    std::uint8_t is_repeated = false;
};

union alignas(alignof(void*)) any_event {
    std::size_t size[0] = {};
    char buf[64];
};

} // namespace floormat
