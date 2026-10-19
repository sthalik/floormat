#include "app.hpp"
#include "compat/borrowed-ptr.inl"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include "src/cover-point.hpp"
#include "src/grid-cover.hpp"
#include "src/grid-pass.hpp"
#include "src/grid-pool-registry.hpp"
#include "src/intra-coord.inl"
#include "src/point.inl"
#include "src/raycast.hpp"
#include "src/search.hpp"
#include "src/search-astar.hpp"
#include "src/search-result.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/scenery-proto.hpp"
#include "src/tile-defs.hpp"
#include <mg/Range.h>

namespace floormat {
namespace {

constexpr chunk_coords_ ch{0, 0, 0};
constexpr auto body = Vector2ui{24, 24};

void add_ground(world& w)
{
    const auto floor = tile_image_proto{ loader.ground_atlas("tiles"), 0 };
    auto& c = w[ch];
    for (auto j = 0u; j < TILE_MAX_DIM; j++)
        for (auto i = 0u; i < TILE_MAX_DIM; i++)
            c[{i, j}].ground() = floor;
}

void add_blocker(world& w, local_coords tile, Vector2ub bbox_size, Vector2b bbox_offset = {})
{
    scenery_proto p;
    p.atlas       = loader.invalid_scenery_atlas().proto->atlas;
    p.subtype     = generic_scenery_proto{};
    p.bbox_size   = bbox_size;
    p.bbox_offset = bbox_offset;
    p.pass        = pass_mode::blocked;
    w.make_scenery(w.make_id(), {ch, tile}, move(p));
}

void rebuild(world& w)
{
    auto& c = w[ch];
    c.mark_passability_modified();
    c.ensure_passability();
}

bool passable(world& w, point pt, Vector2ui size)
{
    const auto min = Vector2(pt.intra().center_shifted() - Vector2i(size/2u));
    return Search::is_passable(w, pt.chunk3(), Range2D{min, min + Vector2(size)}, Search::without_critters());
}

cover_point_result find(world& w, point npc, point threat, uint32_t radius, uint32_t div_size, Vector2ui size = body)
{
    Grid::PoolRegistry pools{div_size};
    return find_cover_point(w, pools, Cover::Params{div_size}, {npc, size, threat, radius});
}

bool hidden_from(world& w, point threat, point pos)
{
    Grid::PoolRegistry pools{8};
    auto& pool = pools.pass_pool(Pass::Params{8, pass_mode::shoot_through});
    const auto ext = Vector2i(body/2u) - Vector2i{1};
    for (auto y = -ext.y(); y <= ext.y(); y++)
        for (auto x = -ext.x(); x <= ext.x(); x++)
            if (Math::abs(x) == ext.x() || Math::abs(y) == ext.y())
                if (raycast(w, threat, pos + Vector2i{x, y}, 0, pool, Search::without_critters()).success)
                    return false;
    return true;
}

void test_hides_behind_wall(local_coords npc_tile, local_coords threat_tile, local_coords wall_tile,
                            Vector2ub wall_size, Vector2i axis, uint32_t radius = 3*tile_size_xy,
                            Vector2ui size = body)
{
    auto w = world();
    add_ground(w);
    add_blocker(w, wall_tile, wall_size);
    rebuild(w);

    const point npc{ch, npc_tile, {}}, threat{ch, threat_tile, {}};
    const auto r = find(w, npc, threat, radius, 8, size);
    fm_assert(r.found);
    fm_assert(r.axis == axis);
    fm_assert(passable(w, r.pos, size));
    fm_assert(!passable(w, r.pos + r.axis, size));

    const auto wall_min = Vector2i(wall_tile)*tile_size_xy - Vector2i(wall_size/2), wall_max = wall_min + Vector2i(wall_size);
    const auto px = Vector3i(r.pos).xy();
    const auto lo = Vector2i(size/2u), hi = Vector2i(size) - lo;
    const auto gap = axis.x() > 0 ? wall_min.x() - (px.x() + hi.x())
                   : axis.x() < 0 ? px.x() - lo.x() - wall_max.x()
                   : axis.y() > 0 ? wall_min.y() - (px.y() + hi.y())
                   :                px.y() - lo.y() - wall_max.y();
    fm_assert_equal(0, gap);
    const auto across = axis.x() != 0 ? 1u : 0u;
    // Rays aim 1 px inside the body and raycast() grows colliders by half a pixel, so the body may stick out by two
    constexpr int fuzz = 2;
    fm_assert(px[across] - lo[across] >= wall_min[across] - fuzz && px[across] + hi[across] <= wall_max[across] + fuzz);
    fm_assert(point::distance(npc, r.pos) <= radius + tile_size_xy);

    auto A = astar{};
    const auto path = A.Dijkstra<0, false>(w, npc, r.pos, chunk_size_xy, size, Search::without_critters());
    fm_assert(path.is_found());
    fm_assert(path.path().back() == r.pos);
}

void test_open_ground_has_no_cover()
{
    auto w = world();
    add_ground(w);
    rebuild(w);

    const auto r = find(w, point{ch, {4, 8}, {}}, point{ch, {12, 8}, {}}, 3*tile_size_xy, 16);
    fm_assert(!r.found);
    fm_assert_equal(0u, r.candidates);
}

void test_narrow_pillar_rejected()
{
    auto w = world();
    add_ground(w);
    // The pass grid clears each 8 px cell the pillar touches, so the cover grid sees it 32 px wide.
    // Cell-centred bodies fit that shadow, but a corner sticks out past the real 18 px.
    add_blocker(w, {7, 8}, {16, 18});
    rebuild(w);

    const auto r = find(w, point{ch, {6, 8}, {}}, point{ch, {14, 8}, {}}, 48, 8);
    fm_assert(!r.found);
    fm_assert(r.candidates > 0);
    fm_assert_equal(r.candidates, r.rejected);
}

void test_gap_between_pillars_rejected()
{
    auto w = world();
    add_ground(w);
    // 6 px apart: the pass grid closes the gap. Neither pillar hides the body alone.
    add_blocker(w, {7, 8}, {16, 16}, {0, -11});
    add_blocker(w, {7, 8}, {16, 16}, {0, 11});
    rebuild(w);

    const auto r = find(w, point{ch, {6, 8}, {}}, point{ch, {14, 8}, {}}, 48, 8);
    fm_assert(r.candidates > 0);
    fm_assert(!r.found);
    fm_assert_equal(r.candidates, r.rejected);
}

void test_side_wall_corner_seen()
{
    auto w = world();
    add_ground(w);
    // The wall's north face is the body's south face and reaches 4 px past its east face. The pillar
    // ahead ends 6 px north of the wall, and the threat sees the body between them.
    add_blocker(w, {6, 8}, {100, 16}, {18, 32});
    add_blocker(w, {7, 8}, {16, 64}, {8, -14});
    rebuild(w);

    const point npc{ch, {7, 8}, {-12, 12}}, threat{ch, {12, 10}, {-20, -24}};
    const auto r = find(w, npc, threat, 48, 8);
    fm_assert(r.candidates > 0);
    fm_assert(!r.found || hidden_from(w, threat, r.pos));
}

// The pillar is as tall as the body, and a wall along its south end stops the body sliding past it.
void test_shift_to_only_spot(int8_t pillar_dy, bool split = false)
{
    auto w = world();
    add_ground(w);
    if (split)
    {
        add_blocker(w, {7, 8}, {16, 17}, {8, (int8_t)(pillar_dy - 4)});
        add_blocker(w, {7, 8}, {16, 7}, {8, (int8_t)(pillar_dy + 8)});
    }
    else
        add_blocker(w, {7, 8}, {16, 24}, {8, pillar_dy});
    add_blocker(w, {5, 8}, {192, 16}, {32, (int8_t)(pillar_dy + 20)});
    rebuild(w);

    const point npc{ch, {6, 8}, {20, -4}}, threat{ch, {14, 8}, {}};
    const auto r = find(w, npc, threat, 48, 8);
    fm_assert(r.found);
    fm_assert(hidden_from(w, threat, r.pos));
}

void test_wall_out_of_radius()
{
    auto w = world();
    add_ground(w);
    add_blocker(w, {9, 8}, {16, 96});
    rebuild(w);

    const auto r = find(w, point{ch, {4, 8}, {}}, point{ch, {13, 8}, {}}, tile_size_xy, 16);
    fm_assert(!r.found);
}

} // namespace

void Test::test_cover_point()
{
    test_hides_behind_wall({4, 8}, {12, 8}, {7, 8}, {16, 96}, {1, 0});
    test_hides_behind_wall({8, 12}, {8, 4}, {8, 9}, {96, 16}, {0, -1});
    // Cell-centred candidates sit 4 px off the pillar's centre line, so the 24 px body fits the
    // 26 px pillar's span only after a sideways shift.
    test_hides_behind_wall({6, 8}, {14, 8}, {7, 8}, {16, 26}, {1, 0}, 48);
    // An odd body's extra pixel is on +x/+y, so its -x and -y sides are the short ones.
    test_hides_behind_wall({10, 8}, {2, 8}, {7, 8}, {16, 96}, {-1, 0}, 3*tile_size_xy, {23, 25});
    test_hides_behind_wall({8, 12}, {8, 4}, {8, 9}, {96, 16}, {0, -1}, 3*tile_size_xy, {23, 25});
    test_open_ground_has_no_cover();
    test_narrow_pillar_rejected();
    test_gap_between_pillars_rejected();
    test_side_wall_corner_seen();
    // Row 516 runs into the wall, and row 508 needs a 6 px shift.
    test_shift_to_only_spot(3);
    // Rays to row 508 hit only the pillar's upper rect.
    test_shift_to_only_spot(3, true);
    // Row 508 needs 1 px, and 4 px runs into the wall.
    test_shift_to_only_spot(-1);
    test_wall_out_of_radius();
}

} // namespace floormat
