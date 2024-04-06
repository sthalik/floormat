#include "../tests-private.hpp"
#include "compat/shared-ptr-wrapper.hpp"
#include "editor/app.hpp"
#include "src/critter.hpp"
#include "floormat/main.hpp"
#include "../imgui-raii.hpp"

namespace floormat::tests {

using namespace floormat::imgui;

namespace {

struct pending_s
{
    point dest;
    bool has_value : 1 = false;
};

struct pf_test final : base_test
{
    pending_s current;

    ~pf_test() noexcept override = default;

    bool handle_key(app& a, const key_event& e, bool is_down) override;
    bool handle_mouse_click(app& a, const mouse_button_event& e, bool is_down) override;
    bool handle_mouse_move(app& a, const mouse_move_event& e) override;
    void draw_overlay(app& a) override;
    void draw_ui(app& a, float menu_bar_height) override;
    void update_pre(app& a, const Ns& dt) override;
    void update_post(app&, const Ns&) override {}
};

bool pf_test::handle_key(app& a, const key_event& e, bool is_down)
{
    (void) a; (void)e; (void)is_down;
    return false;
}

bool pf_test::handle_mouse_click(app& a, const mouse_button_event& e, bool is_down)
{
    if (e.button == mouse_button_left && is_down)
    {
        if (auto ptʹ = a.cursor_state().point())
        {
            current = {
                .dest = *ptʹ,
                .has_value = true,
            };
            return true;
        }
    }
    else if (e.button == mouse_button_right && is_down)
        current = {};
    return false;
}

bool pf_test::handle_mouse_move(floormat::app &a, const mouse_move_event& e)
{
    if (e.buttons & mouse_button_left)
        return handle_mouse_click(a, {e.position, e.mods, mouse_button_left, 1}, true);
    else
        return false;
}

void pf_test::draw_overlay(app& a)
{
    (void)a;
}

void pf_test::draw_ui(app& a, float)
{
    (void)a;
}

void pf_test::update_pre(app& a, const Ns& dt)
{
    if (!current.has_value)
        return;

    auto& m = a.main();
    auto& C = *a.ensure_player_character(m.world()).ptr;
    fm_assert(C.is_dynamic());

    if (C.movement.L | C.movement.R | C.movement.U | C.movement.D) [[unlikely]]
    {
        current.has_value = false;
        return;
    }

    auto index = C.index();
    critter::move_result result;

    if (current.dest == C.position() ||
        (void(result = C.move_toward(index, dt, current.dest)), result.blocked) ||
        result.moved && current.dest == C.position())
        current.has_value = false;
}

} // namespace

Pointer<base_test> tests_data::make_test_walk() { return Pointer<pf_test>{InPlaceInit}; }

} // namespace floormat::tests
