#include "src/raycast-diag.hpp"
#include "src/raycast.hpp"
#include "src/collision.hpp"
#include "src/pass-mode.hpp"
#include "src/point.inl"
#include "src/tile-constants.hpp"
#include "src/world.hpp"
#include "src/wall-atlas.hpp"
#include "src/RTree-search.hpp"
#include "loader/loader.hpp"
#include <benchmark/benchmark.h>
#include <cfloat>
#include <bit>
#include <cr/StructuredBindings.h>
#include <cr/GrowableArray.h>
#include <mg/Functions.h>
#include <mg/Timeline.h>

namespace floormat {

namespace {

world make_world()
{
    constexpr auto var = (variant_t)-1;
    auto wall1_ = loader.wall_atlas("test1"_s);
    auto wall2_ = loader.wall_atlas("concrete1"_s);
    auto wall1 = wall_image_proto{wall1_, var};
    auto wall2 = wall_image_proto{wall2_, var};

    auto w = world{};
    w[{0, 3, 0}][{15,  0}].wall_north() = wall1;
    w[{1, 3, 0}][{ 0,  0}].wall_north() = wall1;
    w[{1, 3, 0}][{ 0,  0}].wall_north() = wall1;
    w[{1, 2, 0}][{ 1, 15}].wall_west()  = wall1;
    w[{1, 2, 0}][{ 1, 14}].wall_west()  = wall1;

    w[{0, 1, 0}][{ 8, 11}].wall_west()  = wall2;
    w[{0, 1, 0}][{ 8, 10}].wall_west()  = wall2;
    w[{0, 1, 0}][{ 7, 10}].wall_north() = wall2;
    w[{0, 1, 0}][{ 6, 10}].wall_north() = wall2;

    w[{0, 1, 0}][{ 9,  8}].wall_north() = wall1;
    w[{0, 1, 0}][{10,  8}].wall_north() = wall1;
    w[{0, 1, 0}][{11,  8}].wall_west()  = wall1;

    w[{0, 2, 0}][{ 9,  0}].wall_north() = wall1;
    w[{0, 2, 0}][{10,  0}].wall_north() = wall1;

    for (int16_t k = -5; k <= -1; k++)
    {
        auto& ch = w[{-5, -5, 0}];
        for (unsigned i = 0; i < TILE_MAX_DIM; i++)
        {
            ch[{(uint8_t)i, 0}].wall_west()  = wall1;
            ch[{(uint8_t)i, 1}].wall_north() = wall1;
            ch[{(uint8_t)i, 2}].wall_north() = wall2;
            ch[{(uint8_t)i, 2}].wall_west()  = wall2;
        }
    }

    for (int16_t i = -15; i <= 15; i++)
        for (int16_t j = -15; j <= 15; j++)
            w[{{i, j}, 0}].mark_modified();

    return w;
}

auto run(point from, point to, world& w, bool b, float len)
{
    constexpr float fuzz = TILE_SIZE2.x();
    auto diag = rc::raycast_diag_s{};
    auto res = raycast_with_diag(diag, w, from, to, 0);
    if (res.success != b)
    {
        fm_error("success != %s", b ? "true" : "false");
        return false;
    }
    if (len != 0.f)
    {
        auto tmin = res.success ? diag.V.length() : diag.tmin;
        auto diff = Math::abs(tmin - len);
        if (diff > fuzz)
        {
            fm_error("|tmin=%f - len=%f| > %f",
                     (double)tmin, (double)len, (double)fuzz);
            return false;
        }
    }
    return true;
}

void Raycast(benchmark::State& state)
{
    auto w = make_world();

    const auto test = [&] {
      { constexpr auto from = point{{0, 0, 0}, {11,12}, {1,-32}};
        fm_assert(run(from, point{{  1,   3, 0}, { 0,  1}, {-21,  23}}, w, false,  2288));
        fm_assert(run(from, point{{  1,   3, 0}, { 8, 10}, {- 9, -13}}, w, true,   3075));
        fm_assert(run(from, point{{  0,   3, 0}, {14,  4}, {  3,  15}}, w, true,   2614));
        fm_assert(run(from, point{{  0,   1, 0}, { 8, 12}, {-27, -19}}, w, false,   752));
        fm_assert(run(from, point{{  2,  33, 0}, {15, 11}, {- 4,  29}}, w, true,  33809));
        fm_assert(run(from, point{{  0,   1, 0}, { 6, 13}, {- 3, -11}}, w, false,   913));
      }
      { fm_assert(run(      point{{  0,   0, 0}, { 1,  0}, {-17,  17}},
                            point{{  0, - 7, 0}, { 1, 15}, {-11,   5}}, w, true,   6220));
      }
    };

    for (int i = 0; i < 3; i++)
        test();
    for (auto _ : state)
        test();
}

BENCHMARK(Raycast)->Unit(benchmark::kMicrosecond);

world make_dense_world()
{
    constexpr auto var = (variant_t)-1;
    auto wall1_ = loader.wall_atlas("test1"_s);
    auto wall2_ = loader.wall_atlas("concrete1"_s);
    auto wall1  = wall_image_proto{wall1_, var};
    auto wall2  = wall_image_proto{wall2_, var};

    auto w = world{};

    constexpr int16_t cmin = -10, cmax = 10;

    auto hash2 = [](uint32_t x, uint32_t y) -> uint32_t {
        uint32_t h = x * 0x9E3779B1u + y * 0x85EBCA77u;
        h ^= h >> 16; h *= 0xC2B2AE3Du;
        h ^= h >> 13; h *= 0x27D4EB2Du;
        h ^= h >> 16;
        return h;
    };

    for (int16_t cx = cmin; cx <= cmax; cx++)
        for (int16_t cy = cmin; cy <= cmax; cy++)
        {
            auto& ch = w[{cx, cy, 0}];
            for (uint8_t lx = 0; lx < 16; lx++)
                for (uint8_t ly = 0; ly < 16; ly++)
                {
                    uint32_t fx = (uint32_t)(cx + 16) * 16 + lx;
                    uint32_t fy = (uint32_t)(cy + 16) * 16 + ly;

                    if ((hash2(fx,           fy ^ 0xA5A5u) & 0x1Fu) == 0u)
                        ch[{lx, ly}].wall_north() = wall1;
                    if ((hash2(fx ^ 0x5A5Au, fy)           & 0x1Fu) == 0u)
                        ch[{lx, ly}].wall_west()  = wall2;
                }
            ch.mark_modified();
        }

    return w;
}

point project_to(point from, point to, int extra_tiles)
{
    auto seg = to - from;
    auto len_sq = (float)(seg.x()*(int64_t)seg.x() + seg.y()*(int64_t)seg.y());
    if (len_sq < 1.f) return to;
    auto len = Math::sqrt(len_sq);
    auto step_px = (float)tile_size_xy * (float)extra_tiles;
    auto dx = (int)((float)seg.x() * step_px / len);
    auto dy = (int)((float)seg.y() * step_px / len);
    return point::normalize_coords(to, Vector2i{dx, dy});
}

bool run_dense(point from, point to_orig, world& w)
{
    auto seg = to_orig - from;
    auto seg_len = Math::sqrt((float)(seg.x()*(int64_t)seg.x() + seg.y()*(int64_t)seg.y()));
    if (seg_len < 3.f * (float)(tile_size_xy * TILE_MAX_DIM))
    {
        fm_error("ray length %.0f px < 3 chunks", (double)seg_len);
        return false;
    }
    auto to = project_to(from, to_orig, 2);
    auto res = raycast(w, from, to, 0);
    if (res.success)
    {
        fm_error("ray did not hit (success=true means no collision)");
        return false;
    }
    if ((collision_type)res.collider.type != collision_type::geometry)
    {
        fm_error("ray hit non-geometry collider type=%d", (int)res.collider.type);
        return false;
    }
    return true;
}

void Raycast_Dense(benchmark::State& state)
{
    auto w = make_dense_world();

    const auto test = [&] {
        fm_assert(run_dense(point{{ 0,  0, 0}, { 0,  0}, {  0,   0}},
                            point{{ 3,  4, 0}, {11,  5}, {-25, -11}}, w));
        fm_assert(run_dense(point{{-1, -1, 0}, {13,  8}, { -2,  17}},
                            point{{-1,-10, 0}, { 6, 12}, { 24,  29}}, w));
        fm_assert(run_dense(point{{ 0,  0, 0}, { 9, 12}, {-25,   2}},
                            point{{ 0,  7, 0}, {12, 14}, { 27, -13}}, w));
        fm_assert(run_dense(point{{ 3,  0, 0}, { 8,  7}, { -7,  -4}},
                            point{{ 7,  4, 0}, { 1,  7}, {-26,  26}}, w));
        fm_assert(run_dense(point{{ 0,  0, 0}, { 0,  0}, {  0,   0}},
                            point{{-6, -5, 0}, {15,  4}, {-15,  23}}, w));
    };

    for (int i = 0; i < 3; i++) test();
    for (auto _ : state) test();
}

BENCHMARK(Raycast_Dense)->Unit(benchmark::kMicrosecond);

namespace old_rc {

using rc::raycast_result_s;
using rc::raycast_diag_s;

template<typename T> constexpr inline auto tile_size = Math::Vector2<T>{iTILE_SIZE2};
template<typename T> constexpr inline auto chunk_size = Math::Vector2<T>{TILE_MAX_DIM} * tile_size<T>;

using floormat::detail_rc::bbox;
using RTree = std::decay_t<decltype(*std::declval<class chunk>().rtree())>;
using Rect = typename RTree::Rect;

static_assert(tile_size<int>.x() == tile_size<int>.y());

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
constexpr bool within_chunk_bounds(Math::Vector2<T> p0, Math::Vector2<T> p1)
{
    constexpr auto max_bb_size = Math::Vector2<T>{T{0xff}, T{0xff}};
    constexpr auto half_bb = (max_bb_size + Math::Vector2{T{1}}) / T{2};
    constexpr auto start = -half_bb, end = chunk_size<T> + half_bb;

    return start.x() <= p1.x() && end.x() >= p0.x() &&
           start.y() <= p1.y() && end.y() >= p0.y();
}

raycast_result_s do_raycasting_old(world& w, point from, point to, object_id self)
{
    raycast_result_s result;
    fm_assert(from.chunk3().z == to.chunk3().z);

    using Math::max;
    using Math::min;
    using Math::abs;
    using Math::ceil;
    using Math::copysign;

    constexpr auto inv_eps = 1e6f, eps = 1/inv_eps;
    constexpr int fuzz = 2;
    constexpr auto fuzz2 = 0.5f;

    result.has_result = false;

    auto V = pt_to_vec(from, to);
    auto ray_len = V.length();

    if (ray_len < eps)
    {
        result = {
            .from = from,
            .to = to,
            .collision = {},
            .collider = {
                .type = (uint64_t)collision_type::none,
                .pass = (uint64_t)pass_mode::pass,
                .id   = ((uint64_t)1 << collision_data_BITS)-1,
            },
            .has_result = true,
            .success = true,
        };
        return result;
    }
    auto dir = V * (1.f/ray_len);

    unsigned major_axis, minor_axis;

    if (abs(dir.y()) > abs(dir.x()))
    {
        major_axis = 1;
        minor_axis = 0;
    }
    else
    {
        major_axis = 0;
        minor_axis = 1;
    }

    auto major_len = max(1u, (unsigned)ceil(abs(V[major_axis]))),
         minor_len = max(1u, (unsigned)ceil(abs(V[minor_axis])));
    auto nsteps = 1u;
    nsteps = max(nsteps, (minor_len + tile_size<unsigned>.x()-1)/tile_size<unsigned>.x());
    nsteps = max(nsteps, (major_len + chunk_size<unsigned>.x()-1)/chunk_size<unsigned>.x());
    auto size_ = Vector2ui{};
    size_[minor_axis] = (minor_len + nsteps*2 - 1) / nsteps;
    size_[major_axis] = (major_len + nsteps - 1) / nsteps;

    auto dir_inv_norm = Vector2{1} / dir;
    auto signs = ray_aabb_signs(dir_inv_norm);

    result = {
        .from = from,
        .to = to,
        .collision = {},
        .collider = {
            .type = (uint64_t)collision_type::none,
            .pass = (uint64_t)pass_mode::pass,
            .id   = ((uint64_t)1 << collision_data_BITS)-1,
        },
        .has_result = true,
        .success = false,
    };

    float min_tmin = FLT_MAX;
    bool b = true;

    auto last_ch = from.chunk3();
    std::array<std::array<chunk*, 3>, 3> nbs = {};
    std::array<std::array<bool, 3>, 3> nbs_done = {};

    for (unsigned k = 0; b && k <= nsteps; k++)
    {
        auto pos_ = ceil(abs(V * (float)k/(float)nsteps));
        auto pos = Vector2i(copysign(pos_, V));
        auto size = size_;

        if (k == 0)
        {
            for (auto axis : { major_axis, minor_axis })
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
            if (size[major_axis] > min_size)
            {
                auto sign = sign_<int>(V[major_axis]);
                auto off = (int)(size[major_axis]/2) - (int)add;
                fm_debug_assert(off >= 0);
                pos[major_axis] -= off/2 * sign;
                size[major_axis] -= (unsigned)off;
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
            if (x.id == self || x.pass == (uint64_t)pass_mode::pass)
                return;
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
                result.collision = point::normalize_coords(from, Vector2i(dir * min_tmin));
                result.collider = x;
                b = false;
            }
        };

        auto center = point::normalize_coords(from, pos);

        if (center.chunk3() != last_ch) [[unlikely]]
        {
            last_ch = center.chunk3();
            nbs = {};
        }

        auto pt = Vector2i(center.local()) * tile_size<int> + Vector2i(center.offset());

        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < 3; j++)
            {
                if (nbs_done[(unsigned)i][(unsigned)j])
                    continue;
                auto*& c = nbs[(unsigned)i][(unsigned)j];
                auto off = chunk_offsets[i][j];
                auto pt0 = pt - Vector2i(size/2), pt1 = pt0 + Vector2i(size);
                auto pt0_ = pt0 - off, pt1_ = pt1 - off;
                auto [fmin, fmax] = Math::minmax(Vector2(pt0_)-Vector2(fuzz2), Vector2(pt1_)+Vector2(fuzz2));
                if (!within_chunk_bounds(fmin, fmax))
                    continue;
                if (!c)
                {
                    c = w.chunk_at_memo({last_ch + Vector2i{i - 1, j - 1}});
                    if (!c)
                    {
                        nbs_done[(unsigned)i][(unsigned)j] = true;
                        continue;
                    }
                }

                auto ch_off = (center.chunk() - from.chunk() + Vector2i(i-1, j-1)) * chunk_size<int>;
                origin = Vector2((Vector2i(from.local()) * tile_size<int>) + Vector2i(from.offset()) - ch_off);
                auto* r = c->rtree();
                r->Search(fmin.data(), fmax.data(), [&](uint64_t data, const Rect& r) {
                    do_check_collider(data, r);
                    return true;
                });
            }
        }
    }
    result.success = b;
    return result;
}

raycast_result_s raycast_old(world& w, point from, point to, object_id self)
{
    Timeline timeline;
    timeline.start();
    auto ret = do_raycasting_old(w, from, to, self);
    ret.time = timeline.currentFrameDuration();
    return ret;
}

} // namespace old_rc

