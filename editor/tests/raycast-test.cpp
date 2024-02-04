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

template<class T> constexpr inline T sign_(auto&& x) {
    constexpr auto zero = std::decay_t<decltype(x)>{0};
    return T(x > zero) - T(x < zero);
}

constexpr Vector2 pt_to_vec(point from, point pt)
{
    auto V = Vector2{};
    V += (Vector2(pt.chunk()) - Vector2(from.chunk())) * chunk_size<float>;
    V += (Vector2(pt.local()) - Vector2(from.local())) * tile_size<float>;
    V += (Vector2(pt.offset()) - Vector2(from.offset()));
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
    std::vector<bbox> path{};
    Vector2 V, dir, dir_inv_norm;
    Vector2ui size;
    //unsigned short_steps, long_steps;
    float tmin;
};

struct result_s
{
    point from, to, collision;
    collision_data collider;
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
            nbs.array[i][j] = w.at(ch + Vector2i(i - 1, j - 1));
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
//static_assert(chunk_offsets[0][0] == Vector2i(-1024, -1024));
//static_assert(chunk_offsets[2][0] == Vector2i(1024, -1024));

template<typename T>
constexpr bool within_chunk_bounds(Math::Vector2<T> p0, Math::Vector2<T> p1)
{
    constexpr auto max_bb_size = Math::Vector2<T>{T{0xff}, T{0xff}};
    constexpr auto half_bb = (max_bb_size + Math::Vector2{T{1}}) / T{2};
    constexpr auto start = -half_bb, end = chunk_size<T> + half_bb;

    return !(start.x() > p1.x() || end.x() < p0.x() ||
             start.y() > p1.y() || end.y() < p0.y());
}
template bool within_chunk_bounds<int>(Math::Vector2<int> p0, Math::Vector2<int> p1);

void print_coord(auto&& buf, Vector3i c, Vector2i l, Vector2i p)
{
    std::snprintf(buf, std::size(buf), "(ch %dx%d) <%dx%d> {%dx%d px}", c.x(), c.y(), l.x(), l.y(), p.x(), p.y());
}

void print_coord_(auto&& buf, point pt)
{
    auto C_c = Vector3i(pt.chunk3());
    auto C_l = Vector2i(pt.local());
    auto C_p = Vector2i(pt.offset());
    print_coord(buf, C_c, C_l, C_p);
}

void print_vec2(auto&& buf, Vector2 vec)
{
    std::snprintf(buf, std::size(buf), "(%.2f x %.2f)", (double)vec.x(), (double)vec.y());
}

void do_column(StringView name)
{
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  text(name);
  ImGui::TableNextColumn();
}

template<bool EnableDiagnostics>
result_s do_raycasting(std::conditional_t<EnableDiagnostics, diag_s*, std::nullptr_t> diag,
                       app& a, point from, point to, object_id self)
{
    result_s result;
    if constexpr(EnableDiagnostics)
        fm_assert(diag != nullptr);
    fm_assert(from.chunk3().z == to.chunk3().z);

    using Math::max;
    using Math::min;
    using Math::abs;
    using Math::ceil;

    constexpr float eps = 1e-6f;
    constexpr float inv_eps = 1/eps;
    constexpr int fuzz = 2;
    constexpr auto fuzz2 = 0.5f;

    result.has_result = false;

    auto& w = a.main().world();
    auto V = pt_to_vec(from, to);
    auto ray_len = V.length();
    auto dir = V.normalized();

    if (abs(dir.x()) < eps && abs(dir.y()) < eps) [[unlikely]]
        dir = {eps, eps};

    unsigned long_axis, short_axis;

    if (abs(dir.y()) > abs(dir.x()))
    {
        long_axis = 1;
        short_axis = 0;
    }
    else
    {
        long_axis = 0;
        short_axis = 1;
    }

    auto long_len  = max(1u, (unsigned)ceil(abs(V[long_axis]))),
         short_len = max(1u, (unsigned)ceil(abs(V[short_axis])));
    auto nsteps = 1u;
    nsteps = max(nsteps, (short_len+tile_size<unsigned>.x()-1)/tile_size<unsigned>.x());
    nsteps = max(nsteps, (long_len+chunk_size<unsigned>.x()-1)/chunk_size<unsigned>.x());
    auto size_ = Vector2ui{};
    size_[short_axis] = (short_len+nsteps*2-1) / nsteps;
    size_[long_axis]  = (long_len+nsteps-1) / nsteps;

    auto dir_inv_norm = Vector2(
        abs(dir.x()) < eps ? std::copysign(inv_eps, dir.x()) : 1 / dir.x(),
        abs(dir.y()) < eps ? std::copysign(inv_eps, dir.y()) : 1 / dir.y()
    );
    auto signs = ray_aabb_signs(dir_inv_norm);

    result = {
        .from = from,
        .to = to,
        .collision = {},
        .collider = {
            .tag  = (uint64_t)collision_type::none,
            .pass = (uint64_t)pass_mode::pass,
            .data = ((uint64_t)1 << collision_data_BITS)-1,
        },
        .has_result = true,
        .success = false,
    };
    if constexpr(EnableDiagnostics)
    {
        *diag = {
            .V = V,
            .dir = dir,
            .dir_inv_norm = dir_inv_norm,
            .size = size_,
            .tmin = 0,
        };
        diag->path.clear();
        diag->path.reserve(nsteps+1);
    }

    float min_tmin = FLT_MAX;
    bool b = true;

    //Debug{} << "------";
    for (unsigned k = 0; b && k <= nsteps; k++)
    {
        auto pos_ = ceil(abs(V * (float)k/(float)nsteps));
        auto pos = Vector2i{(int)std::copysign(pos_.x(), V.x()), (int)std::copysign(pos_.y(), V.y())};
        auto size = size_;

        if (k == 0)
        {
            for (auto axis : { long_axis, short_axis })
            {
                auto sign = sign_<int>(V[axis]);
                pos[axis] += (int)(size[axis]/4) * sign;
                size[axis] -= size[axis]/2;
            }
        }
        else if (k == nsteps)
        {
            constexpr auto add = (tile_size<unsigned>.x()+1)/2,
                           min_size = tile_size<unsigned>.x() + add;
            if (size[long_axis] > min_size)
            {
                auto sign = sign_<int>(V[long_axis]);
                auto off = (int)(size[long_axis]/2) - (int)add;
                fm_debug_assert(off >= 0);
                pos[long_axis] -= off/2 * sign;
                size[long_axis] -= (unsigned)off;
            }
        }

        pos -= Vector2i(fuzz);
        size += Vector2ui(fuzz)*2;

        Vector2 origin;
        min_tmin = FLT_MAX;
        b = true;

        const auto do_check_collider = [&](uint64_t data, const Rect& r)
        {
            auto x = std::bit_cast<collision_data>(data);
            if (x.data == self || x.pass == (uint64_t)pass_mode::pass)
                return;
            //Debug{} << "item" << x.data << Vector2(r.m_min[0], r.m_min[1]) << Vector2(r.m_max[0], r.m_max[1]);
            auto ret = ray_aabb_intersection(origin, dir_inv_norm,
                                             {{{r.m_min[0]-fuzz2, r.m_min[1]-fuzz2},
                                               {r.m_max[0]+fuzz2, r.m_max[1]+fuzz2}}},
                                             signs);
            if (!ret.result)
                return;
            if (ret.tmin > ray_len) [[unlikely]]
                return;
            if (ret.tmin < min_tmin) [[likely]]
            {
                min_tmin = ret.tmin;
                result.collision = object::normalize_coords(from, Vector2i(dir * min_tmin));
                result.collider = x;
                b = false;
            }
        };

        auto center = object::normalize_coords(from, pos);

        if constexpr(EnableDiagnostics)
            diag->path.push_back(bbox{center, size});

        auto last_ch = from.chunk3();
        auto nbs = get_chunk_neighbors(w, from.chunk3());

        if (center.chunk3() != last_ch) [[unlikely]]
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
                auto [fmin, fmax] = Math::minmax(Vector2(pt0_)-Vector2(fuzz2), Vector2(pt1_)+Vector2(fuzz2));
                if (!within_chunk_bounds(fmin, fmax)) continue;
                auto ch_off = (center.chunk() - from.chunk() + Vector2i(i-1, j-1)) * chunk_size<int>;
                origin = Vector2((Vector2i(from.local()) * tile_size<int>) + Vector2i(from.offset()) - ch_off);
                //Debug{} << "search" << fmin << fmax << Vector3i(c->coord());
                r->Search(fmin.data(), fmax.data(), [&](uint64_t data, const Rect& r) {
                    do_check_collider(data, r);
                    return true;
                });
            }
        }
    }
    if constexpr(EnableDiagnostics)
        diag->tmin = b ? 0 : min_tmin;
    result.success = b;
    return result;
}

} // namespace

