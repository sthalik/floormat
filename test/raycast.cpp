#include "app.hpp"
#include "compat/borrowed-ptr.inl"
#include "src/tile-constants.hpp"
#include "src/raycast-diag.hpp"
#include "src/raycast-loop.hpp"
#include "src/intra-coord.inl"
#include "src/grid-pass.hpp"
#include "src/world.hpp"
#include "src/critter.hpp"
#include "loader/loader.hpp"
#include "loader/wall-cell.hpp"
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

auto run(point from, point to, world& w, bool b, float len)
{
    constexpr float fuzz = TILE_SIZE2.x();
    auto diag = rc::raycast_diag_s{};
    auto res = raycast_with_diag(diag, w, from, to, 0);
    const auto div = (int)w.raycast_pass_pool().params().div_size;
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

} // namespace

void Test::test_raycast()
{
    auto w = make_world();
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
    {
        constexpr auto p = point{
            {1, 2, 3},
            {4, 5},
            {6, 7},
        };
        run(p, p, w, true, 0);
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
        auto res = raycast_with_diag(diag, w2, point{ch, {2, 2}, {}}, point{ch, {12, 4}, {}}, 0);
        fm_assert(res.has_result);
        fm_assert(!res.success);
        fm_assert(res.collider.id == B->id);
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

        auto res = raycast(w2, point{ch2, {3, 1}, {-2, 0}}, point{ch2, {3, 12}, {-2, 0}}, 0);
        fm_assert(res.has_result);
        fm_assert(!res.success);
        fm_assert(res.collider.id == C->id);
    }
    {
        auto w2 = world{};
        constexpr auto ch = chunk_coords_{8, 12, 0};

        auto p = critter_proto{};
        p.bbox_size = Vector2ub{42, 26};
        auto C = w2.make_object<critter>(w2.make_id(), {ch, {8, 5}}, p);

        auto diag = rc::raycast_diag_s{};
        auto fwd = raycast_with_diag(diag, w2, point{ch, {2, 5}, {}}, point{ch, {13, 5}, {}}, 0);
        fm_assert(diag.dir.y() == 0);
        fm_assert(!fwd.success);
        fm_assert(fwd.collider.id == C->id);
        fm_assert(Math::abs(diag.tmin - 362.5f) < 1);

        auto back = raycast_with_diag(diag, w2, point{ch, {13, 5}, {}}, point{ch, {2, 5}, {}}, 0);
        fm_assert(!back.success);
        fm_assert(back.collider.id == C->id);
        fm_assert(Math::abs(diag.tmin - 298.5f) < 1);

        auto top = raycast_with_diag(diag, w2, point{ch, {2, 5}, {0, -13}}, point{ch, {13, 5}, {0, -13}}, 0);
        fm_assert(top.collider.id == C->id);
        fm_assert(Math::abs(diag.tmin - 362.5f) < 1);

        auto bottom = raycast_with_diag(diag, w2, point{ch, {2, 5}, {0, 13}}, point{ch, {13, 5}, {0, 13}}, 0);
        fm_assert(bottom.collider.id == C->id);
        fm_assert(Math::abs(diag.tmin - 362.5f) < 1);

        fm_assert(raycast(w2, point{ch, {2, 5}, {0,  14}}, point{ch, {13, 5}, {0,  14}}, 0).success);
        fm_assert(raycast(w2, point{ch, {2, 5}, {0, -14}}, point{ch, {13, 5}, {0, -14}}, 0).success);
    }
    {
        auto w2 = world{};
        constexpr auto ch = chunk_coords_{8, 14, 0};

        auto p = critter_proto{};
        p.bbox_size = Vector2ub{22, 34};
        auto C = w2.make_object<critter>(w2.make_id(), {ch, {5, 8}}, p);

        auto diag = rc::raycast_diag_s{};
        auto fwd = raycast_with_diag(diag, w2, point{ch, {5, 1}, {}}, point{ch, {5, 14}, {}}, 0);
        fm_assert(diag.dir.x() == 0);
        fm_assert(!fwd.success);
        fm_assert(fwd.collider.id == C->id);
        fm_assert(Math::abs(diag.tmin - 430.5f) < 1);

        auto back = raycast_with_diag(diag, w2, point{ch, {5, 14}, {}}, point{ch, {5, 1}, {}}, 0);
        fm_assert(!back.success);
        fm_assert(back.collider.id == C->id);
        fm_assert(Math::abs(diag.tmin - 366.5f) < 1);

        auto left = raycast_with_diag(diag, w2, point{ch, {5, 1}, {-11, 0}}, point{ch, {5, 14}, {-11, 0}}, 0);
        fm_assert(left.collider.id == C->id);
        fm_assert(Math::abs(diag.tmin - 430.5f) < 1);

        auto right = raycast_with_diag(diag, w2, point{ch, {5, 1}, {11, 0}}, point{ch, {5, 14}, {11, 0}}, 0);
        fm_assert(right.collider.id == C->id);
        fm_assert(Math::abs(diag.tmin - 430.5f) < 1);

        fm_assert(raycast(w2, point{ch, {5, 1}, { 12, 0}}, point{ch, {5, 14}, { 12, 0}}, 0).success);
        fm_assert(raycast(w2, point{ch, {5, 1}, {-12, 0}}, point{ch, {5, 14}, {-12, 0}}, 0).success);
    }
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
}

} // namespace floormat
