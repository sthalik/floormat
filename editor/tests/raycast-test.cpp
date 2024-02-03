#include "../tests-private.hpp"
#include "editor/app.hpp"
#include "floormat/main.hpp"
#include "compat/shared-ptr-wrapper.hpp"
#include "../imgui-raii.hpp"
#include "src/critter.hpp"
#include "src/world.hpp"
#include "src/RTree-search.hpp"
#include <cinttypes>
#include <array>
#include <vector>
#include <Corrade/Containers/StructuredBindings.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Color.h>

namespace floormat::tests {

namespace {

using namespace imgui;

template<typename T> constexpr inline auto tile_size = Math::Vector2<T>{iTILE_SIZE2};
template<typename T> constexpr inline auto chunk_size = Math::Vector2<T>{TILE_MAX_DIM} * tile_size<T>;

using RTree = std::decay_t<decltype(*std::declval<struct chunk>().rtree())>;
using Rect = typename RTree::Rect;

constexpr Vector2d pt_to_vec(point from, point pt)
{
    auto V = Vector2d{};
    V += (Vector2d(pt.chunk()) - Vector2d(from.chunk())) * chunk_size<double>;
    V += (Vector2d(pt.local()) - Vector2d(from.local())) * tile_size<double>;
    V += (Vector2d(pt.offset()) - Vector2d(from.offset()));
    return V;
}

struct aabb_result
{
    float tmin;
    bool result;
};

template<typename T>
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

    float tmin = 0, tmax = 16777216;

    for (unsigned d = 0; d < 2; ++d)
    {
        auto bmin = box_minmax[signs[d]][d];
        auto bmax = box_minmax[!signs[d]][d];
        float dmin = (bmin - ray_origin[d]) * ray_dir_inv_norm[d];
        float dmax = (bmax - ray_origin[d]) * ray_dir_inv_norm[d];

        tmin = max(dmin, tmin);
        tmax = min(dmax, tmax);
    }

    return { tmin, tmin < tmax };
}

struct bbox
{
    point center;
    Vector2ui size;
};

struct diag_s
{
    Vector2d vec, v;
    double step;
};

struct result_s
{
    point from, to, collision;
    collision_data collider;
    diag_s diag;
    std::vector<bbox> path;
    bool has_result : 1 = false,
         success    : 1 = false;
};

struct pending_s
{
    point from, to;
    object_id self;
    bool exists : 1 = false;
};

struct chunk_neighbors
{
    chunk* array[3][3];
};

auto get_chunk_neighbors(class world& w, chunk_coords_ ch)
{
    chunk_neighbors nbs;
    for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++)
            nbs.array[i][j] = w.at(ch - Vector2i(i - 1, j - 1));
    return nbs;
}

constexpr Vector2i chunk_offsets[3][3] = {
    {
        { -chunk_size<int>.x(), -chunk_size<int>.y()    },
        { -chunk_size<int>.x(),  0                      },
        { -chunk_size<int>.x(),  chunk_size<int>.y()    },
    },
    {
        { 0,                    -chunk_size<int>.y()    },
        { 0,                     0                      },
        { 0,                     chunk_size<int>.y()    },
    },
    {
        {  chunk_size<int>.x(), -chunk_size<int>.y()    },
        {  chunk_size<int>.x(),  0                      },
        {  chunk_size<int>.x(),  chunk_size<int>.y()    },
    },
};

template<typename T>
constexpr bool within_chunk_bounds(Math::Vector2<T> vec)
{
    constexpr auto max_bb_size = Math::Vector2<T>{T{0xff}, T{0xff}};
    return vec.x() >= -max_bb_size.x() && vec.x() < chunk_size<T>.x() + max_bb_size.x() &&
           vec.y() >= -max_bb_size.y() && vec.y() < chunk_size<T>.y() + max_bb_size.y();
}

//static_assert(chunk_offsets[0][0] == Vector2i(-1024, -1024));
//static_assert(chunk_offsets[2][0] == Vector2i(1024, -1024));

} // namespace

struct raycast_test : base_test
{
    result_s result;
    pending_s pending;

    ~raycast_test() noexcept override;