bool run_dense_old(point from, point to_orig, world& w)
{
    auto to = project_to(from, to_orig, 2);
    auto res = old_rc::raycast_old(w, from, to, 0);
    if (res.success)
        { fm_error("ray did not hit"); return false; }
    if ((collision_type)res.collider.type != collision_type::geometry)
        { fm_error("ray hit non-geometry collider type=%d", (int)res.collider.type); return false; }
    return true;
}

void Raycast_Dense_Old(benchmark::State& state)
{
    auto w = make_dense_world();

    const auto test = [&] {
        fm_assert(run_dense_old(point{{ 0,  0, 0}, { 0,  0}, {  0,   0}},
                                point{{ 3,  4, 0}, {11,  5}, {-25, -11}}, w));
        fm_assert(run_dense_old(point{{-1, -1, 0}, {13,  8}, { -2,  17}},
                                point{{-1,-10, 0}, { 6, 12}, { 24,  29}}, w));
        fm_assert(run_dense_old(point{{ 0,  0, 0}, { 9, 12}, {-25,   2}},
                                point{{ 0,  7, 0}, {12, 14}, { 27, -13}}, w));
        fm_assert(run_dense_old(point{{ 3,  0, 0}, { 8,  7}, { -7,  -4}},
                                point{{ 7,  4, 0}, { 1,  7}, {-26,  26}}, w));
        fm_assert(run_dense_old(point{{ 0,  0, 0}, { 0,  0}, {  0,   0}},
                                point{{-6, -5, 0}, {15,  4}, {-15,  23}}, w));
    };

    for (int i = 0; i < 3; i++) test();
    for (auto _ : state) test();
}

BENCHMARK(Raycast_Dense_Old)->Unit(benchmark::kMicrosecond);

} // namespace

} // namespace floormat
