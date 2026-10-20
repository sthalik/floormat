#include "app.hpp"
#include "compat/borrowed-ptr.inl"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include "src/cover-point.hpp"
#include "src/critter.hpp"
#include "src/grid-cover.hpp"
#include "src/grid-pass.hpp"
#include "src/grid-pool-registry.hpp"
#include "src/intra-coord.inl"
#include "src/point.inl"
#include "src/raycast.hpp"
#include "src/RTree-search.hpp"
#include "src/search.hpp"
#include "src/search-astar.hpp"
#include "src/search-result.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/scenery-proto.hpp"
#include "src/tile-defs.hpp"
#include <algorithm>
#include <cr/GrowableArray.h>
#include <mg/Functions.h>
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

// Puts a critter at pos and shoots at its pixels. What a ray hits changes only at the slope of a pixel
// square's corner, so rays at each corner's slope and between each adjacent pair cover every aim.
bool cant_be_shot(world& w, point threat, point pos, Vector2ui size = body)
{
    critter_proto p;
    p.name = "npc"_s;
    p.bbox_size = Vector2ub(size);
    p.offset = pos.offset();
    const auto id = w.make_object<critter>(w.make_id(), pos.coord(), move(p))->id;
    // PoolRegistry's pools leave critters out of the grid, and raycast() skips the cells it marks empty.
    Grid::Pass::Pool pool{Pass::Params{8, pass_mode::shoot_through}, Search::never_continue()};
    const auto T = Vector3i(threat).xy();
    const auto shot = [&](Vector2i to) {
        const auto r = raycast(w, threat, threat + (to - T), 0, pool, Search::never_continue());
        return !r.success && r.collider.id == id;
    };
    const auto lo = Vector3i(pos).xy() - Vector2i(size/2u), hi = lo + Vector2i(size) - Vector2i{1};
    for (auto y = lo.y(); y <= hi.y(); y++)
        for (auto x = lo.x(); x <= hi.x(); x++)
            if (shot({x, y}))
                return false;

    const auto v = Vector3i(pos).xy() - T;
    const auto m = Math::abs(v.x()) >= Math::abs(v.y()) ? 0u : 1u, n = 1 - m;
    const auto sign = v[m] > 0 ? 1 : -1;
    struct slope { int64_t a, d; };
    const auto less = [](slope x, slope y) { return x.a*y.d < y.a*x.d; };
    // Corners in doubled coordinates from the threat, so the half-pixel edges are odd integers.
    Array<slope> slopes;
    const auto add_box = [&](Vector2i pmin, Vector2i pmax) {
        const auto c0 = (pmin - T)*2 - Vector2i{1}, c1 = (pmax - T)*2 - Vector2i{1};
        for (const auto c : { c0, c1, Vector2i{c0.x(), c1.y()}, Vector2i{c1.x(), c0.y()} })
            if (c[m]*sign > 0)
                arrayAppend(slopes, slope{c[n], c[m]*sign});
    };
    add_box(lo, hi + Vector2i{1});
    fm_assert(slopes.size() == 4);
    const auto [bmin, bmax] = std::minmax_element(slopes.begin(), slopes.end(), less);
    const auto smin = *bmin, smax = *bmax;
    int64_t dmax = 0;
    for (const auto& s : slopes)
        dmax = Math::max(dmax, s.d);
    // The test worlds hold one chunk, at the origin.
    const auto qlo = Vector2{-(float)chunk_size_xy}, qhi = Vector2{2*(float)chunk_size_xy};
    w[ch].rtree()->Search(qlo.data(), qhi.data(), [&](object_id, const chunk::RTree::Rect& r) {
        add_box(Vector2i(Math::ceil(Vector2{r.m_min[0], r.m_min[1]})), Vector2i(Math::ceil(Vector2{r.m_max[0], r.m_max[1]})));
        return true;
    });
    auto n_kept = 0u;
    for (const auto& s : slopes)
        if (!less(s, smin) && !less(smax, s))
            slopes[n_kept++] = s;
    arrayResize(slopes, NoInit, n_kept);
    std::sort(slopes.begin(), slopes.end(), less);

    const auto cast = [&](slope s) {
        const auto k = dmax / s.d + 1;
        Vector2i to;
        to[m] = T[m] + (int)(sign*s.d*k);
        to[n] = T[n] + (int)(s.a*k);
        return shot(to);
    };
    for (auto i = 0u; i < slopes.size(); i++)
        if (cast(slopes[i]) || i + 1 < slopes.size() && cast({slopes[i].a + slopes[i+1].a, slopes[i].d + slopes[i+1].d}))
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
    // Parallax lets a hidden body stick out past the wall's ends, up to 2 px in these scenes.
    constexpr auto fuzz = 2;
    fm_assert(px[across] - lo[across] >= wall_min[across] - fuzz && px[across] + hi[across] <= wall_max[across] + fuzz);
    fm_assert(point::distance(npc, r.pos) <= radius + tile_size_xy);

    auto A = astar{};
    const auto path = A.Dijkstra<0, false>(w, npc, r.pos, chunk_size_xy, size, Search::without_critters());
    fm_assert(path.is_found());
    fm_assert(path.path().back() == r.pos);
    fm_assert(cant_be_shot(w, threat, r.pos, size));
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
    fm_assert(!r.found || cant_be_shot(w, threat, r.pos));
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
    fm_assert(cant_be_shot(w, threat, r.pos));
}

void test_pillar_pair(uint8_t far_x, uint8_t near_h, int8_t near_dy, uint8_t far_h, int8_t far_dy, bool must_find)
{
    auto w = world();
    add_ground(w);
    add_blocker(w, {7, 8}, {16, near_h}, {8, near_dy});
    add_blocker(w, {far_x, 8}, {16, far_h}, {8, far_dy});
    rebuild(w);

    const point npc{ch, {6, 8}, {20, -4}}, threat{ch, {14, 8}, {}};
    const auto r = find(w, npc, threat, 48, 8);
    fm_assert(r.candidates > 0);
    fm_assert(r.found || !must_find);
    fm_assert(!r.found || cant_be_shot(w, threat, r.pos));
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
    // At (436, 492), rays to 15 body pixels pass between the far pillar's bottom and the near pillar's top.
    test_pillar_pair(8, 20, -12, 8, -24, false);
    // At (436, 519) every ray to a body pixel hits a pillar. One aimed past the body clears the far
    // pillar's top corner by 5e-5 in slope and grazes the top row. (436, 520) is hidden.
    test_pillar_pair(8, 8, 0, 24, 8, true);
    test_wall_out_of_radius();
}

} // namespace floormat