struct raycast_test : base_test
{
    result_s result;
    pending_s pending;
    diag_s diag;

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
                                         ? object::normalize_coords(result.from, Vector2i(diag.V))
                                         : result.collision);
            draw.AddLine({p0.x(), p0.y()}, {p1.x(), p1.y()}, color2, 2);
        }

        for (auto [center, size] : diag.path)
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
        else
        {
            auto color4 = ImGui::ColorConvertFloat4ToU32({0, 1, 0, 1});
            auto p = a.point_screen_pos(result.to);
            draw.AddCircleFilled({p.x(), p.y()}, 10, color3);
            draw.AddCircleFilled({p.x(), p.y()}, 7, color4);
        }
    }

    void draw_ui(app&, float) override
    {
        constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
        constexpr auto colflags_1 = ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder |
                                    ImGuiTableColumnFlags_NoSort;
        constexpr auto colflags_0 = colflags_1 | ImGuiTableColumnFlags_WidthFixed;

        if (!result.has_result)
            return;

        if (auto b1 = begin_table("##raycast-results", 2, table_flags))
        {
            ImGui::TableSetupColumn("##name", colflags_0);
            ImGui::TableSetupColumn("##value", colflags_1 | ImGuiTableColumnFlags_WidthStretch);

            char buf[128];

            do_column("from");
            print_coord_(buf, result.from);
            text(buf);

            do_column("to");
            print_coord_(buf, result.to);
            text(buf);

            ImGui::NewLine();

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
                print_coord_(buf, result.collision);
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

            ImGui::NewLine();

            do_column("dir");
            std::snprintf(buf, std::size(buf), "%.4f x %.4f", (double)diag.dir.x(), (double)diag.dir.y());
            text(buf);

            if (!result.success)
            {
                do_column("tmin");
                std::snprintf(buf, std::size(buf), "%f / %f",
                              (double)diag.tmin,
                              (double)(diag.tmin / diag.V.length()));
                text(buf);
            }
            else
            {
                do_column("tmin");
                text("-");
            }

            do_column("vector");
            print_vec2(buf, diag.V);
            text(buf);

            do_column("||dir^-1||");
            std::snprintf(buf, std::size(buf), "%f x %f",
                          (double)diag.dir_inv_norm.x(),
                          (double)diag.dir_inv_norm.y());
            text(buf);

            ImGui::NewLine();

            do_column("bbox-size");
            std::snprintf(buf, std::size(buf), "(%u x %u)", diag.size.x(), diag.size.y());
            text(buf);

            do_column("path-len");
            std::snprintf(buf, std::size(buf), "%zu", diag.path.size());
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
            result = do_raycasting<true>(&diag, a, pending.from, pending.to, pending.self);
        }
    }
};

raycast_test::~raycast_test() noexcept = default;

Pointer<base_test> tests_data::make_test_raycast() { return Pointer<raycast_test>{InPlaceInit}; }

} // namespace floormat::tests
