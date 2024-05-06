#include "../tests-private.hpp"
#include "compat/shared-ptr-wrapper.hpp"
#include "editor/app.hpp"
#include "src/critter.hpp"
#include "src/critter-script.hpp"
#include "src/search-result.hpp"
#include "floormat/main.hpp"
#include "../imgui-raii.hpp"

namespace floormat::tests {

using namespace floormat::imgui;

namespace {

struct pf_test final : base_test
{
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
    auto& m = a.main();

    if (e.button == mouse_button_left && is_down)
    {
        if (auto ptʹ = a.cursor_state().point())
        {

            auto C = a.ensure_player_character(m.world()).ptr;
            fm_assert(C->is_dynamic());
            auto S = critter_script::make_walk_script(*ptʹ, {}, critter_script::walk_mode::line);
            C->script.do_reassign(move(S), move(C));
            return true;
        }
    }
    else if (e.button == mouse_button_right && is_down)
    {
        auto C = a.ensure_player_character(m.world()).ptr;
        C->script.do_clear(C);
    }
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
    (void)a;
    (void)dt;
}

} // namespace

Pointer<base_test> tests_data::make_test_walk() { return Pointer<pf_test>{InPlaceInit}; }

} // namespace floormat::tests
