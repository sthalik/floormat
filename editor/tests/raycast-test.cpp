#include "../tests-private.hpp"
#include "editor/app.hpp"
#include "floormat/main.hpp"
#include "compat/shared-ptr-wrapper.hpp"
#include "src/critter.hpp"
#include "../imgui-raii.hpp"
#include <memory>
#include <array>
#include <vector>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Vector2.h>

namespace floormat::tests {

namespace {

using namespace imgui;

struct aabb_result
{
    Vector2 ts;
    bool result;
};

template<typename T>
requires std::is_arithmetic_v<T>
std::array<uint8_t, 2> ray_aabb_signs(Math::Vector2<T> ray_dir_inv_norm)
{
    bool signs[2];
    for (unsigned d = 0; d < 2; ++d)
        signs[d] = std::signbit(ray_dir_inv_norm[d]);
    return { signs[0], signs[1] };
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

struct bbox
{
    point center;
    Vector2ui size;
};

struct result_s
{
    point from, to;
    Vector3d vec;
    std::vector<bbox> path;
    bool has_result : 1 = false;
};

struct pending_s
{
    point from, to;
    bool exists : 1 = false;
};

} // namespace

struct raycast_test : base_test
{
    result_s result;
    pending_s pending;

    ~raycast_test() noexcept override;

    bool handle_key(app& a, const key_event& e, bool is_down) override
    {
        return false;
    }

    bool handle_mouse_click(app& a, const mouse_button_event& e, bool is_down) override
    {
        if (e.button == mouse_button_left && is_down)
        {
            auto& M = a.main();
            auto& w = M.world();
            if (auto pt_ = a.cursor_state().point())
            {
                auto C = a.ensure_player_character(w).ptr;
                auto pt0 = C->position();
                pending = { .from = pt0, .to = *pt_, .exists = true, };
                return true;
            }
        }

        return false;
    }

    bool handle_mouse_move(app& a, const mouse_move_event& e) override
    {
        return false;
    }

    void draw_overlay(app& a) override
    {
        if (!result.has_result)
            return;

        const auto color = ImGui::ColorConvertFloat4ToU32({1, 0, 0, 1});
        ImDrawList& draw = *ImGui::GetForegroundDrawList();

        for (auto [center, size] : result.path)
        {
            //auto c = a.point_screen_pos(center);
            //draw.AddCircleFilled({c.x(), c.y()}, 3, color);
            const auto hx = (int)(size.x()/2), hy = (int)(size.y()/2);
            auto p00 = a.point_screen_pos(object::normalize_coords(center, {-hx, -hy})),
                 p10 = a.point_screen_pos(object::normalize_coords(center, {hx, -hy})),
                 p01 = a.point_screen_pos(object::normalize_coords(center, {-hx, hy})),
                 p11 = a.point_screen_pos(object::normalize_coords(center, {hx, hy}));
            draw.AddLine({p00.x(), p00.y()}, {p01.x(), p01.y()}, color, 2);
            draw.AddLine({p00.x(), p00.y()}, {p10.x(), p10.y()}, color, 2);
            draw.AddLine({p01.x(), p01.y()}, {p11.x(), p11.y()}, color, 2);
            draw.AddLine({p10.x(), p10.y()}, {p11.x(), p11.y()}, color, 2);
        }
    }

    void draw_ui(app& a, float width) override
    {

    }

    void update_pre(app& a) override
    {

    }

    void update_post(app& a) override
    {
        if (pending.exists)
        {
            pending.exists = false;
            if (pending.from.chunk3().z != pending.to.chunk3().z)
            {
                fm_warn("raycast: wrong Z value");
                return;
            }
            if (pending.from == pending.to)
            {
                fm_warn("raycast: from == to");
                return;
            }

            do_raycasting(a, pending.from, pending.to);
        }
    }

    void do_raycasting(app& a, point from, point to)
    {
        constexpr auto tile_size = Vector2d{iTILE_SIZE2};
        constexpr auto chunk_size = Vector2d{TILE_MAX_DIM} * tile_size;
        constexpr double eps = 1e-6;
        constexpr double inv_eps = 1/eps;
        constexpr double sqrt_2 = Math::sqrt(2.);
        constexpr double inv_sqrt_2 = 1. / sqrt_2;

        result.has_result = false;

        auto vec = Vector2d{};
        vec += (Vector2d(to.chunk()) - Vector2d(from.chunk())) * chunk_size;
        vec += (Vector2d(to.local()) - Vector2d(from.local())) * tile_size;
        vec += (Vector2d(to.offset()) - Vector2d(from.offset()));

        auto dir = vec.normalized();

        if (Math::abs(dir.x()) < eps && Math::abs(dir.y()) < eps)
        {
            fm_error("raycast: bad dir? {%f, %f}", dir.x(), dir.y());
            return;
        }

        double step;
        unsigned long_axis, short_axis;

        if (Math::abs(dir.y()) > Math::abs(dir.x()))
        {
            long_axis = 1;
            short_axis = 0;
        }
        else
        {
            long_axis = 0;
            short_axis = 1;
        }

        if (Math::abs(dir[short_axis]) < eps)
            step = chunk_size.x() * .5;
        else
        {
            constexpr double numer = inv_sqrt_2 * tile_size.x();
            step = Math::abs(numer / dir[short_axis]);
            step = Math::clamp(step, 1., chunk_size.x()*.5);
            //Debug{} << "step" << step;
        }

        Vector2d v;
        v[long_axis] = std::copysign(step, vec[long_axis]);
        v[short_axis] = std::copysign(Math::max(1., Math::min(tile_size.x(), Math::abs(vec[short_axis]))), vec[short_axis]);
        auto size = Vector2ui(Math::abs(v));
        const auto half = Vector2i(v*.5);

        auto nsteps = (uint32_t)Math::ceil(Math::abs(vec[long_axis] / step));

        result.path.clear();
        result.path.reserve(nsteps);
        result.has_result = true;

        {
            //Debug{} << "vec" << vec;
            auto c = object::normalize_coords(from, half);
            //Debug{} << "c" << c << "size" << size;
            result.path.push_back(bbox{c, size});
        }

        size[short_axis] += 2;

        for (auto i = 1u; i < nsteps; i++)
        {
            auto u = Vector2i(vec * i/(double)nsteps);
            u[short_axis] -= 1;
            auto pt = object::normalize_coords(from, half + u);
            result.path.push_back(bbox{pt, size});
        }

        //Debug{} << "path len" << result.path.size();

        auto dir_inv_norm = Vector2d{
            Math::abs(dir.x()) < eps ? std::copysign(inv_eps, dir.x()) : 1. / dir.x(),
            Math::abs(dir.y()) < eps ? std::copysign(inv_eps, dir.y()) : 1. / dir.y(),
        };
        auto signs = ray_aabb_signs(dir_inv_norm);
    }
};

raycast_test::~raycast_test() noexcept = default;

Pointer<base_test> tests_data::make_test_raycast() { return Pointer<raycast_test>{InPlaceInit}; }

} // namespace floormat::tests
