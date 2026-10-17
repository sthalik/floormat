#include "app.hpp"
#include "compat/borrowed-ptr.inl"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include "src/cover-point.hpp"
#include "src/grid-cover.hpp"
#include "src/grid-pass.hpp"
#include "src/intra-coord.inl"
#include "src/point.inl"
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

void add_blocker(world& w, local_coords tile, Vector2ub bbox_size)
{
    scenery_proto p;
    p.atlas     = loader.invalid_scenery_atlas().proto->atlas;
    p.subtype   = generic_scenery_proto{};
    p.bbox_size = bbox_size;
    p.pass      = pass_mode::blocked;
    w.make_scenery(w.make_id(), {ch, tile}, move(p));
}

void rebuild(world& w)
{
    auto& c = w[ch];
    c.mark_passability_modified();
    c.ensure_passability();
}

bool passable(world& w, point pt)
{
    const auto local = Vector2(pt.intra().center_shifted());
    const auto half = Vector2(body) * .5f;
    return Search::is_passable(w, pt.chunk3(), Range2D{local - half, local + half}, Search::without_critters());
}

cover_point_result find(world& w, point npc, point threat, uint32_t radius, uint32_t div_size)
{
    Cover::Pool cover{Cover::Params{div_size}};
    Pass::Pool rays{Pass::Params{div_size, pass_mode::shoot_through}};
    return find_cover_point(w, cover, rays, {npc, body, threat, radius});
}

void test_hides_behind_wall(local_coords npc_tile, local_coords threat_tile, local_coords wall_tile,
                            Vector2ub wall_size, Vector2i axis, uint32_t radius = 3*tile_size_xy)
{
    auto w = world();
    add_ground(w);
    add_blocker(w, wall_tile, wall_size);
    rebuild(w);

    const point npc{ch, npc_tile, {}}, threat{ch, threat_tile, {}};
    const auto r = find(w, npc, threat, radius, 8);
    fm_assert(r.found);
    fm_assert(r.axis == axis);
    fm_assert(passable(w, r.pos));
    fm_assert(!passable(w, r.pos + r.axis));

    const auto wall_min = Vector2i(wall_tile)*tile_size_xy - Vector2i(wall_size/2), wall_max = wall_min + Vector2i(wall_size);
    const auto px = Vector3i(r.pos).xy();
    const auto half = Vector2i(body/2u);
    const auto gap = axis.x() > 0 ? wall_min.x() - (px.x() + half.x())
                   : axis.x() < 0 ? px.x() - half.x() - wall_max.x()
                   : axis.y() > 0 ? wall_min.y() - (px.y() + half.y())
                   :                px.y() - half.y() - wall_max.y();
    fm_assert_equal(0, gap);
    const auto across = axis.x() != 0 ? 1u : 0u;
    // raycast() grows colliders by half a pixel, so the body may stick out by one
    constexpr int fuzz = 1;
    fm_assert(px[across] - half[across] >= wall_min[across] - fuzz && px[across] + half[across] <= wall_max[across] + fuzz);
    fm_assert(point::distance(npc, r.pos) <= radius + tile_size_xy);

    auto A = astar{};
    const auto path = A.Dijkstra<0, false>(w, npc, r.pos, chunk_size_xy, body, Search::without_critters());
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
    // 28 px pillar's span only after a sideways shift.
    test_hides_behind_wall({6, 8}, {14, 8}, {7, 8}, {16, 28}, {1, 0}, 48);
    test_open_ground_has_no_cover();
    test_wall_out_of_radius();
}

} // namespace floormat
