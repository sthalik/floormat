#include "raycast-diag.hpp"
#include "pass-mode.hpp"
#include "src/world.hpp"
#include "src/object.hpp"
#include "src/RTree-search.hpp"
#include <float.h>
#include <Corrade/Containers/StructuredBindings.h>
#include <Corrade/Containers/GrowableArray.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Vector2.h>

namespace floormat::rc {

namespace {

template<typename T> constexpr inline auto tile_size = Math::Vector2<T>{iTILE_SIZE2};
template<typename T> constexpr inline auto chunk_size = Math::Vector2<T>{TILE_MAX_DIM} * tile_size<T>;

using floormat::detail_rc::bbox;
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

template<bool EnableDiagnostics>
raycast_result_s do_raycasting(std::conditional_t<EnableDiagnostics, raycast_diag_s&, std::nullptr_t> diag,
                       world& w, point from, point to, object_id self)
{
    raycast_result_s result;
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
        diag = {
            .V = V,
            .dir = dir,
            .dir_inv_norm = dir_inv_norm,
            .size = size_,
            .tmin = 0,
        };
        arrayResize(diag.path, 0);
        arrayReserve(diag.path, nsteps+1);
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
            arrayAppend(diag.path, bbox{center, size});

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
        diag.tmin = b ? 0 : min_tmin;
    result.success = b;
    return result;
}

} // namespace

raycast_result_s raycast(world& w, point from, point to, object_id self)
{
    return do_raycasting<false>(nullptr, w, from, to, self);
}

raycast_result_s raycast_with_diag(raycast_diag_s& diag, world& w, point from, point to, object_id self)
{
    return do_raycasting<true>(diag, w, from, to, self);
}

} // namespace floormat::rc