    bool handle_key(app&, const key_event&, bool) override
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
                pending = { .from = pt0, .to = *pt_, .self = C->id, .exists = true, };
                return true;
            }
        }

        return false;
    }

    bool handle_mouse_move(app& a, const mouse_move_event& e) override
    {
        if (e.buttons & mouse_button_left)
            return handle_mouse_click(a, {e.position, e.mods, mouse_button_left, 1}, true);
        return true;
    }

    void draw_overlay(app& a) override
    {
        if (!result.has_result)
            return;

        const auto color = ImGui::ColorConvertFloat4ToU32({1, 0, 0, 1}),
                   color2 = ImGui::ColorConvertFloat4ToU32({1, 0, 0.75, 1}),
                   color3 = ImGui::ColorConvertFloat4ToU32({0, 0, 1, 1});
        ImDrawList& draw = *ImGui::GetForegroundDrawList();

        {
            auto p0 = a.point_screen_pos(result.from),
                 p1 = a.point_screen_pos(result.success
                                         ? object::normalize_coords(result.from, Vector2i(result.diag.vec))
                                         : result.collision);
            draw.AddLine({p0.x(), p0.y()}, {p1.x(), p1.y()}, color2, 2);
        }

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

        if (!result.success)
        {
            auto p = a.point_screen_pos(result.collision);
            draw.AddCircleFilled({p.x(), p.y()}, 10, color3);
            draw.AddCircleFilled({p.x(), p.y()}, 7, color);
        }
    }

    void draw_ui(app&, float) override
    {
        constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
        constexpr auto colflags_1 = ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder |
                                    ImGuiTableColumnFlags_NoSort;
        constexpr auto colflags_0 = colflags_1 | ImGuiTableColumnFlags_WidthFixed;

        constexpr auto print_coord = [](auto&& buf, Vector3i c, Vector2i l, Vector2i p)
        {
          std::snprintf(buf, std::size(buf), "(ch %dx%d) <%dx%d> {%dx%d px}", c.x(), c.y(), l.x(), l.y(), p.x(), p.y());
        };

        constexpr auto print_vec2 = [](auto&& buf, Vector2d vec)
        {
          std::snprintf(buf, std::size(buf), "(%.2f x %.2f)", vec.x(), vec.y());
        };

        constexpr auto do_column = [](StringView name)
        {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          text(name);
          ImGui::TableNextColumn();
        };

        if (!result.has_result)
            return;

        if (auto b1 = begin_table("##raycast-results", 2, table_flags))
        {
            ImGui::TableSetupColumn("##name", colflags_0);
            ImGui::TableSetupColumn("##value", colflags_1 | ImGuiTableColumnFlags_WidthStretch);

            char buf[128];
            auto from_c = Vector3i(result.from.chunk3()), to_c = Vector3i(result.to.chunk3());
            auto from_l = Vector2i(result.from.local()), to_l = Vector2i(result.to.local());
            auto from_p = Vector2i(result.from.offset()), to_p = Vector2i(result.to.offset());

            do_column("from");
            print_coord(buf, from_c, from_l, from_p);
            text(buf);

            do_column("to");
            print_coord(buf, to_c, to_l, to_p);
            text(buf);

            if (result.success)
            {
                do_column("collision");
                text("-");
                do_column("collider");
                text("-");
            }
            else
            {
                const char* type;

                switch ((collision_type)result.collider.tag)
                {
                using enum collision_type;
                default: type = "unknown?!"; break;
                case none: type = "none?!"; break;
                case object: type = "object"; break;
                case scenery: type = "scenery"; break;
                case geometry: type = "geometry"; break;
                }

                do_column("collision");
                auto C_c = Vector3i(result.from.chunk3());
                auto C_l = Vector2i(result.from.local());
                auto C_p = Vector2i(result.from.offset());
                print_coord(buf, C_c, C_l, C_p);
                { auto b = push_style_color(ImGuiCol_Text, 0xffff00ff_rgbaf);
                  text(buf);
                }

                do_column("collider");
                std::snprintf(buf, std::size(buf), "%s @ %" PRIu64,
                              type, uint64_t{result.collider.data});
                { auto b = push_style_color(ImGuiCol_Text, 0xffff00ff_rgbaf);
                  text(buf);
                }
            }

            do_column("num-steps");
            std::snprintf(buf, std::size(buf), "%zu", result.path.size());
            text(buf);

            do_column("vector");
            print_vec2(buf, result.diag.vec);
            text(buf);

            do_column("step");
            print_vec2(buf, result.diag.v);
            text(buf);
        }
    }

    void update_pre(app&) override
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

            do_raycasting(a, pending.from, pending.to, pending.self);
        }
    }

    void do_raycasting(app& a, point from, point to, object_id self)
    {
        constexpr double eps = 1e-6;
        constexpr double inv_eps = 1/eps;
        constexpr double sqrt_2 = Math::sqrt(2.);
        constexpr double inv_sqrt_2 = 1. / sqrt_2;
        constexpr int fuzz = 2;

        result.has_result = false;

        auto& w = a.main().world();
        auto V = pt_to_vec(from, to);
        auto ray_len = (float)V.length();
        auto dir = V.normalized();

        if (Math::abs(dir.x()) < eps && Math::abs(dir.y()) < eps)
        {
            fm_error("raycast: bad dir? {%f, %f}", dir.x(), dir.y());
            return;
        }

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

        double long_len  = Math::abs(V[long_axis]), short_len = Math::abs(V[short_axis]);
        auto short_steps = Math::max(1u, (unsigned)(Math::ceil((short_len + tile_size<double>.x()) / iTILE_SIZE2.x())));
        auto long_steps  = Math::max(1u, (unsigned)(Math::ceil(long_len)/tile_size<double>.x()));
        auto nsteps = Math::min(short_steps, long_steps)+1u;

        result = {
            .from = from,
            .to = to,
            .collision = {},
            .collider = {
                .tag  = (uint64_t)collision_type::none,
                .pass = (uint64_t)pass_mode::pass,
                .data = ((uint64_t)1 << collision_data_BITS)-1,
            },
            .diag = {
                .vec  = V,
                .v    = {},
                .step = 0,
            },
            .path = {},
            .has_result = true,
            .success = false,
        };

        Debug{} << "------";
        for (unsigned k = 0; k <= nsteps; k++)
        {
            auto pos_ = Math::ceil(Math::abs(V * (double)k / (double)nsteps));
            auto pos = Vector2i{(int)std::copysign(pos_.x(), V.x()), (int)std::copysign(pos_.y(), V.y())};
            auto size = Vector2ui(iTILE_SIZE2);
            size[long_axis] = Math::max(tile_size<unsigned>.x(), (unsigned)Math::ceil(long_len / nsteps));

            if (k == 0)
            {
                pos[long_axis] += (int)(size[long_axis]/4) * (V[long_axis] < 0 ? -1 : 1);
                size[long_axis] -= size[long_axis]/2;
                pos[short_axis] += (int)(size[short_axis]/4) * (V[short_axis] < 0 ? -1 : 1);
                size[short_axis] -= size[short_axis]/2;
            }

            pos -= Vector2i(fuzz);
            size += Vector2ui(fuzz)*2;

            auto pt = object::normalize_coords(from, pos);
            result.path.push_back(bbox{pt, size});
        }

        auto last_ch = from.chunk3();
        auto nbs = get_chunk_neighbors(w, from.chunk3());

        auto dir_inv_norm = Vector2(
            Math::abs(dir.x()) < eps ? (float)std::copysign(inv_eps, dir.x()) : 1 / (float)dir.x(),
            Math::abs(dir.y()) < eps ? (float)std::copysign(inv_eps, dir.y()) : 1 / (float)dir.y()
        );
        auto signs = ray_aabb_signs(dir_inv_norm);
        Vector2 origin;
        float min_tmin = FLT_MAX;
        bool b = true;

        const auto do_check_collider = [&](uint64_t data, const Rect& r)
        {
            auto x = std::bit_cast<collision_data>(data);
            if (x.data == self || x.pass == (uint64_t)pass_mode::pass)
                return true;
            Debug{} << "item" << x.data << Vector2(r.m_min[0], r.m_min[1]);
            auto ret = ray_aabb_intersection(origin, dir_inv_norm,
                                           {{{r.m_min[0], r.m_min[1]},{r.m_max[0], r.m_max[1]}}},
                                           signs);
            if (!ret.result || ret.tmin > ray_len)
                return true;
            if (ret.tmin < min_tmin)
            {
                min_tmin = ret.tmin;
                result.collision = object::normalize_coords(from, Vector2i(dir * (double)ret.tmin));
                result.collider = x;
                b = false;
            }
            return true;
        };

        for (unsigned i = 0; i < result.path.size(); i++)
        {
            auto [center, size] = result.path[i];
            if (center.chunk3() != last_ch)
            {
                last_ch = center.chunk3();
                nbs = get_chunk_neighbors(w, center.chunk3());
            }

            auto pt = Vector2i(center.local()) * tile_size<int> + Vector2i(center.offset());

            for (int i = 0; i < 3; i++)
            {
                for (int j = 0; j < 3; j++)
                {
                    auto* c = nbs.array[i][j];
                    if (!c)
                        continue;
                    auto* r = c->rtree();
                    auto off = chunk_offsets[i][j];
                    auto pt0 = pt - Vector2i(size/2), pt1 = pt0 + Vector2i(size);
                    auto pt0_ = pt0 - off, pt1_ = pt1 - off;
                    if (!within_chunk_bounds(pt0_) && !within_chunk_bounds(pt1_))
                        continue;
                    auto [fmin, fmax] = Math::minmax(Vector2(pt0 - off), Vector2(pt1 - off));
                    auto ch_off = (chunk_coords(last_ch) - from.chunk()) * chunk_size<int>;
                    origin = Vector2((Vector2i(from.local()) * tile_size<int>) + Vector2i(from.offset()) - ch_off);
                    r->Search(fmin.data(), fmax.data(), [&](uint64_t data, const Rect& r) {
                        return do_check_collider(data, r);
                    });
                }
            }
        }

        result.success = b;
#if 0
        if (Math::abs(dir[short_axis]) < eps)
            step = chunk_size<double>.x() * .5;
        else
        {
            constexpr double max_len = chunk_size<double>.x()/2;
            constexpr double numer = inv_sqrt_2 * tile_size<double>.x();
            step = Math::clamp(Math::round(Math::abs(numer / dir[short_axis])), 1., max_len);
            //Debug{} << "step" << step;
        }

        Vector2d v;
        v[long_axis] = std::copysign(step, V[long_axis]);
        v[short_axis] = std::copysign(Math::clamp(Math::abs(V[short_axis]), 1., tile_size<double>.x()), V[short_axis]);

        auto nsteps = (uint32_t)Math::max(1., Math::ceil(Math::abs(V[long_axis] / step)));

        auto size = Vector2ui{};
        size[long_axis] = (unsigned)Math::ceil(step);
        size[short_axis] = (unsigned)Math::ceil(Math::abs(v[short_axis]));
        const auto half = Vector2i(v*.5);

        result = {
            .from = from,
            .to = to,
            .collision = {},
            .collider = {
                .tag  = (uint64_t)collision_type::none,
                .pass = (uint64_t)pass_mode::pass,
                .data = ((uint64_t)1 << collision_data_BITS)-1,
            },
            .diag = {
                .vec  = V,
                .v    = v,
                .step = step,
            },
            .path = {},
            .has_result = true,
            .success = false,
        };

        //result.path.clear();
        result.path.reserve(nsteps);

        size[short_axis] += (unsigned)(fuzz * 2);
        auto half_size = Vector2i(size/2);

        auto last_ch = from.chunk3();
        auto nbs = get_chunk_neighbors(w, from.chunk3());

        auto dir_inv_norm = Vector2(
            Math::abs(dir.x()) < eps ? (float)std::copysign(inv_eps, dir.x()) : 1 / (float)dir.x(),
            Math::abs(dir.y()) < eps ? (float)std::copysign(inv_eps, dir.y()) : 1 / (float)dir.y()
        );
        auto signs = ray_aabb_signs(dir_inv_norm);

        Debug{} << "----";

        const auto do_check_collider = [&](Vector2 origin, uint64_t data, const Rect& r, bool& b)
        {
            auto x = std::bit_cast<collision_data>(data);
            if (x.data == self || x.pass == (uint64_t)pass_mode::pass)
                return true;
            Debug{} << "item" << x.data << Vector2(r.m_min[0], r.m_min[1]);
            auto ret = ray_aabb_intersection(origin, dir_inv_norm,
                                           {{{r.m_min[0], r.m_min[1]},{r.m_max[0], r.m_max[1]}}},
                                           signs);
            if (!ret.result || ret.tmin > ray_len)
                return true;
            result.collision = object::normalize_coords(from, Vector2i(dir * (double)ret.tmin));
            result.collider = x;
            return b = false;
        };

        for (auto k = 0u; k < nsteps; k++)
        {
            auto u = Vector2i(Math::round(V * k/(double)nsteps));
            u[short_axis] -= fuzz;
            auto pt = object::normalize_coords(from, half + u);

            result.path.push_back(bbox{pt, size});

            if (pt.chunk3() != last_ch)
            {
                last_ch = pt.chunk3();
                nbs = get_chunk_neighbors(w, pt.chunk3());
            }

            auto center = Vector2i(pt.local()) * tile_size<int> + Vector2i(pt.offset());

            for (int i = 0; i < 3; i++)
            {
                for (int j = 0; j < 3; j++)
                {
                    auto* c = nbs.array[i][j];
                    if (!c)
                        continue;
                    auto off = chunk_offsets[i][j];
                    if (!within_chunk_bounds(center - off))
                        continue;
                    auto* r = c->rtree();
                    auto pt0 = center - Vector2i(half_size), pt1 = pt0 + Vector2i(size);
                    auto [fmin, fmax] = Math::minmax(Vector2(pt0 - off), Vector2(pt1 - off));
                    bool b = true;
                    auto ch_off = (chunk_coords(last_ch) - from.chunk()) * chunk_size<int>;
                    auto origin = Vector2((Vector2i(from.local()) * tile_size<int>) + Vector2i(from.offset()) - ch_off);
                    r->Search(fmin.data(), fmax.data(), [&](uint64_t data, const Rect& r) {
                        return do_check_collider(origin, data, r, b);
                    });
                    if (!b)
                        goto last;
                }
            }
        }
        result.success = true;
        return;
last:
        void();
#endif
    }
};

raycast_test::~raycast_test() noexcept = default;

Pointer<base_test> tests_data::make_test_raycast() { return Pointer<raycast_test>{InPlaceInit}; }

} // namespace floormat::tests
