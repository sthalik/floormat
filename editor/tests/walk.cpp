#include "../tests-private.hpp"
#include "compat/limits.hpp"
#include "compat/shared-ptr-wrapper.hpp"
#include "editor/app.hpp"
#include "src/world.hpp"
#include "src/critter.hpp"
#include "src/point.inl"
#include "src/anim-atlas.hpp"
#include "src/nanosecond.hpp"
#include "floormat/main.hpp"
#include "../imgui-raii.hpp"
#include <mg/Functions.h>

namespace floormat::tests {

using namespace floormat::imgui;

namespace {

struct step_s
{
    uint32_t count;
    Vector2b direction;
};

constexpr step_s next_stepʹ(Vector2i vec_in)
{
    const auto vec = Vector2ui(Math::abs(vec_in));
    const auto signs = Vector2b(Math::sign(vec_in));

    if (vec.x() == vec.y())
        return { vec.x(), Vector2b{1, 1} * signs };
    else if (vec.y() == 0)
        return { vec.x(), Vector2b{1, 0} * signs };
    else if (vec.x() == 0)
        return { vec.y(), Vector2b{0, 1} * signs };
    else
    {
        uint32_t major_idx, minor_idx;
        if (vec.x() > vec.y())
        {
            major_idx = 0;
            minor_idx = 1;
        }
        else
        {
            major_idx = 1;
            minor_idx = 0;
        }
        const auto major = vec[major_idx], minor = vec[minor_idx];
        const auto num_axis_aligned = (uint32_t)Math::abs((int)major - (int)minor);
        auto axis_aligned = Vector2b{};
        axis_aligned[major_idx] = 1;
        return { num_axis_aligned, axis_aligned * signs };
    }
}

constexpr rotation dir_from_step(step_s step)
{
    if (step.direction.isZero()) [[unlikely]]
        return rotation_COUNT;

    auto x = step.direction.x() + 1;
    auto y = step.direction.y() + 1;
    fm_debug_assert((x & 3) == x && (y & 3) == y);
    auto val = x << 2 | y;

    switch (val)
    {
    using enum rotation;
    case 0 << 2 | 0: /* -1 -1 */ return NW;
    case 0 << 2 | 1: /* -1  0 */ return W;
    case 0 << 2 | 2: /* -1  1 */ return SW;
    case 1 << 2 | 0: /*  0 -1 */ return N;
    case 1 << 2 | 1: /*  0  0 */ return rotation_COUNT;
    case 1 << 2 | 2: /*  0  1 */ return S;
    case 2 << 2 | 0: /*  1 -1 */ return NE;
    case 2 << 2 | 1: /*  1  0 */ return E;
    case 2 << 2 | 2: /*  1  1 */ return SE;
    default: return rotation_COUNT;
    }
}

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

constexpr step_s next_step(point from, point to)
{
    fm_debug_assert(from.chunk3().z == to.chunk3().z);
    const auto vec = to - from;
    fm_debug_assert(!vec.isZero());
    return next_stepʹ(vec);
}

constexpr float step_magnitude(Vector2b vec)
{
    constexpr double cʹ = critter::move_speed * critter::frame_time;
    constexpr double dʹ = cʹ / Vector2d{1,  1}.length();
    constexpr auto c = (float)cʹ, d = (float)dʹ;

    if (vec.x() * vec.y() != 0)
        // diagonal
        return c;
    else
        // axis-aligned
        return d;
}

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

#if 0
    auto& m = a.main();
    auto& C = *a.ensure_player_character(m.world()).ptr;
    fm_assert(C.is_dynamic());

    if (C.movement.L | C.movement.R | C.movement.U | C.movement.D) [[unlikely]]
    {
        current.has_value = false;
        return;
    }

    const auto& info = C.atlas->info();
    const auto nframes = C.alloc_frame_time(dt, C.delta, info.fps, C.speed);

    if (nframes == 0)
        return;

    C.set_keys(false, false, false, false);

    auto index = C.index();
    bool ok = true;

    for (uint32_t i = 0; i < nframes; i++)
    {
        C.chunk().ensure_passability();

        const auto from = C.position();
        if (from == current.dest)
        {
            current.has_value = false;
            //Debug{} << "done!" << from;
            C.set_keys(false, false, false, false);
            return;
        }
        const auto step = next_step(from, current.dest);
        //Debug{} << "step" << step.direction << step.count << "|" << C.position();
        C.set_keys_auto();
        if (step.direction == Vector2b{})
        {
            //Debug{} << "no dir break";
            ok = false;
            break;
        }
        fm_assert(step.count > 0);
        const auto new_r = dir_from_step(step);
        using Frac = decltype(critter::offset_frac)::Type;
        constexpr auto frac = (float{limits<Frac>::max}+1)/2;
        constexpr auto inv_frac = 1 / frac;
        const auto mag = step_magnitude(step.direction);
        const auto vec = Vector2(step.direction) * mag;
        const auto sign_vec = Math::sign(vec);
        auto offset_ = vec + Vector2(C.offset_frac) * sign_vec * inv_frac;
        auto off_i = Vector2i(offset_);
        //Debug{} << "vec" << vec << "mag" << mag << "off_i" << off_i << "offset_" << Vector2(C.offset_frac) * sign_vec * inv_frac;

        if (!off_i.isZero())
        {
            C.offset_frac = Math::Vector2<Frac>(Math::abs(Math::fmod(offset_, 1.f)) * frac);
            if (C.can_move_to(off_i))
            {
                C.move_to(index, off_i, new_r);
                ++C.frame %= info.nframes;
            }
            else
            {
                ok = false;
                break;
            }
        }
        else
            C.offset_frac = Math::Vector2<Frac>(Math::min({2.f,2.f}, Math::abs(offset_)) * frac);
    }

    if (!ok) [[unlikely]]
    {
        C.set_keys(false, false, false, false);
        C.delta = {};
        C.offset_frac = {};
        current.has_value = false;
    }
#endif
}

} // namespace

Pointer<base_test> tests_data::make_test_walk() { return Pointer<pf_test>{InPlaceInit}; }

} // namespace floormat::tests
