#include "app.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/function2.hpp"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/critter.hpp"
#include "src/scenery-proto.hpp"
#include "src/ground-atlas.hpp"
#include "src/tile-image.hpp"
#include "src/tile-defs.hpp"
#include "src/tile-constants.hpp"
#include "src/global-coords.hpp"
#include "src/nanosecond.inl"
#include "src/point.inl"
#include "src/sweep-aabb.hpp"
#include "src/raycast-line.hpp"
#include "src/grid-pass.hpp"
#include "src/search.hpp"
#include "src/rect-intersects.hpp"
#include "test/run.hpp"

namespace floormat {
namespace {

constexpr Ns dt_60hz = Second / 60;

void place_pillar(world& w, chunk_coords_ ch, local_coords tile, Vector2b bb_off, Vector2ub bb_size,
                  pass_mode pass = pass_mode::blocked)
{
    scenery_proto p;
    p.atlas       = loader.invalid_scenery_atlas().proto->atlas;
    p.subtype     = generic_scenery_proto{};
    p.bbox_offset = bb_off;
    p.bbox_size   = bb_size;
    p.pass        = pass;
    w.make_scenery(w.make_id(), {ch, tile}, move(p));
}

// A pillar covering [min, max) in the frame of chunk ch. Its center must lie on one of ch's tiles.
void place_rect(world& w, chunk_coords_ ch, Vector2i min, Vector2i max, pass_mode pass = pass_mode::blocked)
{
    const auto center = min + (max - min) / 2;
    fm_assert(center >= -half_tile<Vector2i> && center < chunk_size<Vector2i> - half_tile<Vector2i>);
    const auto tile = (center + half_tile<Vector2i>) / tile_size_xy;
    place_pillar(w, ch, local_coords{tile}, Vector2b(center - tile * tile_size_xy), Vector2ub(max - min), pass);
}

void fill_ground(chunk& c)
{
    auto ground = loader.ground_atlas("floor-tiles");
    for (auto k = 0u; k < TILE_COUNT; k++)
        c[k].ground() = { ground, variant_t(k % ground->num_tiles()) };
}

bptr<critter> make_slit(world& w)
{
    // pillar geometry from editor/world.cpp:populate_sweep_aabb_slit.
    constexpr chunk_coords_ ch{0, 0, 0};
    auto& c = w[ch];
    fill_ground(c);
    place_pillar(w, ch, {0, 0}, Vector2b{ 0, -1}, Vector2ub{2, 2});
    place_pillar(w, ch, {1, 1}, Vector2b{ 1,  0}, Vector2ub{2, 2});

    object_id id = 0;
    auto proto = Run::make_proto(20.f);
    proto.bbox_size = Vector2ub(tile_size_xy);
    auto C = w.ensure_player_character(id, proto);
    auto i = C->index();
    C->teleport_to(i, global_coords{ch, {1, 1}}, Vector2b{-32, -32}, rotation_COUNT);
    c.mark_modified();
    w.init_scripts();
    return C;
}

void hold_keys(const bptr<critter>& C, bool L, bool R, bool U, bool D)
{
    C->set_keys(L, R, U, D);
    for (int k = 0; k < 60 * 2; k++)
    {
        auto idx = C->index();
        C->update(C, idx, dt_60hz);
    }
}

void test_critter_slit()
{
    // Each pillar reaches 1 px into the box's path, so the NE destination box is free and the N and
    // E ones aren't. N and E try NE as a diagonal alternative.
    // arrows_to_dir is iso-screen: R → world NE, R+U → N, R+D → E, L → SW.
    constexpr struct { bool L, R, U, D; } keys[] = {
        { false, true, false, false },
        { false, true, true,  false },
        { false, true, false, true  },
    };
    for (const auto& k : keys)
    {
        auto w = world();
        auto C = make_slit(w);
        const auto initial = C->position();
        hold_keys(C, k.L, k.R, k.U, k.D);
        fm_assert(C->position() == initial);
        hold_keys(C, true, false, false, false);
        fm_assert(point::distance_l1(C->position(), initial) > 16);
        w.finish_scripts();
    }
}

uint32_t first_blocked_step(world& w, chunk_coords_ ch, const rc::bresenham_line& line,
                            uint32_t first, uint32_t last, Vector2ui size)
{
    const auto free = [&](Vector2i c) {
        return Search::is_passable(w, ch, Range2D::fromSize(Vector2(c) - Vector2(size/2u), Vector2(size)));
    };
    if (!free(line.pixel_at(first)))
        return first;
    for (auto t = first; t < last; t++)
    {
        if (!free(line.pixel_at(t + 1)))
            return t + 1;
        const auto o = line.off_axis_at(t);
        if (o.diagonal && (!free(o.pixels[0]) || !free(o.pixels[1])))
            return t + 1;
    }
    return last + 1;
}

void check_swept_line(world& w, chunk_coords_ ch, Vector2i from, Vector2i to, uint32_t first, uint32_t last,
                      Vector2ui size, Pass::Pool* pool)
{
    const auto line = rc::bresenham_line{from, to};
    const auto expected = first_blocked_step(w, ch, line, first, last, size);
    const auto res = find_swept_collider(w, ch, line, first, last, size, Search::never_continue(), pool);
    if (res.step != expected || res.has_collider != (expected <= last))
        fm_abort("line (%d %d)-(%d %d) steps %u-%u size %ux%u pool %d: step %u, expected %u",
                 from.x(), from.y(), to.x(), to.y(), first, last, size.x(), size.y(),
                 pool ? (int)pool->params().div_size() : 0, res.step, expected);

    uint32_t nearest = last + 1, calls = 0;
    find_swept_collider(w, ch, line, first, last, size, Search::never_continue(), pool, [&](const swept_collider_result& r) {
        fm_assert(r.has_collider && r.step >= first && r.step <= last);
        nearest = Math::min(nearest, r.step);
        return true;
    });
    fm_assert(nearest == expected);
    find_swept_collider(w, ch, line, first, last, size, Search::never_continue(), pool, [&](const swept_collider_result&) {
        calls++;
        return false;
    });
    fm_assert(calls == (res.has_collider ? 1u : 0u));

    if (!res.has_collider)
        return;
    const auto overlaps = [&](Vector2i c) {
        const auto min = Vector2(c) - Vector2(size/2u);
        return rect_intersects(min, min + Vector2(size), res.rect.min(), res.rect.max());
    };
    bool any = overlaps(line.pixel_at(res.step));
    if (res.step > first)
        if (const auto o = line.off_axis_at(res.step - 1); o.diagonal)
            any |= overlaps(o.pixels[0]) || overlaps(o.pixels[1]);
    fm_assert(any);
}

// The step (15, 15) -> (16, 16) crosses the corner of four 4 px cells. Only its off-axis pixel
// (16, 15) is blocked, so the cells of both its pixels have their bits set.
void test_swept_collider_cell_corner()
{
    auto w = world();
    constexpr chunk_coords_ ch{0, 0, 0};
    fill_ground(w[ch]);
    place_pillar(w, ch, {0, 0}, Vector2b{18, 12}, Vector2ub{2, 4});
    w[ch].mark_modified();
    Pass::Pool pool{Pass::Params{4, 4, pass_mode::pass}, Search::never_continue()};
    check_swept_line(w, ch, {8, 8}, {24, 24}, 0, 16, Vector2ui{4}, &pool);
    const auto res = find_swept_collider(w, ch, rc::bresenham_line{{8, 8}, {24, 24}}, 0, 16, Vector2ui{4},
                                         Search::never_continue(), &pool);
    fm_assert(res.has_collider && res.step == 8);
}

void test_swept_collider_line()
{
    auto w = world();
    constexpr chunk_coords_ ch{0, 0, 0}, ch2{1, 0, 0}, ch3{1, 1, 0};
    for (auto c : { ch, ch2, ch3 })
        fill_ground(w[c]);
    place_pillar(w, ch, {3, 4}, Vector2b{0, 0}, Vector2ub{20, 12});
    place_pillar(w, ch, {8, 9}, Vector2b{-5, 7}, Vector2ub{10, 30});
    // across the edge with ch2
    place_pillar(w, ch, {15, 6}, Vector2b{20, 0}, Vector2ub{30, 8});
    place_pillar(w, ch2, {1, 10}, Vector2b{-3, 11}, Vector2ub{14, 6});
    // at the corner of ch, ch2 and ch3
    place_pillar(w, ch2, {0, 15}, Vector2b{-28, 20}, Vector2ub{6, 8});
    place_pillar(w, ch3, {2, 1}, Vector2b{0, 0}, Vector2ub{18, 24});
    for (auto c : { ch, ch2, ch3 })
        w[c].mark_modified();

    // through the corner of four chunks, where the off-axis pixels are in neither pixel's chunk
    for (auto size : { 1u, 4u, 7u })
    {
        check_swept_line(w, ch, {950, 950}, {1040, 1040}, 0, 90, Vector2ui{size}, nullptr);
        check_swept_line(w, ch, {1040, 950}, {950, 1040}, 0, 90, Vector2ui{size}, nullptr);
    }

    uint32_t seed = 0x2545f491;
    const auto next = [&](uint32_t n) {
        seed = seed * 1664525u + 1013904223u;
        return (seed >> 8) % n;
    };
    for (auto size : { Vector2ui{1}, Vector2ui{3}, Vector2ui{4, 9}, Vector2ui{8}, Vector2ui{13, 6}, Vector2ui{24} })
    {
        const auto pool_bbox = (Math::max(size.x(), size.y()) + 1) & ~1u;
        Pass::Pool pool8{Pass::Params{8, pool_bbox, pass_mode::pass}, Search::never_continue()};
        Pass::Pool pool16{Pass::Params{16, pool_bbox, pass_mode::pass}, Search::never_continue()};
        for (auto i = 0u; i < 24; i++)
        {
            const auto from = Vector2i{(int32_t)next(1900) - 32, (int32_t)next(1900) - 32};
            const auto to = from + Vector2i{(int32_t)next(601) - 300, (int32_t)next(601) - 300};
            const auto steps = rc::bresenham_line{from, to}.steps();
            const auto first = next(steps + 1), last = first + next(steps - first + 1);
            check_swept_line(w, ch, from, to, first, last, size, nullptr);
            check_swept_line(w, ch, from, to, first, last, size, &pool8);
            check_swept_line(w, ch, from, to, first, last, size, &pool16);
        }
    }
}

// A box walking 41 steps ends flush against a pillar: on its leading face for a cardinal line, at its
// leading corner for a diagonal one. Touching isn't overlap, so nothing blocks. With the pillar 1 px
// deeper, step 41 is the first the box can't reach. A pass_mode::pass pillar across the line never
// blocks.
void test_swept_collider_flush()
{
    constexpr chunk_coords_ ch{0, 0, 0};
    constexpr auto from = Vector2i{301, 287};
    constexpr uint32_t steps = 41;
    constexpr Vector2i dirs[] = { {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1} };
    for (auto size : { Vector2ui{1}, Vector2ui{4}, Vector2ui{7}, Vector2ui{24, 25} })
        for (auto d : dirs)
            for (int32_t deeper : { 0, 1 })
            {
                auto w = world();
                fill_ground(w[ch]);
                const auto box = [&](uint32_t k) {
                    const auto min = from + d * int32_t(k) - Vector2i(size/2u);
                    return Range2Di{min, min + Vector2i(size)};
                };
                const auto end = box(steps), mid = box(steps/2);
                Vector2i min, max;
                for (auto i = 0u; i < 2; i++)
                {
                    min[i] = d[i] > 0 ? end.max()[i] - deeper : d[i] < 0 ? end.min()[i] + deeper - 8 : end.min()[i] - 3;
                    max[i] = d[i] == 0 ? end.max()[i] + 3 : min[i] + 8;
                }
                place_rect(w, ch, min, max);
                place_rect(w, ch, mid.min() - Vector2i{5}, mid.max() + Vector2i{5}, pass_mode::pass);
                w[ch].mark_modified();

                const auto line = rc::bresenham_line{from, from + d * int32_t(steps)};
                const auto res = find_swept_collider(w, ch, line, 0, steps, size, Search::never_continue());
                fm_assert(res.has_collider == (deeper != 0));
                fm_assert(res.step == (deeper ? steps : steps + 1));

                const auto pool_bbox = (Math::max(size.x(), size.y()) + 1) & ~1u;
                Pass::Pool pool8{Pass::Params{8, pool_bbox, pass_mode::pass}, Search::never_continue()};
                check_swept_line(w, ch, from, from + d * int32_t(steps), 0, steps, size, nullptr);
                check_swept_line(w, ch, from, from + d * int32_t(steps), 0, steps, size, &pool8);
            }
}

} // namespace

void Test::test_sweep_aabb()
{
    test_critter_slit();
    test_swept_collider_cell_corner();
    test_swept_collider_line();
    test_swept_collider_flush();
}

} // namespace floormat
