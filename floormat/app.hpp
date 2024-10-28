#pragma once
#include "main/sdl-fwd.hpp"

namespace Magnum::Math { template<typename T> class Vector2; template<class T> class Nanoseconds; }

namespace floormat {

struct mouse_move_event;
struct mouse_button_event;
struct mouse_scroll_event;
struct key_event;
struct text_input_event;
struct text_editing_event;
union any_event;

struct chunk_coords;
struct chunk_coords_;
class chunk;
struct Ns;

struct z_bounds;
struct draw_bounds;

struct floormat_app
{
    explicit floormat_app() noexcept;
    virtual ~floormat_app() noexcept;

    floormat_app(const floormat_app&) = delete;
    floormat_app& operator=(const floormat_app&) = delete;
    [[deprecated]] floormat_app(floormat_app&&) = default;
    [[deprecated]] floormat_app& operator=(floormat_app&&) = default;

    virtual void update(Ns t) = 0;
    virtual void maybe_initialize_chunk(const chunk_coords_& pos, chunk& c) = 0;
    virtual void draw() = 0;
    virtual z_bounds get_z_bounds() = 0;

    virtual void on_mouse_move(const mouse_move_event& event, const sdl2::EvMove& ev) noexcept = 0;
    virtual void on_mouse_up_down(const mouse_button_event& event, bool is_down, const sdl2::EvClick& ev) noexcept = 0;
    virtual void on_mouse_scroll(const mouse_scroll_event& event, const sdl2::EvScroll& ev) noexcept = 0;
    virtual void on_key_up_down(const key_event& event, bool is_down, const sdl2::EvKey& ev) noexcept = 0;
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
