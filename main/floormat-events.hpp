#pragma once
#include <Magnum/Math/Vector2.h>
#include <SDL_keycode.h>
#include <SDL_mouse.h>
#include <SDL_events.h>

namespace floormat {

enum mouse_button : std::uint8_t {
    mouse_button_none   = 0,
    mouse_button_left   = SDL_BUTTON_LMASK,
    mouse_button_middle = SDL_BUTTON_MMASK,
    mouse_button_right  = SDL_BUTTON_RMASK,
    mouse_button_x1     = SDL_BUTTON_X1MASK,
    mouse_button_x2     = SDL_BUTTON_X2MASK,
};

struct mouse_button_event final {
    Vector2i position;
    SDL_Keymod mods = KMOD_NONE;
    mouse_button button = mouse_button_none;
    std::uint8_t click_count = 0;
};

struct mouse_move_event final {
    Vector2i position, relative_position;
    mouse_button buttons = mouse_button_none;
    SDL_Keymod mods = KMOD_NONE;
};

struct mouse_scroll_event final {
    Magnum::Vector2 offset;
    Vector2i position;
    SDL_Keymod mods = KMOD_NONE;
};

struct text_input_event final {
    Containers::StringView text;
};

struct text_editing_event final {
    Containers::StringView text;
    std::int32_t start = 0, length = 0;
};

struct key_event final {
    SDL_Keycode key = SDLK_UNKNOWN;
    SDL_Keymod mods = KMOD_NONE;
    bool is_repeated = false;
};

struct any_event final {
    SDL_Event event = {};
};

} // namespace floormat
