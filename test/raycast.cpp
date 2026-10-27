#include "app.hpp"
#include "compat/array-size.hpp"
#include "compat/borrowed-ptr.inl"
#include "src/tile-constants.hpp"
#include "src/raycast-diag.hpp"
#include "src/raycast-loop.hpp"
#include "src/raycast-line.hpp"
#include "src/scenery-proto.hpp"
#include "src/point.inl"
#include "src/intra-coord.inl"
#include "src/grid-pass.hpp"
#include "src/world.hpp"
#include "src/critter.hpp"
#include "src/hole.hpp"
#include "src/wall-atlas.hpp"
#include "loader/loader.hpp"
#include "loader/wall-cell.hpp"
#include "loader/scenery-cell.hpp"
#include <cfloat>
#include <cr/GrowableArray.h>
#include <mg/Functions.h>

namespace floormat {

namespace {

world make_world()
{
    constexpr auto var = (variant_t)-1;
#if 1
    auto wall1_ = loader.invalid_wall_atlas().atlas;
    auto wall2_ = loader.invalid_wall_atlas().atlas;
#else
    auto wall1_ = loader.wall_atlas("test1"_s);
    auto wall2_ = loader.wall_atlas("concrete1"_s);
#endif
    auto wall1 = wall_image_proto{wall1_, var};
    auto wall2 = wall_image_proto{wall2_, var};

    auto w = world{};

    const auto tile = [&w](global_coords pt) {
        auto& c = w[pt.chunk3()];
        return c[pt.local()];
    };

    tile(global_coords{{0, 3, 0}, {15,  0}}).wall_north() = wall1;
    tile(global_coords{{1, 3, 0}, { 0,  0}}).wall_north() = wall1;
    tile(global_coords{{1, 3, 0}, { 0,  0}}).wall_north() = wall1;
    tile(global_coords{{1, 2, 0}, { 1, 15}}).wall_west()  = wall1;
    tile(global_coords{{1, 2, 0}, { 1, 14}}).wall_west()  = wall1;

    tile(global_coords{{0, 1, 0}, { 8, 11}}).wall_west()  = wall2;
    tile(global_coords{{0, 1, 0}, { 8, 10}}).wall_west()  = wall2;
    tile(global_coords{{0, 1, 0}, { 7, 10}}).wall_north() = wall2;
    tile(global_coords{{0, 1, 0}, { 6, 10}}).wall_north() = wall2;

    tile(global_coords{{0, 1, 0}, { 9,  8}}).wall_north() = wall1;
    tile(global_coords{{0, 1, 0}, {10,  8}}).wall_north() = wall1;
    tile(global_coords{{0, 1, 0}, {11,  8}}).wall_west()  = wall1;

    tile(global_coords{{0, 2, 0}, { 9,  0}}).wall_north() = wall1;
    tile(global_coords{{0, 2, 0}, {10,  0}}).wall_north() = wall1;

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

auto run(Pass::Pool& pool, point from, point to, world& w, bool b, float len)
{
    constexpr float fuzz = TILE_SIZE2.x();
    auto diag = rc::raycast_diag_s{};
    auto res = raycast_with_diag(diag, w, from, to, 0, pool, Search::never_continue());
    const auto div = (int)pool.params().div_size();
    for (const auto& q : diag.queries)
        fm_assert(Vector2i(intra_coord{q.center}) % div == Vector2i{div / 2});
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

void test_window(pass_mode hole_pass)
{
    constexpr auto ch = chunk_coords_{0, 0, 0};
    constexpr auto pos = global_coords{ch, {8, 8}};
    const auto W = wall_image_proto{ loader.wall_atlas("empty"), 0 };
    const auto mid_y = (int8_t)(-tile_size_xy/2 - (int)W.atlas->info().depth/2);

    auto w = world();
    auto& c = w[ch];
    c[pos.local()].wall_north() = W;
    hole_proto hp;
    hp.offset = {0, -tile_size_xy/2};
    hp.bbox_size = {32, 32};
    hp.pass = hole_pass;
    (void)w.make_object<hole>(w.make_id(), pos, hp);

    Pass::Pool shoot{Pass::Params{8, 8, pass_mode::shoot_through}, Search::never_continue()};
    Pass::Pool walk{Pass::Params{8, 8, pass_mode::pass}, Search::never_continue()};
    const auto open_at = [&](Pass::Pool& pool, int8_t x) {
        pool.maybe_mark_stale_all(w);
        auto g = pool[c];
        g.build_if_stale();
        return g.bit(g.get_bitmask_index_from_coord(intra_coord{pos.local(), {x, mid_y}}));
    };
    const auto ray_passes = [&](Pass::Pool& pool, int8_t x) {
        return raycast(w, point{ch, {8, 6}, {x, 0}}, point{ch, {8, 10}, {x, 0}}, 0,
                       pool, Search::never_continue()).success;
    };

    const bool shoots = hole_pass == pass_mode::shoot_through;
    fm_assert(open_at(shoot, 0) == shoots);
    fm_assert(!open_at(walk, 0));
    fm_assert(!open_at(shoot, -24));
    fm_assert(ray_passes(shoot, 0) == shoots);
    fm_assert(!ray_passes(walk, 0));
    fm_assert(!ray_passes(shoot, -24));
}

void test_edit_in_same_frame()
{
    constexpr auto ch = chunk_coords_{0, 0, 0};
    auto w = world();
    auto& c = w[ch];
    Pass::Pool pool{Pass::Params{8, 8, pass_mode::pass}, Search::never_continue()};
    const auto frame = w.frame_no();
    const auto ray_passes = [&] {
        return raycast(w, point{ch, {2, 8}, {}}, point{ch, {14, 8}, {}}, 0,
                       pool, Search::never_continue()).success;
    };

    fm_assert(ray_passes());
    c[{8, 8}].wall_west() = wall_image_proto{loader.invalid_wall_atlas().atlas, (variant_t)-1};
    c.mark_passability_modified();
    fm_assert(w.frame_no() == frame);
    fm_assert(!ray_passes());
}

void test_rays(const Pass::Params& params, world& w)
{
    Pass::Pool pool{params, Search::never_continue()};
    const auto ray = [&](world& w, point from, point to, object_id self) {
        return raycast(w, from, to, self, pool, Search::never_continue());
    };
    const auto ray_diag = [&](rc::raycast_diag_s& diag, world& w, point from, point to, object_id self) {
        return raycast_with_diag(diag, w, from, to, self, pool, Search::never_continue());
    };

    { constexpr auto from = point{{0, 0, 0}, {11,12}, {1,-32}};
      fm_assert(run(pool, from, point{{  1,   3, 0}, { 0,  1}, {-21,  23}}, w, false,  2288));
      fm_assert(run(pool, from, point{{  1,   3, 0}, { 8, 10}, {- 9, -13}}, w, true,   3075));
      fm_assert(run(pool, from, point{{  0,   3, 0}, {14,  4}, {  3,  15}}, w, true,   2614));
      fm_assert(run(pool, from, point{{  0,   1, 0}, { 8, 12}, {-27, -19}}, w, false,   752));
      fm_assert(run(pool, from, point{{  2,  33, 0}, {15, 11}, {- 4,  29}}, w, true,  33809));
      fm_assert(run(pool, from, point{{  0,   1, 0}, { 6, 13}, {- 3, -11}}, w, false,   913));
    }
    { fm_assert(run(pool, point{{  0,   0, 0}, { 1,  0}, {-17,  17}},
                          point{{  0, - 7, 0}, { 1, 15}, {-11,   5}}, w, true,   6220));
    }
    {
        constexpr auto p = point{
            {1, 2, 3},
            {4, 5},
            {6, 7},
        };
        run(pool, p, p, w, true, 0);
    }
    {
        constexpr auto from = point{{0, 0, 0}, {11, 12}, {1, -32}};
        auto diag = rc::raycast_diag_s{};
        (void)ray_diag(diag, w, from, point{{0, 1, 0}, {8, 12}, {-27, -19}}, 0);
        fm_assert(!diag.path.isEmpty());
        (void)ray_diag(diag, w, from, from, 0);
        fm_assert(diag.V.isZero() && diag.path.isEmpty() && diag.queries.isEmpty());
    }
    {
        constexpr auto from = point{{0, 0, 0}, {11, 12}, {1, -32}};
        auto diag = rc::raycast_diag_s{};
        (void)ray_diag(diag, w, from, point{{2, 33, 0}, {15, 11}, {-4, 29}}, 0);
        const auto path_cap = arrayCapacity(diag.path), queries_cap = arrayCapacity(diag.queries);
        (void)ray_diag(diag, w, from, point{{0, 0, 0}, {12, 12}, {1, -32}}, 0);
        fm_assert(arrayCapacity(diag.path) >= path_cap && arrayCapacity(diag.queries) >= queries_cap);
    }
    {   // the nearest collider must win even when a farther rect's entry into an
        // early cell puts it into the result before the walk reaches the near one
        auto w2 = world{};
        constexpr auto ch = chunk_coords_{8, 8, 0};

        auto decoy = critter_proto{};
        decoy.bbox_offset = Vector2b{0, 50};
        decoy.bbox_size = Vector2ub{240, 44};
        (void)w2.make_object<critter>(w2.make_id(), {ch, {5, 2}}, decoy);

        auto blocker = critter_proto{};
        blocker.offset = Vector2b{-17, 18};
        blocker.bbox_size = Vector2ub{22, 12};
        auto B = w2.make_object<critter>(w2.make_id(), {ch, {4, 2}}, blocker);

        auto diag = rc::raycast_diag_s{};
        auto res = ray_diag(diag, w2, point{ch, {2, 2}, {}}, point{ch, {12, 4}, {}}, 0);
        fm_assert(res.has_result);
        fm_assert(!res.success);
        fm_assert(res.collider.id == B->id);
        fm_assert(res.rect == Range2D::fromCenter(Vector2{4*64 - 17, 2*64 + 18}, Vector2{11, 6}));
        fm_assert(Math::abs(diag.tmin - 101.5f) < 16);
    }
    {   // an entry rect reaches offset + bbox_offset + bbox_size/2 past the owner
        // chunk's edge, farther than the neighbor cull's old 128px margin
        auto w2 = world{};
        constexpr auto ch = chunk_coords_{8, 10, 0};
        constexpr auto ch2 = chunk_coords_{9, 10, 0};
        (void)w2[ch2];

        auto p = critter_proto{};
        p.offset = Vector2b{31, 0};
        p.bbox_offset = Vector2b{127, 0};
        p.bbox_size = Vector2ub{200, 60};
        auto C = w2.make_object<critter>(w2.make_id(), {ch, {15, 8}}, p);

        auto res = ray(w2, point{ch2, {3, 1}, {-2, 0}}, point{ch2, {3, 12}, {-2, 0}}, 0);
        fm_assert(res.has_result);
        fm_assert(!res.success);
        fm_assert(res.collider.id == C->id);
        fm_assert(res.rect == Range2D::fromCenter(Vector2{15*64 + 31 + 127 - chunk_size<int>, 8*64}, Vector2{100, 30}));
    }
    {
        auto w2 = world{};
        constexpr auto ch = chunk_coords_{8, 12, 0};

        auto p = critter_proto{};
        p.bbox_size = Vector2ub{42, 26};
        auto C = w2.make_object<critter>(w2.make_id(), {ch, {8, 5}}, p);

        auto diag = rc::raycast_diag_s{};
        auto fwd = ray_diag(diag, w2, point{ch, {2, 5}, {}}, point{ch, {13, 5}, {}}, 0);
        fm_assert(diag.dir.y() == 0);
        fm_assert(!fwd.success);
        fm_assert(fwd.collider.id == C->id);
        fm_assert(Math::abs(diag.tmin - 362.5f) < 1);

        auto back = ray_diag(diag, w2, point{ch, {13, 5}, {}}, point{ch, {2, 5}, {}}, 0);
        fm_assert(!back.success);
        fm_assert(back.collider.id == C->id);
        fm_assert(Math::abs(diag.tmin - 299.5f) < 1);

        auto top = ray_diag(diag, w2, point{ch, {2, 5}, {0, -13}}, point{ch, {13, 5}, {0, -13}}, 0);
        fm_assert(top.collider.id == C->id);
        fm_assert(Math::abs(diag.tmin - 362.5f) < 1);

        auto bottom = ray_diag(diag, w2, point{ch, {2, 5}, {0, 12}}, point{ch, {13, 5}, {0, 12}}, 0);
        fm_assert(bottom.collider.id == C->id);
        fm_assert(Math::abs(diag.tmin - 362.5f) < 1);

        fm_assert(ray(w2, point{ch, {2, 5}, {0,  13}}, point{ch, {13, 5}, {0,  13}}, 0).success);
        fm_assert(ray(w2, point{ch, {2, 5}, {0, -14}}, point{ch, {13, 5}, {0, -14}}, 0).success);
    }
    {
        auto w2 = world{};
        constexpr auto ch = chunk_coords_{8, 14, 0};

        auto p = critter_proto{};
        p.bbox_size = Vector2ub{22, 34};
        auto C = w2.make_object<critter>(w2.make_id(), {ch, {5, 8}}, p);

        auto diag = rc::raycast_diag_s{};
        auto fwd = ray_diag(diag, w2, point{ch, {5, 1}, {}}, point{ch, {5, 14}, {}}, 0);
        fm_assert(diag.dir.x() == 0);
        fm_assert(!fwd.success);
        fm_assert(fwd.collider.id == C->id);
        fm_assert(Math::abs(diag.tmin - 430.5f) < 1);

        auto back = ray_diag(diag, w2, point{ch, {5, 14}, {}}, point{ch, {5, 1}, {}}, 0);
        fm_assert(!back.success);
        fm_assert(back.collider.id == C->id);
        fm_assert(Math::abs(diag.tmin - 367.5f) < 1);

        auto left = ray_diag(diag, w2, point{ch, {5, 1}, {-11, 0}}, point{ch, {5, 14}, {-11, 0}}, 0);
        fm_assert(left.collider.id == C->id);
        fm_assert(Math::abs(diag.tmin - 430.5f) < 1);

        auto right = ray_diag(diag, w2, point{ch, {5, 1}, {10, 0}}, point{ch, {5, 14}, {10, 0}}, 0);
        fm_assert(right.collider.id == C->id);
        fm_assert(Math::abs(diag.tmin - 430.5f) < 1);

        fm_assert(ray(w2, point{ch, {5, 1}, { 11, 0}}, point{ch, {5, 14}, { 11, 0}}, 0).success);
        fm_assert(ray(w2, point{ch, {5, 1}, {-12, 0}}, point{ch, {5, 14}, {-12, 0}}, 0).success);
    }
    {   // the 42x26 critter's pixel squares [-21.5, 20.5]x[-13.5, 12.5], grown by a circle
        auto w2 = world{};
        constexpr auto ch = chunk_coords_{8, 16, 0};

        auto p = critter_proto{};
        p.bbox_size = Vector2ub{42, 26};
        auto C = w2.make_object<critter>(w2.make_id(), {ch, {8, 5}}, p);
        const auto P0 = point{ch, {8, 5}, {}};

        // y = -20 and 19 are skipped at 6.5 px: the circle only grazes the rect there
        constexpr struct { float radius; int hit_lo, hit_hi, miss_lo, miss_hi; } cases[] = {
            {6, -19, 18, -20, 19}, {6.5f, -19, 18, -21, 20},
        };
        auto diag = rc::raycast_diag_s{};
        for (auto [radius, hit_lo, hit_hi, miss_lo, miss_hi] : cases)
        {
            if (rc::min_pool_bbox_size(radius) > pool.params().bbox_size)
                continue;

            const auto ray_r = [&](Vector2i from, Vector2i to) {
                return raycast_with_diag(diag, w2, P0 + from, P0 + to, 0, pool, Search::never_continue(), radius);
            };

            for (int y = hit_lo; y <= hit_hi; y++)
            {
                auto res = ray_r({-384, y}, {320, y});
                fm_assert(!res.success);
                fm_assert(res.collider.id == C->id);
                const float dy = Math::max(0.f, Math::max(-13.5f - (float)y, (float)y - 12.5f));
                fm_assert(Math::abs(diag.tmin - (384 - 21.5f - Math::sqrt(radius*radius - dy*dy))) < 1e-3f);
                fm_assert(res.collision == P0 + Vector2i{-21, Math::clamp(y, -13, 12)});
                fm_assert(res.rect == Range2D::fromCenter(Vector2{8*64, 5*64}, Vector2{21, 13}));
            }
            fm_assert(ray_r({-384, miss_lo}, {320, miss_lo}).success);
            fm_assert(ray_r({-384, miss_hi}, {320, miss_hi}).success);

            // past the corner (20.5, 12.5) diagonally: 5.66 px away hits, 7.07 px misses
            // though it crosses the rect grown by the square
            auto res = ray_r({-59, 100}, {100, -59});
            fm_assert(!res.success);
            fm_assert(res.collider.id == C->id);
            fm_assert(res.collision == P0 + Vector2i{20, 12});
            fm_assert(ray_r({-57, 100}, {100, -57}).success);
        }
    }
}

struct pillar
{
    object_id id;
    Vector2i min, max;
    Vector2i shift;
};

// Its rect in origin's frame, as scenery_tile() computes it.
pillar place_pillar(world& w, chunk_coords_ ch, chunk_coords_ origin, local_coords tile, Vector2b bb_off, Vector2ub bb_size)
{
    scenery_proto p;
    p.atlas       = loader.invalid_scenery_atlas().proto->atlas;
    p.subtype     = generic_scenery_proto{};
    p.bbox_offset = bb_off;
    p.bbox_size   = bb_size;
    p.pass        = pass_mode::blocked;
    const auto id = w.make_id();
    w.make_scenery(id, {ch, tile}, move(p));
    const auto min = (point{ch, tile, {}} - point{origin, {}, {}}) + Vector2i(bb_off) - Vector2i(bb_size)/2;
    return { id, min, min + Vector2i(bb_size), point{ch, {}, {}} - point{origin, {}, {}} };
}

bool contains(const pillar& p, Vector2i px)
{
    return (px >= p.min).all() && (px < p.max).all();
}

struct supercover_hit
{
    Vector2i pixels[2];
    uint32_t count;
    bool off_axis;
};

// The first supercover pixels inside a pillar, walked pixel by pixel. Two when both off-axis
// pixels at a corner are.
supercover_hit first_supercover_hit(ArrayView<const pillar> pillars, const rc::bresenham_line& line)
{
    const auto blocked = [&](Vector2i px) {
        for (const auto& p : pillars)
            if (contains(p, px))
                return true;
        return false;
    };
    for (auto k = 0u; k <= line.steps(); k++)
    {
        if (blocked(line.pixel_at(k)))
            return { { line.pixel_at(k), {} }, 1, false };
        if (k == line.steps())
            break;
        supercover_hit ret = {};
        if (const auto o = line.off_axis_at(k); o.diagonal)
            for (auto i = 0u; i < 2; i++)
                if (o.crossed[i] && blocked(o.pixels[i]))
                    ret.pixels[ret.count++] = o.pixels[i];
        ret.off_axis = ret.count > 0;
        if (ret.count)
            return ret;
    }
    return {};
}

// In each pillar's chunk frame with raycast()'s float operations, since near tangency the corner
// test loses more than 1e-3 px to the frame's magnitude.
float nearest_rounded_hit(ArrayView<const pillar> pillars, Vector2i from, Vector2 V, float radius)
{
    using namespace rc::detail;
    const auto len = V.length();
    const auto dir = V * (1.f/len), inv = dir_inverse(dir);
    float best = FLT_MAX;
    for (const auto& p : pillars)
    {
        const auto o = Vector2(from) - Vector2(p.shift);
        const auto r = ray_rounded_rect_intersection(o, dir, inv, {{Vector2(p.min - p.shift) - Vector2{.5f},
                                                                    Vector2(p.max - p.shift) - Vector2{.5f}}},
                                                     radius, ray_aabb_signs(inv));
        if (r.result && r.tmin <= len)
            best = Math::min(best, r.tmin);
    }
    return best;
}

void test_supercover_hits()
{
    auto w = world{};
    constexpr chunk_coords_ ch{0, 0, 0}, ch2{1, 0, 0}, ch3{1, 1, 0};
    const pillar pillars[] = {
        place_pillar(w, ch,  ch, {3, 4},  Vector2b{0, 0},    Vector2ub{20, 12}),
        place_pillar(w, ch,  ch, {8, 9},  Vector2b{-5, 7},   Vector2ub{10, 30}),
        place_pillar(w, ch,  ch, {15, 6}, Vector2b{20, 0},   Vector2ub{30, 8}),
        place_pillar(w, ch2, ch, {1, 10}, Vector2b{-3, 11},  Vector2ub{14, 6}),
        place_pillar(w, ch2, ch, {0, 15}, Vector2b{-28, 20}, Vector2ub{6, 8}),
        place_pillar(w, ch3, ch, {2, 1},  Vector2b{0, 0},    Vector2ub{18, 24}),
        place_pillar(w, ch,  ch, {5, 12}, Vector2b{0, 0},    Vector2ub{2, 10}),
        place_pillar(w, ch,  ch, {11, 2}, Vector2b{3, -4},   Vector2ub{8, 2}),
        // the line (100, 100)-(300, 300) touches it only at the corner (150.5, 150.5)
        place_pillar(w, ch,  ch, {2, 2},  Vector2b{24, 22},  Vector2ub{2, 2}),
        // (0, 0)-(400, 1) has its pixels at y = 0 up to x = 199 while the segment rises toward
        // y = 0.5, so at radius 2.25 this is hit 2.75 to 3.25 px from the pixels
        place_pillar(w, ch,  ch, {2, 0},  Vector2b{55, 7},   Vector2ub{6, 8}),
        // at radius 2.25 and div 16, (0, 0)-(640, 66) finds the wide one in cell x 48-63 and hits it at
        // x 70.3, after the small one at x 65.3, which only the next cell finds
        place_pillar(w, ch,  ch, {2, 0},  Vector2b{43, 45},  Vector2ub{255, 70}),
        place_pillar(w, ch,  ch, {1, 0},  Vector2b{5, 7},    Vector2ub{2, 2}),
        place_pillar(w, ch,  ch, {6, 13}, Vector2b{0, 0},    Vector2ub{13, 7}),
        place_pillar(w, ch,  ch, {12, 12}, Vector2b{-3, 5},  Vector2ub{9, 21}),
        place_pillar(w, ch,  ch, {10, 5}, Vector2b{0, 0},    Vector2ub{1, 1}),
        place_pillar(w, ch,  ch, {13, 9}, Vector2b{2, -1},   Vector2ub{3, 6}),
        place_pillar(w, ch2, ch, {3, 6},  Vector2b{1, -2},   Vector2ub{25, 4}),
        // min (410, 483). (100, 479)-(500, 480) stays below y = 480, where cell rows end, and passes
        // 2.6 to 2.75 px under its pixel squares, so radius 2.75 hits it only from a query grown by 3.
        place_pillar(w, ch,  ch, {7, 8},  Vector2b{-18, -24}, Vector2ub{40, 10}),
    };
    for (auto c : { ch, ch2, ch3 })
        w[c].mark_modified();

    Pass::Pool pools[] = {
        Pass::Pool{Pass::Params{4, 16, pass_mode::pass}, Search::never_continue()},
        Pass::Pool{Pass::Params{8, 16, pass_mode::pass}, Search::never_continue()},
        Pass::Pool{Pass::Params{16, 16, pass_mode::pass}, Search::never_continue()},
    };
    const auto origin = point{ch, {}, {}};
    uint32_t hits = 0, off_axis_hits = 0;

    const auto check = [&](Vector2i from, Vector2i to) {
        const auto line = rc::bresenham_line{from, to};
        const auto ref = first_supercover_hit(pillars, line);
        hits += ref.count > 0;
        off_axis_hits += ref.off_axis;
        auto diag = rc::raycast_diag_s{};
        for (auto& pool : pools)
        {
            const auto res = raycast_with_diag(diag, w, origin + from, origin + to, 0, pool, Search::never_continue());
            const auto px = res.collision - origin;
            bool ok = res.success == !ref.count;
            if (ok && ref.count)
            {
                ok = px == ref.pixels[0] || (ref.count == 2 && px == ref.pixels[1]);
                const auto shift = Vector2(point{res.from.chunk3(), {}, {}} - origin);
                bool in_collider = false;
                for (const auto& p : pillars)
                    if (p.id == (object_id)res.collider.id)
                        in_collider |= contains(p, px) && res.rect == Range2D{Vector2(p.min) - shift, Vector2(p.max) - shift};
                ok &= in_collider;
            }
            if (!ok)
                fm_abort("ray (%d %d)-(%d %d) div %u: success %d at (%d %d), expected %u pixels from (%d %d)",
                         from.x(), from.y(), to.x(), to.y(), pool.params().div_size(), (int)res.success,
                         px.x(), px.y(), ref.count, ref.pixels[0].x(), ref.pixels[0].y());

            for (auto radius : { .5f, 2.25f, 2.75f, 6.5f })
            {
                const auto expected = nearest_rounded_hit(pillars, from, Vector2(to - from), radius);
                const auto r = raycast_with_diag(diag, w, origin + from, origin + to, 0, pool, Search::never_continue(), radius);
                if (r.success != (expected == FLT_MAX) || (!r.success && Math::abs(diag.tmin - expected) > 1e-3f))
                    fm_abort("ray (%d %d)-(%d %d) div %u radius %g: success %d tmin %g, expected %g",
                             from.x(), from.y(), to.x(), to.y(), pool.params().div_size(), (double)radius,
                             (int)r.success, (double)diag.tmin, (double)expected);
            }
        }
    };

    check({100, 100}, {300, 300});
    check({0, 0}, {400, 1});
    check({0, 0}, {640, 66});
    check({100, 479}, {500, 480});
    fm_assert(!raycast(w, origin + Vector2i{100, 100}, origin + Vector2i{300, 300}, 0, pools[0], Search::never_continue()).success);
    // through the corner of four chunks
    check({950, 950}, {1040, 1040});
    check({1040, 950}, {950, 1040});

    uint32_t seed = 0x6c8e9cf5;
    const auto next = [&](uint32_t n) {
        seed = seed * 1664525u + 1013904223u;
        return (int32_t)((seed >> 8) % n);
    };
    for (auto i = 0u; i < 144; i++)
    {
        const auto from = Vector2i{next(1300) - 32, next(1300) - 32};
        const auto& p = pillars[next((uint32_t)array_size(pillars))];
        const auto size = p.max - p.min;
        const auto corner = Vector2i{next(2), next(2)} * size;
        const auto aim = i % 3 == 1 ? p.min + Vector2i{next((uint32_t)size.x() + 7), next((uint32_t)size.y() + 7)} - Vector2i{3}
                                    : p.min + corner + Vector2i{next(5), next(5)} - Vector2i{2};
        const auto to = i % 3 ? aim + (aim - from) / 4 : from + Vector2i{next(601) - 300, next(601) - 300};
        check(from, to);
    }
    fm_assert(hits >= 64 && off_axis_hits >= 16);
}

static_assert(rc::min_pool_bbox_size(6) == 15);
static_assert(rc::min_pool_bbox_size(6.25f) == 16);
static_assert(rc::min_pool_bbox_size(6.5f) == 16);

} // namespace

void Test::test_raycast()
{
    test_window(pass_mode::see_through);
    test_window(pass_mode::shoot_through);
    test_edit_in_same_frame();
    test_supercover_hits();

    constexpr uint32_t sizes[][2] = {
        {4, 4}, {4, 16}, {8, 8}, {8, 13}, {16, 16}, {16, 64}, {32, 32}, {64, 64},
    };
    auto w = make_world();
    for (const auto& [div, bbox] : sizes)
        test_rays(Pass::Params{div, bbox, pass_mode::pass}, w);

    {   // A ray running along a box side makes the slab test compute 0 * inf = NaN.
        // ray_aabb_intersection passes it as min/max's second argument, which Magnum
        // drops, so the side counts as a hit. raycast() can't produce this yet:
        // origins are integers and it inflates rect sides to n + 0.5.
        // If only the on-side rays fail, the NaN reached `tmin < tmax`: the min/max
        // arguments were swapped back, or raycast-loop.cpp lost -fno-finite-math-only.
        // cl clamps 1/0 to 1e20, tilting the ray toward +x/+y, so its max sides miss.
        using namespace rc::detail;
#if defined _MSC_VER && !defined __clang__
        constexpr bool max_side = false;
#else
        constexpr bool max_side = true;
#endif
        constexpr auto box = std::array<Vector2, 2>{{{-7, -13}, {11, 19}}};
        const auto check = [&](Vector2 origin, Vector2 dir, bool hit, float tmin) {
            auto inv = dir_inverse(dir);
            auto r = ray_aabb_intersection(origin, inv, box, ray_aabb_signs(inv));
            return r.result == hit && (!hit || Math::abs(r.tmin - tmin) < 1e-6f);
        };

        fm_assert(check({-53, -12}, { 1,  0}, true,     46));
        fm_assert(check({-53, -14}, { 1,  0}, false,     0));
        fm_assert(check({ -6, -54}, { 0,  1}, true,     41));
        fm_assert(check({ -8, -54}, { 0,  1}, false,     0));

        fm_assert(check({-53, -13}, { 1,  0}, true,     46));
        fm_assert(check({ 41, -13}, {-1,  0}, true,     30));
        fm_assert(check({-53,  19}, { 1,  0}, max_side, 46));
        fm_assert(check({ 41,  19}, {-1,  0}, max_side, 30));
        fm_assert(check({ -7, -54}, { 0,  1}, true,     41));
        fm_assert(check({ -7,  67}, { 0, -1}, true,     48));
        fm_assert(check({ 11, -54}, { 0,  1}, max_side, 41));
        fm_assert(check({ 11,  67}, { 0, -1}, max_side, 48));
    }
    {
        using namespace rc::detail;
        constexpr auto box = std::array<Vector2, 2>{{{-7, -13}, {11, 19}}};
        const auto check = [&](Vector2 origin, Vector2 dir, float radius, bool hit, float tmin) {
            auto inv = dir_inverse(dir);
            auto r = ray_rounded_rect_intersection(origin, dir, inv, box, radius, ray_aabb_signs(inv));
            return r.result == hit && (!hit || Math::abs(r.tmin - tmin) < 1e-3f);
        };

        fm_assert(check({-53, -12}, {1, 0}, 0, true, 46));
        fm_assert(check({-53,   0}, {1, 0}, 5, true, 41));
        // the corner (-7, 19): 3-4-5 triangles
        fm_assert(check({-53,  22}, {1, 0}, 5, true, 42));
        fm_assert(check({-53,  23}, {1, 0}, 5, true, 43));

        const auto c = box[1];
        const auto n = Vector2{1, 1}.normalized(), d = Vector2{1, -1}.normalized();
        fm_assert(check(c + n*4 - d*20, d, 5, true, 17));
        fm_assert(check(c + n*6 - d*20, d, 5, false, 0));
        fm_assert(check(c + n*2, d, 5, true, 0));
        const auto o = c + n*6 - d*20, inv = dir_inverse(d);
        fm_assert(ray_aabb_intersection(o, inv, {{box[0] - Vector2{5}, box[1] + Vector2{5}}}, ray_aabb_signs(inv)).result);
    }
}

} // namespace floormat
