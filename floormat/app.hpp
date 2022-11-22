#pragma once

namespace Magnum::Math { template<typename T> class Vector2; }

namespace floormat {

struct mouse_move_event;
struct mouse_button_event;
struct mouse_scroll_event;
struct key_event;
struct text_input_event;
struct text_editing_event;
union any_event;

struct chunk_coords;
struct chunk;

struct floormat_app
{
    explicit floormat_app() noexcept;
    virtual ~floormat_app() noexcept;

    floormat_app(const floormat_app&) = delete;
    floormat_app& operator=(const floormat_app&) = delete;
    [[deprecated]] floormat_app(floormat_app&&) = default;
    [[deprecated]] floormat_app& operator=(floormat_app&&) = default;

    virtual void update(float dt) = 0;
    virtual void maybe_initialize_chunk(const chunk_coords& pos, chunk& c) = 0;
    virtual void draw() = 0;

    virtual void on_mouse_move(const mouse_move_event& event) noexcept = 0;
    virtual void on_mouse_up_down(const mouse_button_event& event, bool is_down) noexcept = 0;
    virtual void on_mouse_scroll(const mouse_scroll_event& event) noexcept = 0;
    virtual void on_key_up_down(const key_event& event, bool is_down) noexcept = 0;
    virtual void on_text_input_event(const text_input_event& event) noexcept = 0;
    //virtual bool on_text_editing_event(const text_editing_event& event) noexcept = 0;
    virtual void on_viewport_event(const Magnum::Math::Vector2<int>& size) noexcept = 0;
    virtual void on_any_event(const any_event& event) noexcept = 0;
    virtual void on_focus_in() noexcept = 0;
    virtual void on_focus_out() noexcept = 0;
    virtual void on_mouse_leave() noexcept = 0;
    virtual void on_mouse_enter() noexcept = 0;
};

} // namespace floormat
