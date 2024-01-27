#include "../tests-private.hpp"
#include <array>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Vector2.h>

namespace floormat::tests {

namespace {

struct aabb_result
{
    Vector2 ts;
    bool result;
};

std::array<uint8_t, 2> ray_aabb_signs(Vector2 ray_dir_inv_norm)
{
    bool signs[2];
    for (unsigned d = 0; d < 2; ++d)
        signs[d] = std::signbit(ray_dir_inv_norm[d]);
    return { signs[0], signs[1] };
}

Vector2 dir_to_inv_norm(Vector2 ray_dir)
{
    constexpr float eps = 1e-6f;
    auto dir = ray_dir.normalized();
    Vector2 inv_dir{NoInit};
    for (unsigned i = 0; i < 2; i++)
        if (Math::abs(dir[i]) < eps)
            inv_dir[i] = 0;
        else
            inv_dir[i] = 1 / dir[i];
    return inv_dir;
}

// https://tavianator.com/2022/ray_box_boundary.html
// https://www.researchgate.net/figure/The-slab-method-for-ray-intersection-detection-15_fig3_283515372
aabb_result ray_aabb_intersection(Vector2 ray_origin, Vector2 ray_dir_inv_norm,
                                  std::array<Vector2, 2> box_minmax, std::array<uint8_t, 2> signs)
{
    using Math::min;
    using Math::max;

    float ts[2];
    float tmin = 0, tmax = 16777216;

    for (unsigned d = 0; d < 2; ++d)
    {
        float bmin = box_minmax[signs[d]][d];
        float bmax = box_minmax[!signs[d]][d];

        float dmin = (bmin - ray_origin[d]) * ray_dir_inv_norm[d];
        float dmax = (bmax - ray_origin[d]) * ray_dir_inv_norm[d];

        ts[d] = dmin;
        tmin = max(dmin, tmin);
        tmax = min(dmax, tmax);
    }

    return { {ts[0], ts[1] }, tmin < tmax };
}

} // namespace

struct raycast_test : base_test
{
    ~raycast_test() noexcept override;

    bool handle_key(app& a, const key_event& e, bool is_down) override
    {
        return false;
    }

    bool handle_mouse_click(app& a, const mouse_button_event& e, bool is_down) override
    {
        return false;
    }

    bool handle_mouse_move(app& a, const mouse_move_event& e) override
    {
        return false;
    }

    void draw_overlay(app& a) override
    {

    }

    void draw_ui(app& a, float width) override
    {

    }

    void update_pre(app& a) override
    {

    }

    void update_post(app& a) override
    {

    }
};

raycast_test::~raycast_test() noexcept = default;

Pointer<base_test> tests_data::make_test_raycast() { return Pointer<raycast_test>{InPlaceInit}; }

} // namespace floormat::tests
