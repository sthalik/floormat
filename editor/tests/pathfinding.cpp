#include "../tests-private.hpp"
#include "compat/shared-ptr-wrapper.hpp"
#include "editor/app.hpp"
#include "src/world.hpp"
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

    explicit constexpr operator bool() const { return count != 0 && count != -1u; }
};

constexpr inline auto empty_step = step_s{0, {}}, invalid_step = step_s{-1u, {}};

constexpr step_s next_step(Vector2i vec_in)
{
    const auto vec = Vector2ui(Math::abs(vec_in));
    const auto signs = Vector2b(Math::sign(vec_in));
    fm_debug_assert(!vec.isZero());

    if (vec.y() == 0)
        return { vec.x(), Vector2b{1, 0} * signs };
    else if (vec.x() == 0)
        return { vec.y(), Vector2b{0, 1} * signs };
    else if (vec.x() == vec.y())
        return { vec.x(), Vector2b{1} * signs };
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
        fm_debug_assert(num_axis_aligned > 0);
        auto axis_aligned = Vector2b{};
        axis_aligned[major] = 1;
        return { num_axis_aligned, axis_aligned * signs };
        // moving on the minor axis first looks more natural

    }
}

struct result_s
{
    bool has_value : 1 = false;
};

struct pending_s
{
    bool has_value : 1 = false;
};

struct pf_test final : base_test
{
    result_s result;
    pending_s pending;

    ~pf_test() noexcept override = default;

    step_s get_next_step(point from, point to);

    bool handle_key(app& a, const key_event& e, bool is_down) override;
    bool handle_mouse_click(app& a, const mouse_button_event& e, bool is_down) override;
    bool handle_mouse_move(app& a, const mouse_move_event&) override { return {}; }
    void draw_overlay(app& a) override;
    void draw_ui(app& a, float menu_bar_height) override;
    void update_pre(app& a) override;
    void update_post(app& a) override;
};

step_s pf_test::get_next_step(point from, point to)
{
    if (from.chunk3().z != to.chunk3().z)
        return invalid_step;

    if (from == to)
        return empty_step;

    const auto vec = to.coord() - from.coord();

    fm_debug_assert(!vec.isZero());

    return next_step(vec);
}

bool pf_test::handle_key(app& a, const key_event& e, bool is_down)
{
    return false;
}

bool pf_test::handle_mouse_click(app& a, const mouse_button_event& e, bool is_down)
{
    return false;
}

void pf_test::draw_overlay(app& a)
{
}

void pf_test::draw_ui(app& a, float menu_bar_height)
{
}

void pf_test::update_pre(app& a)
{
    if (!pending.has_value)
        return;
    pending.has_value = false;

    auto& m = a.main();
    auto& c = *a.ensure_player_character(m.world()).ptr;
}

void pf_test::update_post(app& a)
{
}

} // namespace

Pointer<base_test> tests_data::make_test_pathfinding() { return Pointer<pf_test>{InPlaceInit}; }

} // namespace floormat::tests
