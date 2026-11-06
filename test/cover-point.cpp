#include "app.hpp"
#include "compat/borrowed-ptr.inl"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include "src/cover-point.hpp"
#include "src/critter.hpp"
#include "src/critter-script.hpp"
#include "src/critter-script-cover.hpp"
#include "src/critter-walk.hpp"
#include "src/anim-atlas.hpp"
#include "src/nanosecond.inl"
#include "src/grid-cover.hpp"
#include "src/grid-pass.hpp"
#include "src/grid-registry.hpp"
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
#include "src/scenery.hpp"
#include "src/tile-defs.hpp"
#include <algorithm>
#include <cmath>
#include <cr/GrowableArray.h>
#include <cr/Optional.h>
#include <mg/Constants.h>
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

bptr<scenery> add_blocker(world& w, global_coords pos, Vector2ub bbox_size, Vector2b bbox_offset = {},
                          pass_mode pass = pass_mode::blocked)
{
    scenery_proto p;
    p.atlas       = loader.invalid_scenery_atlas().proto->atlas;
    p.subtype     = generic_scenery_proto{};
    p.bbox_size   = bbox_size;
    p.bbox_offset = bbox_offset;
    p.pass        = pass;
    return w.make_scenery(w.make_id(), pos, move(p));
}

bptr<scenery> add_blocker(world& w, local_coords tile, Vector2ub bbox_size, Vector2b bbox_offset = {},
                          pass_mode pass = pass_mode::blocked)
{
    return add_blocker(w, global_coords{ch, tile}, bbox_size, bbox_offset, pass);
}

void rebuild(world& w)
{
    auto& c = w[ch];
    c.mark_passability_modified();
    c.ensure_passability();
}

bool passable(world& w, point pt, Vector2ui size, const Search::pred& pred = Search::without_critters())
{
    const auto min = Vector2(pt.intra().center_shifted() - Vector2i(size/2u));
    return Search::is_passable(w, pt.chunk3(), Range2D{min, min + Vector2(size)}, pred);
}

cover_point_result find(world& w, point npc, point threat, u32 radius, u32 div_size, Vector2ui size = body,
                        const Search::pred& pred = Search::without_critters(), bool list_spots = false,
                        u8 octants = 0, u32 max_cover_end_dist = 0)
{
    static cover_point finder;
    Grid::GridRegistry pools;
    return finder.find_cover_point(w, pools, Cover::Params{div_size}, {npc, size, threat, radius, octants, max_cover_end_dist},
                                   pred, list_spots);
}

bptr<critter> add_critter(world& w, point pos, Vector2ui size = body)
{
    critter_proto p;
    p.name = "npc"_s;
    p.bbox_size = Vector2ub(size);
    p.offset = pos.offset();
    return w.make_object<critter>(w.make_id(), pos.coord(), move(p));
}

// Shoots from threat at the pixels of critter id, whose box is centred on pos, passing through
// critter threat_id. What a ray hits changes only at the slope of a pixel square's corner, so rays
// at each corner's slope and between each adjacent pair cover every aim.
bool cant_be_shot_at(world& w, point threat, object_id threat_id, point pos, Vector2ui size, object_id id)
{
    // GridRegistry's pools leave critters out of the grid, and raycast() skips the cells it marks empty.
    Grid::Pass::Pool pool{Pass::Params{8, pass_mode::shoot_through}, Search::never_continue()};
    const auto T = Vector3i(threat).xy();
    const auto shot = [&](Vector2i to) {
        const auto r = raycast(w, threat, threat + (to - T), threat_id, pool, Search::never_continue());
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
    struct slope { i64 a, d; };
    const auto less = [](slope x, slope y) { return x.a*y.d < y.a*x.d; };
    // Corners in doubled coordinates from the threat, so the half-pixel edges are odd integers.
    Array<slope> slopes;
    arrayReserve(slopes, 16);
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
    i64 dmax = 0;
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

// Puts a critter at pos and shoots at it.
bool cant_be_shot(world& w, point threat, point pos, Vector2ui size = body)
{
    return cant_be_shot_at(w, threat, 0, pos, size, add_critter(w, pos, size)->id);
}

void test_hides_behind_wall(local_coords npc_tile, local_coords threat_tile, local_coords wall_tile,
                            Vector2ub wall_size, Vector2i axis, u32 radius = 3*tile_size_xy,
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

void test_narrow_pillar_rejected(Vector2ui size = body)
{
    auto w = world();
    add_ground(w);
    // The pass grid clears each 8 px cell the pillar touches, so the cover grid sees it 32 px wide.
    // Cell-centred bodies fit that shadow, but a corner sticks out past the real 18 px.
    add_blocker(w, {7, 8}, {16, 18});
    rebuild(w);

    const auto r = find(w, point{ch, {6, 8}, {}}, point{ch, {14, 8}, {}}, 48, 8, size);
    fm_assert(!r.found);
    fm_assert(r.candidates > 0);
    fm_assert_equal(r.candidates, r.rejected);
}

void test_gap_between_pillars_rejected(Vector2ui size = body)
{
    auto w = world();
    add_ground(w);
    // 6 px apart: the pass grid closes the gap. Neither pillar hides the body alone.
    add_blocker(w, {7, 8}, {16, 16}, {0, -11});
    add_blocker(w, {7, 8}, {16, 16}, {0, 11});
    rebuild(w);

    const auto r = find(w, point{ch, {6, 8}, {}}, point{ch, {14, 8}, {}}, 48, 8, size);
    fm_assert(r.candidates > 0);
    fm_assert(!r.found);
    fm_assert_equal(r.candidates, r.rejected);
}

void test_side_wall_corner_seen(Vector2ui size = body)
{
    auto w = world();
    add_ground(w);
    // The wall's north face is the body's south face and reaches 4 px past its east face. The pillar
    // ahead ends 6 px north of the wall, and the threat sees the body between them.
    const auto d = Vector2i(size - size/2u) - Vector2i(body/2u);
    add_blocker(w, {6, 8}, {100, 16}, {(i8)(18 + d.x()), (i8)(32 + d.y())});
    add_blocker(w, {7, 8}, {16, 64}, {(i8)(8 + d.x()), (i8)(-14 + d.y())});
    rebuild(w);

    const point npc{ch, {7, 8}, {-12, 12}}, threat{ch, {12, 10}, {-20, -24}};
    const auto r = find(w, npc, threat, 48, 8, size);
    fm_assert(r.candidates > 0);
    fm_assert(!r.found || cant_be_shot(w, threat, r.pos, size));
}

// The pillar is as tall as the body, and a wall along its south end stops the body going past it.
void test_shift_to_only_spot(i8 pillar_dy, bool split = false, Vector2ui size = body)
{
    auto w = world();
    add_ground(w);
    if (split)
    {
        add_blocker(w, {7, 8}, {16, 17}, {8, (i8)(pillar_dy - 4)});
        add_blocker(w, {7, 8}, {16, 7}, {8, (i8)(pillar_dy + 8)});
    }
    else
        add_blocker(w, {7, 8}, {16, (u8)size.y()}, {8, pillar_dy});
    add_blocker(w, {5, 8}, {192, 16}, {32, (i8)(pillar_dy + (int)(size.y() - size.y()/2) + 8)});
    rebuild(w);

    const point npc{ch, {6, 8}, {20, -4}}, threat{ch, {14, 8}, {}};
    const auto r = find(w, npc, threat, 48, 8, size);
    fm_assert(r.found);
    fm_assert(cant_be_shot(w, threat, r.pos, size));
}

void test_pillar_pair(u8 far_x, u8 near_h, i8 near_dy, u8 far_h, i8 far_dy, bool must_find)
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

point at_px(Vector2i px) { return point{Vector3i{px, 0}}; }

// Found only when each cell reads both octants beside the threat's bearing, not only the nearest.
void test_octants_beside_bearing()
{
    auto w = world();
    add_ground(w);
    add_blocker(w, {7, 8}, {16, 96});
    rebuild(w);

    const auto threat = at_px({574, 667});
    const auto r = find(w, at_px({438, 468}), threat, 48, 8);
    fm_assert(r.found);
    fm_assert(cant_be_shot(w, threat, r.pos));
}

// Three 16 px blocks corner to corner. The spot is hidden only where two blocks' slope ranges from
// the threat meet at the corner they share.
void test_touching_blockers()
{
    auto w = world();
    add_ground(w);
    add_blocker(w, {7, 8}, {16, 16});
    add_blocker(w, {7, 8}, {16, 16}, {16, 16});
    add_blocker(w, {7, 8}, {16, 16}, {32, 32});
    rebuild(w);

    const auto threat = at_px({454, 567});
    const auto r = find(w, at_px({455, 484}), threat, 48, 8);
    fm_assert(r.found);
    fm_assert(cant_be_shot(w, threat, r.pos));
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

// A critter stands on the spot found on empty ground, so the body has to fit somewhere else.
void test_spot_taken()
{
    auto w = world();
    add_ground(w);
    add_blocker(w, {7, 8}, {16, 96});
    rebuild(w);

    const point npc{ch, {4, 8}, {}}, threat{ch, {12, 8}, {}};
    const auto spot = find(w, npc, threat, 3*tile_size_xy, 8);
    fm_assert(spot.found);
    const auto other = add_critter(w, spot.pos);
    const auto r = find(w, npc, threat, 3*tile_size_xy, 8, body, Search::never_continue());
    fm_assert(r.found);
    fm_assert(passable(w, r.pos, body, Search::never_continue()));
    w[ch].remove_object(*other, other->index());
    fm_assert(cant_be_shot(w, threat, r.pos));
}

// Returns the search for npc and threat with its spots listed, after checking that a search
// without them returns the same fields. The spots are valid until the next find().
cover_point_result find_listed(world& w, point npc, point threat, u32 radius)
{
    const auto r0 = find(w, npc, threat, radius, 8);
    const auto r = find(w, npc, threat, radius, 8, body, Search::without_critters(), true);
    fm_assert(r.found == r0.found);
    fm_assert(r.pos == r0.pos && r.axis == r0.axis);
    fm_assert_equal(r0.candidates, r.candidates);
    fm_assert_equal(r0.rejected, r.rejected);
    fm_assert(r.spots.isEmpty() == !r.found);
    fm_assert(!r.found || r.spots[0].pos == r.pos && r.spots[0].axis == r.axis);
    for (auto i = 0u; i < r.spots.size(); i++)
    {
        const auto [pos, axis] = r.spots[i];
        for (auto j = i + 1; j < r.spots.size(); j++)
            fm_assert(pos != r.spots[j].pos);
        fm_assert(passable(w, pos, body));
        // flush against its blocker
        fm_assert(!passable(w, pos + axis, body));
        // Removed after the shot, so it can't stop the rays aimed at the next spot.
        const auto c = add_critter(w, pos);
        fm_assert(cant_be_shot_at(w, threat, 0, pos, body, c->id));
        w[ch].remove_object(*c, c->index());
    }
    return r;
}

// The scene of test_hides_behind_wall's first call, where the wall hides the body at more than
// one spot.
void test_lists_cover_spots()
{
    auto w = world();
    add_ground(w);
    add_blocker(w, {7, 8}, {16, 96});
    rebuild(w);

    const point npc{ch, {4, 8}, {}}, threat{ch, {12, 8}, {}};
    constexpr auto radius = 3*tile_size_xy;
    cover_point fresh;
    Grid::GridRegistry pools;
    const auto r1 = fresh.find_cover_point(w, pools, Cover::Params{8}, {npc, body, threat, radius},
                                           Search::without_critters(), true);
    // Spots left over from a search on the other side of the wall would show in the next list.
    const auto other = find(w, point{ch, {10, 8}, {}}, point{ch, {2, 8}, {}}, radius, 8, body, Search::without_critters(), true);
    fm_assert(!other.spots.isEmpty());
    const auto r = find_listed(w, npc, threat, radius);
    fm_assert(r.spots.size() > 1);
    fm_assert_equal(r1.spots.size(), r.spots.size());
    for (auto i = 0u; i < r.spots.size(); i++)
        fm_assert(r.spots[i].pos == r1.spots[i].pos && r.spots[i].axis == r1.spots[i].axis);
}

// test_lists_cover_spots' scene on fresh pools, searched with max_fills = 1 until a call finds
// cover. Each call does one fill, and together they do the fills of one unlimited search and find
// its spot.
void test_max_fills()
{
    auto w = world();
    add_ground(w);
    add_blocker(w, {7, 8}, {16, 96});
    rebuild(w);

    const point npc{ch, {4, 8}, {}}, threat{ch, {12, 8}, {}};
    constexpr auto radius = 3*(u32)tile_size_xy;
    cover_point finder;
    cover_query q{npc, body, threat, radius, 3};
    const auto whole = [&] {
        Grid::GridRegistry pools;
        return finder.find_cover_point(w, pools, Cover::Params{8}, q, Search::without_critters());
    }();
    fm_assert(whole.found && !whole.out_of_fills);
    fm_assert(whole.fills > 2);

    Grid::GridRegistry pools;
    q.max_fills = 1;
    auto calls = 0u, fills = 0u;
    cover_point_result r;
    do
    {
        r = finder.find_cover_point(w, pools, Cover::Params{8}, q, Search::without_critters());
        fm_assert(r.fills <= 1);
        fm_assert(!r.out_of_fills || r.fills == 1 && !r.found);
        fills += r.fills;
        calls++;
        fm_assert(calls <= whole.fills + 1);
    }
    while (r.out_of_fills);
    fm_assert(calls > 1);
    fm_assert_equal(whole.fills, fills);
    fm_assert(r.found && r.pos == whole.pos && r.axis == whole.axis);

    q.max_fills = 0;
    const auto warm = finder.find_cover_point(w, pools, Cover::Params{8}, q, Search::without_critters());
    fm_assert(warm.found && !warm.out_of_fills && warm.fills == 0 && warm.pos == whole.pos);
}

// test_narrow_pillar_rejected's pillar, then a wall the npc is nearer to. The pillar's candidates
// are tried after the wall's first spot, so the list's search rejects them past pos.
void test_lists_past_rejected()
{
    const point npc{ch, {6, 6}, {}}, threat{ch, {14, 8}, {}};
    constexpr auto radius = 3*tile_size_xy;
    auto w = world();
    add_ground(w);
    add_blocker(w, {7, 8}, {16, 18});
    rebuild(w);
    const auto p = find(w, npc, threat, radius, 8);
    fm_assert(!p.found && p.rejected > 0);

    add_blocker(w, {7, 5}, {16, 64});
    rebuild(w);
    const auto r = find_listed(w, npc, threat, radius);
    fm_assert(r.found);
    fm_assert(r.spots.size() > 1);
}

// Searches of the tiles around npc, each with its area, try the candidates of one search over
// radius once each and together list its spots, of which there are more than min_spots. With npc on
// a cell center, the edges of the tiles around it pass through cell centers.
void check_lists_by_area(world& w, point npc, point threat, u8 octants, u32 max_cover_end_dist, u32 min_spots)
{
    constexpr auto radius = 4*(u32)tile_size_xy;
    cover_point finder;
    Grid::GridRegistry pools;
    const auto search = [&](Optional<Range2Di> area) {
        return finder.find_cover_point(w, pools, Cover::Params{8}, {npc, body, threat, radius, octants, max_cover_end_dist, area},
                                       Search::without_critters(), true);
    };
    const auto whole = search({});
    Array<cover_spot> spots;
    arrayAppend(spots, whole.spots);
    fm_assert(spots.size() > min_spots);

    Array<point> tiled;
    arrayReserve(tiled, spots.size());
    u32 candidates = 0;
    constexpr auto n = (int)radius/tile_size_xy + 1;
    for (auto y = -n; y <= n; y++)
        for (auto x = -n; x <= n; x++)
        {
            const auto mid = Vector2i{x, y}*tile_size_xy;
            const auto r = search(Range2Di{mid - Vector2i{half_tile<int>}, mid + Vector2i{half_tile<int>}});
            candidates += r.candidates;
            for (const auto& s : r.spots)
                if (std::find(tiled.begin(), tiled.end(), s.pos) == tiled.end())
                    arrayAppend(tiled, s.pos);
        }
    fm_assert_equal(whole.candidates, candidates);
    fm_assert_equal(spots.size(), tiled.size());
    for (const auto& s : spots)
        fm_assert(std::find(tiled.begin(), tiled.end(), s.pos) != tiled.end());
}

// Spots moved along one face end at its two ends, so there are fewer of them.
void test_lists_by_area(u8 octants, u32 max_cover_end_dist = 0)
{
    auto w = world();
    add_ground(w);
    add_blocker(w, {7, 8}, {16, 96});
    add_blocker(w, {9, 4}, {96, 16});
    add_blocker(w, {4, 11}, {16, 16});
    rebuild(w);
    check_lists_by_area(w, point{ch, {6, 7}, {4, 4}}, point{ch, {12, 8}, {}}, octants, max_cover_end_dist, max_cover_end_dist ? 1 : 3);
}

// A wall of eight blockers 64 px long end to end, x [440, 456) and y [224, 736), between the npc
// and the threat. A search of one tile collects the rects of the blockers near that tile, and a
// move of 2 tiles along the wall reaches blockers past them, which hide the body there.
void test_lists_by_area_long_wall()
{
    auto w = world();
    add_ground(w);
    for (auto y = 4; y <= 11; y++)
        add_blocker(w, {7, (u8)y}, {16, 64});
    rebuild(w);
    check_lists_by_area(w, point{ch, {6, 8}, {4, 4}}, point{ch, {12, 8}, {}}, 0, 2*tile_size_xy, 1);
}

// Where find_cover_point puts the threat turned about pos by j octants, rounded to whole px. It
// rounds float math, so each component must lie at least 1e-3 px from a half pixel.
Vector2i turned_threat(Vector2i threat, Vector2i pos, int j)
{
    const auto v = Vector2d(threat - pos);
    const auto a = j * 2 * Math::Constants<double>::pi() / Cover::octant_count;
    const auto t = Vector2d{v.x()*std::cos(a) - v.y()*std::sin(a), v.x()*std::sin(a) + v.y()*std::cos(a)};
    fm_assert((Math::abs(t - Math::floor(t) - Vector2d{.5}) >= Vector2d{1e-3}).all());
    return pos + Vector2i(Math::round(t));
}

// True when no ray from threat hits a critter of size body centered on pos, put there for the
// shots. It is removed afterwards, so it can't stop the rays aimed at the next spot.
bool hidden_from(world& w, Vector2i threat, point pos)
{
    const auto c = add_critter(w, pos);
    const auto ret = cant_be_shot_at(w, at_px(threat), 0, pos, body, c->id);
    w[ch].remove_object(*c, c->index());
    return ret;
}

Array<point> copy_spots(const cover_point_result& r)
{
    Array<point> ret;
    arrayReserve(ret, r.spots.size());
    for (const auto& s : r.spots)
        arrayAppend(ret, s.pos);
    return ret;
}

// test_hides_behind_wall's first scene. With 3 octants, the spots listed are those listed with
// none that the threat turned an octant either way doesn't see, and some near the wall's ends are
// left out. Both take the same first spot, behind the wall's middle.
void test_octants_wall_middle()
{
    auto w = world();
    add_ground(w);
    add_blocker(w, {7, 8}, {16, 96});
    rebuild(w);

    const point npc{ch, {4, 8}, {}}, threat{ch, {12, 8}, {}};
    const auto T = Vector3i(threat).xy();
    constexpr auto radius = 3*tile_size_xy;
    const auto r0 = find(w, npc, threat, radius, 8, body, Search::without_critters(), true);
    fm_assert(r0.found);
    const auto spots0 = copy_spots(r0);
    const auto pos0 = r0.pos;
    const auto r3 = find(w, npc, threat, radius, 8, body, Search::without_critters(), true, 3);
    fm_assert(r3.found && r3.pos == pos0);
    const auto spots3 = copy_spots(r3);

    auto kept = 0u;
    for (const auto p : spots0)
    {
        const auto v = Vector3i(p).xy();
        fm_assert(hidden_from(w, T, p));
        const auto hidden = hidden_from(w, turned_threat(T, v, -1), p) && hidden_from(w, turned_threat(T, v, 1), p);
        const auto listed = std::find(spots3.begin(), spots3.end(), p) != spots3.end();
        fm_assert(listed == hidden);
        kept += hidden;
    }
    fm_assert_equal(kept, (u32)spots3.size());
    fm_assert(kept < spots0.size());
}

// The npc stands west of the wall near its north end, and the threat east of it, a little south.
// Without octants, each spot found lies at the north end, which the threat turned an octant north
// sees. Spots farther south hidden from all three lie past the radius, and with 3 octants the test
// from the turned threat rejects every candidate the cover grid passes.
void test_octants_turned_threat_sees()
{
    auto w = world();
    add_ground(w);
    add_blocker(w, {7, 8}, {16, 96});
    rebuild(w);

    const point npc{ch, {4, 6}, {}};
    const auto T = Vector2i{634, 499};
    constexpr auto radius = 3*tile_size_xy;
    const auto r0 = find(w, npc, at_px(T), radius, 8, body, Search::without_critters(), true);
    fm_assert(r0.found);
    const auto spots0 = copy_spots(r0);
    const auto r3 = find(w, npc, at_px(T), radius, 8, body, Search::without_critters(), true, 3);
    fm_assert(!r3.found);
    fm_assert_equal(r3.candidates, r3.rejected);
    for (const auto p : spots0)
    {
        fm_assert(hidden_from(w, T, p));
        fm_assert(!hidden_from(w, turned_threat(T, Vector3i(p).xy(), -1), p));
    }
}

// True when neither the threat nor the threat turned about pos by up to octants/2 octants either
// way hits a body centered on pos.
bool hidden_from_turned(world& w, Vector2i threat, point pos, u8 octants)
{
    const auto v = Vector3i(pos).xy();
    if (!hidden_from(w, threat, pos))
        return false;
    for (auto j = 1; j <= octants/2; j++)
        if (!hidden_from(w, turned_threat(threat, v, -j), pos) || !hidden_from(w, turned_threat(threat, v, j), pos))
            return false;
    return true;
}

// A wall 160 px long, x [440, 456) and y [432, 592), with the threat east of its middle and the
// npc west of it, 12 px north of its middle. The spot behind the wall moves north, to the nearer
// end, up to where the body 1 px farther north is hit from the threat or a turned threat. With
// north_leg, a wall going west from the north end stops that, and the spot moves to the south end.
void test_moves_to_cover_end(u8 octants, bool north_leg)
{
    auto w = world();
    add_ground(w);
    add_blocker(w, {7, 8}, {16, 160});
    if (north_leg)
        add_blocker(w, {7, 7}, {48, 16}, {-32, -8});
    rebuild(w);

    const point npc{ch, {6, 8}, {0, -12}}, threat{ch, {12, 8}, {}};
    const auto T = Vector3i(threat).xy();
    constexpr auto radius = 3*(u32)tile_size_xy, max_cover_end_dist = 2*(u32)tile_size_xy;
    const auto r0 = find(w, npc, threat, radius, 8, body, Search::without_critters(), false, octants);
    const auto r = find(w, npc, threat, radius, 8, body, Search::without_critters(), false, octants, max_cover_end_dist);
    fm_assert(r0.found && r.found && r.axis == r0.axis && r.axis == Vector2i{1, 0});
    const auto p0 = Vector3i(r0.pos).xy(), p = Vector3i(r.pos).xy();
    fm_assert(p.x() == p0.x());
    fm_assert(north_leg ? p.y() > p0.y() : p.y() < p0.y());
    fm_assert(Math::abs(p.y() - p0.y()) <= (int)max_cover_end_dist);
    fm_assert(passable(w, r.pos, body) && !passable(w, r.pos + r.axis, body));
    const auto farther = r.pos + Vector2i{0, north_leg ? 1 : -1};
    fm_assert(hidden_from_turned(w, T, r.pos, octants));
    fm_assert(!hidden_from_turned(w, T, farther, octants));
}

// test_cover_script's wall and threat. The npc hid at hidden, where the threat sees no pixel of its body.
// 1 px farther along the wall on one side, a point within threat_reach px of the threat's cell sees one.
void test_cover_script_at_wall_end(point hidden)
{
    auto w = world();
    add_ground(w);
    add_blocker(w, {7, 8}, {16, 96});
    rebuild(w);
    const auto T = Vector3i(point{ch, {12, 8}, {}}).xy();
    fm_assert(hidden_from(w, T, hidden));
    constexpr auto n = (2*tile_size_xy + 16)/8;
    const auto seen = [&](point pos) {
        for (auto j = -n; j <= n; j++)
            for (auto i = -n; i <= n; i++)
                if (i*i + j*j <= n*n && !hidden_from(w, T + Vector2i{i, j}*8, pos))
                    return true;
        return false;
    };
    fm_assert(seen(hidden + Vector2i{0, 1}) || seen(hidden - Vector2i{0, 1}));
}

struct cover_cycle { point hidden, peek; };

// Where cover_script puts the gun of a body centered on pos: offset px to the right of pos as it
// faces threat, rounded to whole px. gun_point() in critter-script-cover.cpp has the same float
// expression, whose last bits may differ under fast math, so each component must lie at least
// 1e-4 px from a half pixel.
point gun_at(point pos, point threat, int offset)
{
    if (pos == threat)
        return pos;
    const auto f = Vector2(threat - pos);
    const auto v = Vector2{-f.y(), f.x()} * ((float)offset / f.length());
    fm_assert((Math::abs(v - Math::floor(v) - Vector2{.5f}) >= Vector2{1e-4f}).all());
    return pos + Vector2i(Math::round(v));
}

// The npc walks behind the wall, stays hidden for 2 s, then peeks along the wall the least that
// lets a ray from the threat reach its gun. It stays out 2 s and walks back to the same spot, with
// another critter at other_pos throughout.
cover_cycle test_cover_script(const Optional<point>& other_pos = {})
{
    auto w = world();
    add_ground(w);
    add_blocker(w, {7, 8}, {16, 96});
    rebuild(w);

    const point threat_pos{ch, {12, 8}, {}};
    const auto threat = add_critter(w, threat_pos);
    auto npc = add_critter(w, point{ch, {4, 8}, {}});
    const auto other = other_pos ? add_critter(w, *other_pos) : bptr<critter>{};
    w.init_scripts();
    auto A = astar{};
    npc->script.do_reassign(critter_script::make_cover_script(A, threat->id, 8), npc);

    const auto fps = npc->atlas->info().fps;
    const auto frame = Second / fps;
    struct rest { point pos; u32 frames; };
    Array<rest> rests;
    arrayReserve(rests, 4);
    auto last = npc->position();
    auto still = 0u;
    // Shot at while the npc stands on its first rest, as a rest is recorded only once the npc has left it.
    Optional<bool> hidden_unshot;
    for (auto frames = 0u; rests.size() < 4; frames++)
    {
        fm_assert(frames < 60*fps);
        auto i = npc->index();
        npc->update(npc, i, frame);
        if (npc->position() == last)
        {
            if (++still == fps && rests.isEmpty())
                hidden_unshot = cant_be_shot_at(w, threat_pos, threat->id, last, body, npc->id);
            continue;
        }
        if (still >= fps)
            arrayAppend(rests, rest{last, still});
        last = npc->position();
        still = 0;
    }

    if (other)
        w[ch].kill_object(*other, other->index());

    const auto [hidden, peek, back, peek2] = std::array{rests[0], rests[1], rests[2], rests[3]};
    // each wait may end a frame early or late, and the walk's arrival frame counts as still
    const auto near_2s = [&](u32 n) { return n + 2 >= 2*fps && n <= 2*fps + 2; };
    fm_assert(near_2s(hidden.frames) && near_2s(peek.frames) && near_2s(back.frames));
    fm_assert(hidden_unshot && *hidden_unshot);
    fm_assert(back.pos == hidden.pos && peek2.pos == peek.pos);

    const auto peek_offset = Vector3i(peek.pos).xy() - Vector3i(hidden.pos).xy();
    fm_assert(peek_offset.x() == 0 && peek_offset.y() != 0);
    Grid::Pass::Pool pool{Pass::Params{8, pass_mode::shoot_through}, Search::without_critters()};
    // The peek is along y, so the body's width along the wall is body.y().
    const auto seen = [&](point p) {
        const auto gun = gun_at(p, threat_pos, (int)body.y()/2);
        return raycast(w, threat_pos, gun, threat->id, pool, Search::without_critters()).success;
    };
    fm_assert(seen(peek.pos));
    fm_assert(!seen(peek.pos - Vector2i{0, peek_offset.y() > 0 ? 1 : -1}));
    w.finish_scripts();
    return { hidden.pos, peek.pos };
}

// The wall and the npc of test_cover_script, with the threat at threat_pos and another critter at
// other_pos when given. The npc's bbox_size is npc_size while it faces north, as it starts, and
// its gun is gun_offset px to its right, or where the 3-argument make_cover_script puts it when
// gun_offset is empty.
struct cover_scene
{
    world w;
    astar A;
    Grid::Pass::Pool ray_pool{Pass::Params{8, pass_mode::shoot_through}, Search::without_critters()};
    bptr<critter> threat, npc;
    bptr<scenery> wall;
    u32 fps = 0;
    Ns frame;

    explicit cover_scene(point threat_pos = point{ch, {12, 8}, {}}, Optional<int> gun_offset = 0,
                         const Optional<point>& other_pos = {}, Vector2ui npc_size = body);
    ~cover_scene() noexcept;
    void step();
    bool seen_from(point threat_pos, point pos);
    point next_stop();
    u32 stay(u32 max, Vector2i threat_step = {});
    void move_threat(point pos);
    cover_script_status status();
    void step_until(point pos);
    void step_until(cover_script_status::state st);
    u32 finish_search();
};

cover_scene::cover_scene(point threat_pos, Optional<int> gun_offset, const Optional<point>& other_pos,
                         Vector2ui npc_size)
{
    add_ground(w);
    wall = add_blocker(w, {7, 8}, {16, 96});
    rebuild(w);
    threat = add_critter(w, threat_pos);
    npc = add_critter(w, point{ch, {4, 8}, {}}, npc_size);
    if (other_pos)
        add_critter(w, *other_pos);
    w.init_scripts();
    auto script = gun_offset ? critter_script::make_cover_script(A, threat->id, 8, *gun_offset)
                             : critter_script::make_cover_script(A, threat->id, 8);
    npc->script.do_reassign(move(script), npc);
    fps = npc->atlas->info().fps;
    frame = Second / fps;
}

cover_scene::~cover_scene() noexcept { w.finish_scripts(); }

void cover_scene::step()
{
    auto i = npc->index();
    npc->update(npc, i, frame);
}

bool cover_scene::seen_from(point threat_pos, point pos)
{
    return raycast(w, threat_pos, pos, threat->id, ray_pool, Search::without_critters()).success;
}

void cover_scene::move_threat(point pos)
{
    auto i = threat->index();
    threat->teleport_to(i, pos, rotation_COUNT);
}

cover_script_status cover_scene::status()
{
    const auto s = get_cover_script_status(*npc);
    fm_assert(s);
    return *s;
}

void cover_scene::step_until(point pos)
{
    for (auto frames = 0u; npc->position() != pos; frames++)
    {
        fm_assert(frames < 60*fps);
        step();
    }
}

void cover_scene::step_until(cover_script_status::state st)
{
    for (auto frames = 0u; status().st != st; frames++)
    {
        fm_assert(frames < 60*fps);
        step();
    }
}

// Steps while the npc searches for a retreat, or for cover, also during the retreat walk, and returns the
// number of updates that started in state::find_retreat.
u32 cover_scene::finish_search()
{
    using state = cover_script_status::state;
    const auto num_searches = status().num_searches;
    const auto searching = [&] {
        const auto t = status();
        return t.st == state::search || t.st == state::find_retreat ||
               t.st == state::retreat && t.num_searches == num_searches;
    };
    auto retreat_updates = 0u;
    for (auto frames = 0u; searching(); frames++)
    {
        fm_assert(frames < 60*fps);
        const auto t0 = status();
        step();
        const auto num_dijkstra_calls = status().num_dijkstra_calls - t0.num_dijkstra_calls;
        if (t0.st == state::find_retreat)
        {
            retreat_updates++;
            fm_assert(num_dijkstra_calls <= 4);
            fm_assert(t0.num_retreat_spots > 0 && t0.retreat_spot <= t0.num_retreat_spots);
            fm_assert(t0.num_failed_retreat_dijkstra_calls <= 8);
            // The panel's progress only moves forward, except when the threat's octant changes.
            const auto t = status();
            fm_assert(t.num_retreat_restarts - t0.num_retreat_restarts <= 1);
            if (t.st == state::find_retreat && t.num_retreat_restarts == t0.num_retreat_restarts)
                fm_assert(t.retreat_all && !t0.retreat_all || t.retreat_all == t0.retreat_all &&
                          t.retreat_spot > t0.retreat_spot);
        }
        else
            fm_assert(num_dijkstra_calls <= 1);
    }
    return retreat_updates;
}

// Steps until the npc holds a position for a second, a stop as in test_cover_script, and returns
// it while the npc still stands there.
point cover_scene::next_stop()
{
    auto last = npc->position();
    for (auto frames = 0u, still = 0u; still < fps; frames++)
    {
        fm_assert(frames < 60*fps);
        step();
        if (npc->position() != last)
        {
            last = npc->position();
            still = 0;
        }
        else
            still++;
    }
    return last;
}

// The number of updates the npc stays where it stands, at most max. The threat moves by
// threat_step before each one.
u32 cover_scene::stay(u32 max, Vector2i threat_step)
{
    const auto pos = npc->position();
    for (auto frames = 0u; ; frames++)
    {
        if (threat_step != Vector2i{})
        {
            auto i = threat->index();
            threat->teleport_to(i, threat->position() + threat_step, rotation_COUNT);
        }
        step();
        if (npc->position() != pos)
            return frames;
        fm_assert(frames < max);
    }
}

// Half a second after the npc reaches peek, the threat moves to the wall's west side, from where
// the cover spot is in the open. The npc walks around the wall to a spot east of it that the
// threat sees, stays there for the rest of its 2 s out, which the walk doesn't spend, and hides.
void test_cover_script_keeps_peeking(point hidden, point peek)
{
    cover_scene s;
    set_cover_script_options(*s.npc, {.search_every_move = true});
    for (auto frames = 0u; s.npc->position() != peek; frames++)
    {
        fm_assert(frames < 60*s.fps);
        s.step();
    }
    // The walk to peek is a straight line, which arrives on the update that reaches it.
    const auto out = s.fps/2;
    for (auto k = 0u; k < out; k++)
        s.step();
    fm_assert(s.npc->position() == peek);

    const point threat_pos{ch, {2, 8}, {}};
    auto i = s.threat->index();
    s.threat->teleport_to(i, threat_pos, rotation_COUNT);
    fm_assert(s.seen_from(threat_pos, hidden));

    // The npc is seen all the way around the wall's north end, and its first step back toward
    // cover takes its center out of sight. still counts the updates since the npc last moved.
    auto peek2 = peek;
    auto seen = 0u, still = 0u;
    for (;;)
    {
        fm_assert(seen < 60*s.fps);
        s.step();
        const auto pos = s.npc->position();
        if (!s.seen_from(threat_pos, pos))
            break;
        still = pos == peek2 ? still + 1 : 0;
        peek2 = pos;
        seen++;
    }
    fm_assert(peek2 != peek);
    // The first update after the teleport started with the npc standing at peek, so it spent a
    // frame of the wait. That frame and a wait ending a frame early or late are far less than the
    // walk's half second, so the check fails when the walk spends the wait.
    const auto standing = out + 1 + still;
    fm_assert(standing + 2 >= 2*s.fps && standing <= 2*s.fps + 2);
    fm_assert(seen - still >= s.fps/2);
    const auto hidden2 = s.next_stop();
    fm_assert(cant_be_shot_at(s.w, threat_pos, s.threat->id, hidden2, body, s.npc->id));
}

// Half a second after the npc is back at cover from its first peek, the threat moves north-east of
// the wall, from where it can shoot the npc at the cover spot. The npc walks past the wall's south
// end to a spot the wall hides from there, stays there for the rest of its 2 s hidden, which the
// walk doesn't spend, then steps out. The wall is too narrow for that spot to hide the npc from
// the threat turned an octant either way, or after it walks 2 tiles, so octants and threat_reach are 0.
void test_cover_script_follows_while_hidden()
{
    cover_scene s;
    set_cover_script_options(*s.npc, {.search_every_move = true, .octants = 0, .threat_reach = 0});
    const auto hidden = s.next_stop();
    (void)s.stay(2*s.fps);
    (void)s.next_stop();
    (void)s.stay(2*s.fps);
    // The walk back from peek is a straight line, which arrives on the update that reaches it.
    for (auto frames = 0u; s.npc->position() != hidden; frames++)
    {
        fm_assert(frames < 60*s.fps);
        s.step();
    }
    const auto before = s.fps/2;
    for (auto k = 0u; k < before; k++)
        s.step();
    fm_assert(s.npc->position() == hidden);

    const point threat_pos{ch, {9, 2}, {}};
    auto i = s.threat->index();
    s.threat->teleport_to(i, threat_pos, rotation_COUNT);
    fm_assert(!cant_be_shot_at(s.w, threat_pos, s.threat->id, hidden, body, s.npc->id));

    // Diagonal steps cost more than a frame, so some updates of a walk don't move the npc. The
    // loop ends at the npc's first move after a second without one, which is it stepping out.
    auto spot = hidden;
    auto walk = 0u, still = 0u;
    auto spot_hidden = false;
    for (;;)
    {
        fm_assert(walk + still < 60*s.fps);
        s.step();
        if (s.npc->position() == spot)
        {
            if (++still == s.fps)
                spot_hidden = cant_be_shot_at(s.w, threat_pos, s.threat->id, spot, body, s.npc->id);
            continue;
        }
        if (still >= s.fps)
            break;
        spot = s.npc->position();
        walk += still + 1;
        still = 0;
    }
    // The first update after the teleport spent a frame of the wait. The slack and the walk's
    // least length are test_cover_script_keeps_peeking's.
    const auto standing = before + 1 + still;
    fm_assert(standing + 2 >= 2*s.fps && standing <= 2*s.fps + 2);
    fm_assert(walk >= s.fps/2);
    fm_assert(spot_hidden);
}

// The threat stands 56 px east of the wall's east face, and the npc's cover 28 px west of it, so a
// ray to a spot the npc peeks from passes the face's corner two thirds of the way, and the spot
// moves 1 px for each 2 px the threat moves along the wall. With a second of its 2 s out left, the
// threat walks 1 px per frame along the wall, away from the peeking side and back, over 4 px. The
// npc follows the spot and still turns back to cover when its 2 s are up.
void test_cover_script_threat_walks_along_wall(bool search_every_move)
{
    cover_scene s{point{ch, {8, 8}, {}}};
    // From 56 px, the threat could walk around either end of the wall within threat_reach.
    set_cover_script_options(*s.npc, {.search_every_move = search_every_move, .threat_reach = 0});
    const auto hidden = s.next_stop();
    (void)s.stay(2*s.fps);
    const auto peek = s.next_stop();
    const auto peek_offset = Vector3i(peek).xy() - Vector3i(hidden).xy();
    fm_assert(peek_offset.x() == 0 && peek_offset.y() != 0);
    const auto side = peek_offset.y() > 0 ? 1 : -1;
    const auto num_searches = s.status().num_searches;

    // next_stop() returned after a second at peek.
    const auto left = s.fps;
    constexpr auto amplitude = 4u;
    auto farthest = 0;
    for (auto frames = 0u; ; frames++)
    {
        const auto dir = frames % (2*amplitude) < amplitude ? -side : side;
        s.move_threat(s.threat->position() + Vector2i{0, dir});
        s.step();
        const auto past = (Vector3i(s.npc->position()).y() - Vector3i(peek).y())*side;
        // The spots it peeks from lie on the far side of peek from the cover, so this is the walk back.
        if (past < 0)
        {
            // Those spots span less than amplitude px, and the walk back is 1 px per frame along
            // the face, so it crosses peek within amplitude + 1 frames.
            fm_assert(frames + 2 >= left && frames <= left + amplitude + 2);
            break;
        }
        farthest = Math::max(farthest, past);
        fm_assert(frames <= left + amplitude + 2);
        fm_assert(search_every_move || s.status().num_searches == num_searches);
    }
    fm_assert(farthest > 0);
}

// From the npc's arrival at cover, the threat walks east 1 px per frame, away from the wall. The npc moves along the
// wall's face as the threat starts to see its body, and still steps out when its 2 s there are up.
void test_cover_script_walking_threat(point hidden)
{
    cover_scene s;
    for (auto frames = 0u; s.npc->position() != hidden; frames++)
    {
        fm_assert(frames < 60*s.fps);
        s.step();
    }
    auto frames = 0u;
    for (;; frames++)
    {
        s.move_threat(s.threat->position() + Vector2i{1, 0});
        s.step();
        if (s.status().st == cover_script_status::state::to_peek)
            break;
        fm_assert(frames < 2*s.fps + 2);
        fm_assert(Vector3i(s.npc->position()).x() == Vector3i(hidden).x());
        fm_assert(cant_be_shot_at(s.w, s.threat->position(), s.threat->id, s.npc->position(), body, s.npc->id));
    }
    fm_assert(frames + 2 >= 2*s.fps);
}

// The npc's cover and peek spots in a cover_scene made with these arguments, after asserting that
// the threat sees the gun at the peek spot and not with the peek 1 px shorter. gun_offset is as in
// cover_scene.
cover_cycle peek_with_gun(Optional<int> gun_offset, const Optional<point>& other_pos = {}, Vector2ui npc_size = body,
                          u32 max_cover_end_dist = cover_script_options{}.max_cover_end_dist)
{
    cover_scene s{point{ch, {12, 8}, {}}, gun_offset, other_pos, npc_size};
    set_cover_script_options(*s.npc, {.max_cover_end_dist = max_cover_end_dist});
    const auto hidden = s.next_stop();
    // The peek is along y, so the default is half of bbox_size.y(). find_peek_pos() reads bbox_size
    // at hidden, and each walk turns the npc, which rotates bbox_size.
    const auto offset = gun_offset ? *gun_offset : (int)s.npc->bbox_size.y()/2;
    (void)s.stay(2*s.fps);
    const auto peek = s.next_stop();
    const auto peek_offset = Vector3i(peek).xy() - Vector3i(hidden).xy();
    fm_assert(peek_offset.x() == 0 && peek_offset.y() != 0);
    const auto side = peek_offset.y() > 0 ? 1 : -1;
    const auto threat = s.threat->position();
    fm_assert(s.seen_from(threat, gun_at(peek, threat, offset)));
    fm_assert(!s.seen_from(threat, gun_at(peek - Vector2i{0, side}, threat, offset)));
    return { hidden, peek };
}

// The npc faces the threat in the east, and north is -y, so its right is +y. Compared with a gun at
// its center, a gun to its right shortens the peek to its right and lengthens the one to its left,
// and a gun to its left does the reverse. The default is half the body's width to its right, which
// peek_with_gun tells from half its depth, since a 30 by 24 px body's two differ.
void test_cover_script_gun_offset()
{
    constexpr auto right = 1, left = -1;
    const auto hidden = peek_with_gun(0, {}, body, 0).hidden;
    // A critter stands 1 px past the body at hidden on the side not tested.
    const auto peek_dist = [&](int offset, int side) {
        const auto c = peek_with_gun(offset, hidden - Vector2i{0, side*((int)body.y() + 1)}, body, 0);
        fm_assert(c.hidden == hidden);
        const auto dy = Vector3i(c.peek).y() - Vector3i(c.hidden).y();
        fm_assert(dy*side > 0);
        return Math::abs(dy);
    };
    const auto r0 = peek_dist(0, right), l0 = peek_dist(0, left);
    const auto r7 = peek_dist(7, right), l7 = peek_dist(7, left);
    const auto r11 = peek_dist(-11, right), l11 = peek_dist(-11, left);
    fm_assert(r7 < r0 && r0 < r11);
    fm_assert(l7 > l0 && l0 > l11);

    const auto d = peek_with_gun({}, {}, body, 0);
    fm_assert(d.hidden == hidden);
    fm_assert(d.peek == peek_with_gun((int)body.y()/2, {}, body, 0).peek);
    fm_assert(Vector3i(d.peek).y() > Vector3i(hidden).y());
    (void)peek_with_gun({}, {}, Vector2ui{30, 24}, 0);
}

// A critter stands 1 px past the body at the npc's peek spot, on the far side from the cover. The
// peek distance there, 44 px, is no multiple of find_peek_pos's step of half the body, so the first
// step past it is blocked, and the npc still peeks from that spot.
void test_cover_script_peek_beside_critter()
{
    const auto c = peek_with_gun({}, {}, body, 0);
    const auto side = Vector3i(c.peek).y() > Vector3i(c.hidden).y() ? 1 : -1;
    const auto c2 = peek_with_gun({}, c.peek + Vector2i{0, side*((int)body.y() + 1)}, body, 0);
    fm_assert(c2.hidden == c.hidden && c2.peek == c.peek);
}

// Whether a ray from threat reaches the gun of a body centered on pos, where the 3-argument
// make_cover_script puts it for a cover spot with its axis along x.
bool gun_seen(world& w, point threat, point pos)
{
    Grid::Pass::Pool pool{Pass::Params{8, pass_mode::shoot_through}, Search::without_critters()};
    return raycast(w, threat, gun_at(pos, threat, (int)body.y()/2), 0, pool, Search::without_critters()).success;
}

// The walk from spot to find_cover_peek's spot for it, for an npc standing on spot with the cover
// script and its default gun point, and its threat at threat_pos. w holds the blockers. It asserts
// that the threat sees the gun at the peek spot, and doesn't at spot or 1 px back toward it.
Vector2i peek_walk(world& w, const cover_spot& spot, point threat_pos)
{
    rebuild(w);
    const auto threat = add_critter(w, threat_pos);
    const auto npc = add_critter(w, spot.pos);
    w.init_scripts();
    auto A = astar{};
    npc->script.do_reassign(critter_script::make_cover_script(A, threat->id, 8), npc);
    set_cover_script_options(*npc, {.max_cover_end_dist = 0});
    const auto p = find_cover_peek(*npc, spot);
    w.finish_scripts();
    fm_assert(p);
    const auto d = Vector3i(p->pos).xy() - Vector3i(spot.pos).xy();
    const auto back = Vector2i{Math::sign(d.x()), Math::sign(d.y())};
    fm_assert(gun_seen(w, threat_pos, p->pos));
    fm_assert(!gun_seen(w, threat_pos, p->pos - back) && !gun_seen(w, threat_pos, spot.pos));
    return d;
}

// A 64 px pillar, and a spot flush against its west face, 48 px north of its south-west corner.
// The threat stands 4 tiles to the south-east. The gun's ray to the threat clears that corner after
// about 23 px of a walk south, 27 px west and 18 px south-west, so the npc peeks south-west.
void test_cover_script_peeks_diagonally()
{
    auto w = world();
    add_ground(w);
    add_blocker(w, {8, 8}, {64, 64});
    const cover_spot spot{point{ch, {7, 8}, {20, -16}}, {1, 0}};
    const auto threat = spot.pos + Vector2i{256, 256};
    const auto d = peek_walk(w, spot, threat);
    fm_assert(d.x() < 0 && d.y() == -d.x());
    for (auto i = 1; i*i <= 2*d.y()*d.y(); i++)
    {
        fm_assert(!gun_seen(w, threat, spot.pos + Vector2i{0, i}));
        fm_assert(!gun_seen(w, threat, spot.pos - Vector2i{i, 0}));
    }
}

// An L: a wall 192 px long from north to south, and one 32 px long west from its south end. The
// spot is in the corner, flush against both, with its axis toward the long wall in the east. The
// threat stands 2 tiles east and 4 tiles south, so the short wall hides the gun, and only a walk
// west or north-west clears it, the west one sooner.
void test_cover_script_peeks_from_corner()
{
    auto w = world();
    add_ground(w);
    add_blocker(w, {8, 7}, {16, 192});
    add_blocker(w, {8, 8}, {32, 16}, {-24, 24});
    const cover_spot spot{point{ch, {8, 8}, {-20, 4}}, {1, 0}};
    const auto d = peek_walk(w, spot, spot.pos + Vector2i{128, 256});
    fm_assert(d.x() < 0 && d.y() == 0);
}

// The one of the 8 rotations nearest v's direction, from v's angle clockwise from north, which is
// -y. The angle must lie at least 1° from a boundary between two of them, so that atan2's rounding
// can't change the result.
rotation nearest_of_8(Vector2i v)
{
    const auto octants = std::atan2((double)v.x(), (double)-v.y()) / (Math::Constants<double>::pi()/4);
    const auto k = std::round(octants);
    fm_assert(.5 - std::abs(octants - k) >= 1./45);
    return rotation((u8)(((int)k + 8) % 8));
}

// The npc's gun is 23 px to its left, and a critter stands 1 px north of the body at hidden, so the
// npc peeks past the wall's south end. Half a second after it arrives there, the threat moves to 13
// px east and 3 px north of the wall's south-east corner, where the same gun spot is still the
// first the threat sees. The npc stays, faces the threat's new direction until its 2 s out are up,
// and casts two rays, to its gun and its cover, only in the update after the move, never searching.
void test_cover_script_faces_threat()
{
    constexpr auto gun_offset = -23;
    const auto hidden = peek_with_gun(0, {}, body, 0).hidden;
    const auto north = hidden - Vector2i{0, (int)body.y() + 1};
    const auto peek = peek_with_gun(gun_offset, north, body, 0).peek;
    cover_scene s{point{ch, {12, 8}, {}}, gun_offset, north};
    set_cover_script_options(*s.npc, {.max_cover_end_dist = 0});
    for (auto frames = 0u; s.npc->position() != peek; frames++)
    {
        fm_assert(frames < 60*s.fps);
        s.step();
    }
    const auto r1 = nearest_of_8(s.threat->position() - peek);
    fm_assert(s.npc->r == r1);
    const auto out = s.fps/2;
    const auto t0 = s.status();
    const auto threat0 = s.threat->position();
    fm_assert(t0.gun_ray_from == threat0 && t0.gun == gun_at(peek, threat0, gun_offset));
    for (auto k = 0u; k < out; k++)
    {
        s.step();
        fm_assert(s.npc->position() == peek && s.npc->r == r1);
        fm_assert_equal(t0.num_rays, s.status().num_rays);
    }

    const point threat_pos{ch, {7, 9}, {21, -19}};
    s.move_threat(threat_pos);
    const auto r2 = nearest_of_8(threat_pos - peek);
    fm_assert(r2 != r1);
    const auto gun2 = gun_at(peek, threat_pos, gun_offset);
    fm_assert(gun2 != t0.gun);
    auto frames = 0u;
    for (;;)
    {
        s.step();
        const auto t = s.status();
        fm_assert_equal(t0.num_searches, t.num_searches);
        fm_assert_equal(t0.num_rays + 1, t.num_rays);
        fm_assert_equal(t0.num_body_checks + 1, t.num_body_checks);
        if (s.npc->position() != peek)
            break;
        fm_assert(s.npc->r == r2);
        fm_assert(t.gun_ray_from == threat_pos && t.gun == gun2);
        fm_assert(++frames <= 2*s.fps + 2);
    }
    // As in test_cover_script, a wait may end a frame early or late. The update in which the wait
    // runs out starts the walk back without a move, so frames counts it.
    const auto standing = out + frames;
    fm_assert(standing + 2 >= 2*s.fps && standing <= 2*s.fps + 2);
}

using state = cover_script_status::state;

// The npc of cover_scene hides at hidden, and stays without searching while the threat walks east 1 px per frame.
// When the threat moves north-west of the wall, the npc retreats at once and hides at cover its search finds.
void test_cover_script_hidden_checks(point hidden)
{
    cover_scene s;
    fm_assert(!get_cover_script_status(*s.threat));
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == hidden);
    const auto t0 = s.status();
    fm_assert(t0.has_cover && t0.cover == hidden && !t0.has_peek);
    fm_assert_equal(1u, t0.num_searches);
    fm_assert_equal(1u, t0.num_dijkstra_calls);
    for (auto k = 0u; k < 10; k++)
    {
        s.step();
        fm_assert_equal(t0.num_body_checks, s.status().num_body_checks);
    }
    for (auto k = 1u; k <= 15; k++)
    {
        s.move_threat(s.threat->position() + Vector2i{1, 0});
        s.step();
        const auto t = s.status();
        fm_assert(s.npc->position() == hidden && t.st == state::hidden);
        fm_assert_equal(1u, t.num_searches);
        fm_assert_equal(t0.num_rays, t.num_rays);
        fm_assert_equal(t0.num_body_checks + 2*k, t.num_body_checks);
        fm_assert_equal(1u, t.num_dijkstra_calls);
    }

    const point threat_pos{ch, {2, 4}, {}};
    s.move_threat(threat_pos);
    fm_assert(s.seen_from(threat_pos, hidden));
    s.step();
    s.finish_search();
    const auto t = s.status();
    fm_assert_equal(2u, t.num_searches);
    // the retreat's and the search's
    fm_assert_equal(3u, t.num_dijkstra_calls);
    fm_assert(t.st == state::to_cover && t.cover != hidden);
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == t.cover);
    fm_assert(cant_be_shot_at(s.w, threat_pos, s.threat->id, t.cover, body, s.npc->id));
}

using hold_phase = cover_script_options::hold_phase;

// The npc of cover_scene steps through hold's values, staying put 4 s in each held phase. It steps
// out when hold becomes exposed, peeks for its whole 2 s out once hold is none, and, held hidden
// again, still walks to new cover when the threat moves to where it sees hidden.
void test_cover_script_holds_hidden(point hidden, point peek)
{
    cover_scene s;
    const auto stays_at = [&](point pos, state st) {
        for (auto k = 0u; k < 4*s.fps; k++)
        {
            s.step();
            fm_assert(s.npc->position() == pos && s.status().st == st);
        }
    };
    set_cover_script_options(*s.npc, {.hold = hold_phase::hidden});
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == hidden);
    stays_at(hidden, state::hidden);

    set_cover_script_options(*s.npc, {.hold = hold_phase::exposed});
    s.step();
    fm_assert(s.status().st == state::to_peek);
    s.step_until(peek);
    stays_at(peek, state::exposed);

    set_cover_script_options(*s.npc, {});
    // As in test_cover_script_faces_threat.
    const auto frames = s.stay(2*s.fps + 3);
    fm_assert(frames + 2 >= 2*s.fps);
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == hidden);

    set_cover_script_options(*s.npc, {.hold = hold_phase::hidden});
    const point threat_pos{ch, {2, 4}, {}};
    s.move_threat(threat_pos);
    fm_assert(s.seen_from(threat_pos, hidden));
    s.step();
    s.finish_search();
    const auto t = s.status();
    fm_assert(t.st == state::to_cover && t.cover != hidden);
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == t.cover);
    stays_at(t.cover, state::hidden);
}

// The npc of cover_scene, held exposed from the start, hides at hidden for one update and steps
// out to peek. Half a second later the threat moves as in test_cover_script_flanked, and the npc
// leaves for the cover its search finds, hides there for one update and steps out again.
// Held hidden from there, it walks back to the new cover and stays.
void test_cover_script_holds_exposed(point hidden, point peek)
{
    cover_scene s;
    set_cover_script_options(*s.npc, {.hold = hold_phase::exposed});
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == hidden);
    s.step();
    fm_assert(s.status().st == state::to_peek);
    s.step_until(peek);
    for (auto k = 0u; k < s.fps/2; k++)
    {
        s.step();
        fm_assert(s.npc->position() == peek && s.status().st == state::exposed);
    }

    const point threat_pos{ch, {2, 8}, {}};
    s.move_threat(threat_pos);
    fm_assert(s.seen_from(threat_pos, hidden));
    s.step();
    s.finish_search();
    fm_assert(s.status().st == state::to_cover);
    const auto cover = s.status().cover;
    fm_assert(cover != hidden);
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == cover);
    s.step();
    fm_assert(s.status().st == state::to_peek);
    s.step_until(state::exposed);
    const auto peek2 = s.npc->position();
    fm_assert(peek2 != cover);

    set_cover_script_options(*s.npc, {.hold = hold_phase::hidden});
    s.step();
    fm_assert(s.status().st == state::to_cover);
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == cover);
    for (auto k = 0u; k < 4*s.fps; k++)
    {
        s.step();
        fm_assert(s.npc->position() == cover && s.status().st == state::hidden);
    }
}

// The npc of cover_scene, held hidden at hidden, is sent to peek from there and stays out 4 s,
// since the order moves hold to exposed. Sent back to cover, it stays hidden 4 s.
void test_cover_script_hold_follows_orders(point hidden, point peek)
{
    cover_scene s;
    set_cover_script_options(*s.npc, {.hold = hold_phase::hidden});
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == hidden);
    Array<cover_spot> spots;
    list_cover_spots(*s.npc, spots);
    fm_assert(!spots.isEmpty() && spots[0].pos == s.status().canonical);
    const auto stays_at = [&](point pos, state st) {
        for (auto k = 0u; k < 4*s.fps; k++)
        {
            s.step();
            fm_assert(s.npc->position() == pos && s.status().st == st);
        }
    };

    fm_assert(send_to_peek(*s.npc, spots[0]));
    s.step_until(peek);
    stays_at(peek, state::exposed);

    fm_assert(send_to_cover(*s.npc, spots[0]));
    s.step_until(hidden);
    stays_at(hidden, state::hidden);
}

// Half a second after the npc reaches peek, the threat moves west of the wall and sees the cover spot. The npc
// retreats at once and hides behind the wall's east face, found by its search on the way.
void test_cover_script_flanked(point hidden, point peek)
{
    cover_scene s;
    s.step_until(peek);
    for (auto k = 0u; k < s.fps/2; k++)
        s.step();
    fm_assert(s.npc->position() == peek);
    const auto t0 = s.status();
    fm_assert(t0.st == state::exposed && t0.cover == hidden && t0.has_peek && t0.peek == peek);

    const point threat_pos{ch, {2, 8}, {}};
    s.move_threat(threat_pos);
    fm_assert(s.seen_from(threat_pos, hidden));
    s.step();
    fm_assert(s.status().st == state::retreat);
    s.finish_search();
    const auto t = s.status();
    fm_assert_equal(t0.num_searches + 1, t.num_searches);
    fm_assert(t.st == state::to_cover && Vector3i(t.cover).x() > Vector3i(hidden).x());
    fm_assert(s.npc->position() != peek);
    const auto cover = t.cover;
    // A walk that is blocked would send the npc to seek.
    for (auto frames = 0u; s.status().st != state::hidden; frames++)
    {
        fm_assert(frames < 60*s.fps);
        fm_assert(s.status().st == state::to_cover);
        s.step();
    }
    fm_assert(s.npc->position() == cover);
    fm_assert_equal(t0.num_searches + 1, s.status().num_searches);
    fm_assert(cant_be_shot_at(s.w, threat_pos, s.threat->id, cover, body, s.npc->id));
}

// As test_cover_script_flanked, with the threat north of the wall, where nothing in the chunk hides the npc. The
// npc walks at once to a spot over 3 tiles farther from the threat.
void test_cover_script_flanked_retreat(point peek)
{
    cover_scene s;
    s.step_until(peek);
    for (auto k = 0u; k < s.fps/2; k++)
        s.step();
    const auto t0 = s.status();
    fm_assert(t0.st == state::exposed);

    const point threat_pos{ch, {7, 1}, {}};
    s.move_threat(threat_pos);
    fm_assert(s.seen_from(threat_pos, t0.cover));
    s.step();
    s.finish_search();
    fm_assert(s.status().st == state::retreat);
    s.step_until(state::seek);
    fm_assert_equal(t0.num_searches + 1, s.status().num_searches);
    const auto dist = [&](point p) { return std::sqrt((double)point::distance_squared(p, threat_pos)); };
    fm_assert(dist(s.npc->position()) - dist(peek) > 3*tile_size_xy);
}

// Adds three blockers north, east and south of the cover behind cover_scene's wall's east face, and
// returns the south one. With the wall they close every way there, since each gap they leave is
// narrower than the body, and the east one starts 10 px below the north one, so that the searches
// below reach the cover east of it after the walled-in cover.
bptr<scenery> wall_in_east_cover(world& w)
{
    add_blocker(w, {8, 7}, {58, 16}, {-31, 0});
    add_blocker(w, {8, 8}, {16, 126}, {-10, 17});
    auto south = add_blocker(w, {8, 9}, {58, 16}, {-31, 7});
    rebuild(w);
    return south;
}

// For scenes with wall_in_east_cover. needs_peek and threat_reach would skip walled-in spots before their Dijkstra
// search, which the tests count.
cover_script_options walled_in_options()
{
    auto o = cover_script_options{};
    o.threat_reach = 0;
    o.needs_peek = false;
    return o;
}

bool walled_in(point pos)
{
    const auto v = Vector3i(pos).xy();
    const auto h = Vector2i(body)/2;
    return v.x() - h.x() >= 456 && v.x() + h.x() <= 494 && v.y() - h.y() >= 456 && v.y() + h.y() <= 575;
}

// As in test_cover_script_flanked, with wall_in_east_cover's blockers added at peek. Measured from the
// retreat's end, the spot east of the blockers comes first, and the npc tries no walled-in spot.
void test_cover_script_flanked_searches_from_retreat_end(point peek)
{
    const point west{ch, {2, 8}, {}};
    cover_scene s;
    s.step_until(peek);
    for (auto k = 0u; k < s.fps/2; k++)
        s.step();
    const auto t0 = s.status();
    fm_assert(t0.st == state::exposed);
    wall_in_east_cover(s.w);

    s.move_threat(west);
    const auto r = find(s.w, peek, west, 5*tile_size_xy, 8, body, Search::never_continue(), false,
                        cover_script_options{}.octants);
    fm_assert(r.found && walled_in(r.pos));
    s.step();
    s.finish_search();
    fm_assert_equal(t0.num_searches + 1, s.status().num_searches);
    fm_assert(s.npc->position() != peek);
    const auto t1 = s.status();
    fm_assert(t1.st == state::to_cover);
    // the retreat's and the cover's
    fm_assert_equal(t0.num_dijkstra_calls + 2, t1.num_dijkstra_calls);
    fm_assert(Vector3i(t1.cover).x() - (int)body.x()/2 >= 510);
    s.step_until(t1.cover);
    fm_assert(s.status().st == state::hidden);
    fm_assert(!s.seen_from(west, t1.cover));
}

// The threat stands on the wall's west side from the start, with wall_in_east_cover's blockers. The npc's first
// search, from its start, finds the walled-in cover first and, with no path there, takes the next spot with one.
void test_cover_script_seek_no_path()
{
    const point west{ch, {2, 8}, {}};
    cover_scene s{west};
    set_cover_script_options(*s.npc, walled_in_options());
    wall_in_east_cover(s.w);
    const auto t0 = s.status();
    const auto r = find(s.w, s.npc->position(), west, 5*tile_size_xy, 8, body, Search::never_continue(), false,
                        cover_script_options{}.octants);
    fm_assert(r.found && walled_in(r.pos));
    s.step();
    fm_assert(s.status().num_dijkstra_calls - t0.num_dijkstra_calls <= 1);
    s.finish_search();
    const auto t1 = s.status();
    fm_assert_equal(t0.num_searches + 1, t1.num_searches);
    fm_assert(t1.st == state::to_cover);
    fm_assert_equal(t0.num_dijkstra_calls + 3, t1.num_dijkstra_calls);
    fm_assert(Vector3i(t1.cover).x() - (int)body.x()/2 >= 510);
    s.step_until(t1.cover);
    fm_assert(s.status().st == state::hidden);
    fm_assert(!s.seen_from(west, t1.cover));
}

// As in test_cover_script_seek_no_path, with max_cover_end_dist 0, which leaves a walled-in spot in each cell
// along the wall. joins_unreachable ties them to the first, which had no path, so the search tries no other.
void test_cover_script_no_path_ties()
{
    const point west{ch, {2, 8}, {}};
    cover_scene s{west};
    auto o = walled_in_options();
    o.max_cover_end_dist = 0;
    set_cover_script_options(*s.npc, o);
    wall_in_east_cover(s.w);
    const auto t0 = s.status();
    Array<cover_spot> spots;
    list_cover_spots(*s.npc, cover_query{s.npc->position(), body, west, 5*tile_size_xy, o.octants}, spots);
    fm_assert(std::count_if(spots.begin(), spots.end(), [](const cover_spot& x) { return walled_in(x.pos); }) > 2);
    s.step();
    s.finish_search();
    const auto t1 = s.status();
    fm_assert(t1.st == state::to_cover);
    fm_assert_equal(t0.num_dijkstra_calls + 2, t1.num_dijkstra_calls);
    fm_assert(Vector3i(t1.cover).x() - (int)body.x()/2 >= 510);
}

struct no_path_search
{
    cover_script_status before;
    bptr<scenery> south;
};

// test_cover_script_seek_no_path's scene and options, for a cover_scene built with the threat there, up to the
// update that finds no path to the nearest walled-in spot. Returns the status from before the first update.
no_path_search start_no_path_search(cover_scene& s)
{
    set_cover_script_options(*s.npc, walled_in_options());
    const auto before = s.status();
    auto south = wall_in_east_cover(s.w);
    for (auto frames = 0u; s.status().num_dijkstra_calls == before.num_dijkstra_calls; frames++)
    {
        fm_assert(frames < 60*s.fps);
        s.step();
        fm_assert(s.status().st == state::search);
    }
    fm_assert_equal(before.num_dijkstra_calls + 1, s.status().num_dijkstra_calls);
    return { before, move(south) };
}

// Two scenes of start_no_path_search, in one of which the threat then moves to `to`. In the same octant the next
// update lists the search's tile again. In another the search starts over and skips the spot with no path.
void test_cover_script_no_path_threat_moves(point to, bool same_octant)
{
    const point west{ch, {2, 8}, {}};
    cover_scene s{west}, still{west};
    const auto start = s.npc->position();
    fm_assert(same_octant == (nearest_octant(Vector2(to - start)) == nearest_octant(Vector2(west - start))));
    const auto t0 = start_no_path_search(s).before;
    (void)start_no_path_search(still);
    const auto num_calls = s.status().num_find_cover_point_calls;
    const auto still_num_calls = still.status().num_find_cover_point_calls;
    s.move_threat(to);
    s.step();
    still.step();
    if (same_octant)
        fm_assert_equal(still.status().num_find_cover_point_calls - still_num_calls + 1,
                        s.status().num_find_cover_point_calls - num_calls);
    s.finish_search();
    const auto t1 = s.status();
    fm_assert(t1.st == state::to_cover);
    // Each walled-in spot gets one Dijkstra search.
    fm_assert_equal(t0.num_dijkstra_calls + 3, t1.num_dijkstra_calls);
    fm_assert(Vector3i(t1.cover).x() - (int)body.x()/2 >= 510);
    fm_assert(!s.seen_from(to, t1.cover));
}

// As in start_no_path_search, then the south blocker goes away. The changed pass grids start the search over,
// which forgets that the walled-in spot had no path, and the npc takes it.
void test_cover_script_no_path_opened()
{
    const point west{ch, {2, 8}, {}};
    cover_scene s{west};
    auto south = start_no_path_search(s).south;
    const auto o = walled_in_options();
    const auto r = find(s.w, s.npc->position(), west, 5*tile_size_xy, 8, body, Search::never_continue(), false,
                        o.octants, o.max_cover_end_dist);
    fm_assert(r.found && walled_in(r.pos));
    const auto gen = s.w.pass_gen();
    s.w[ch].kill_object(*south, south->index());
    fm_assert(s.w.pass_gen() != gen);
    s.finish_search();
    const auto t1 = s.status();
    fm_assert(t1.st == state::to_cover);
    // The spot that had no path. The other walled-in one is more than a tile away.
    fm_assert(walled_in(t1.cover) && point::distance(t1.cover, r.pos) < tile_size_xy);
    s.step_until(t1.cover);
    fm_assert(!s.seen_from(west, t1.cover));
}

// As in start_no_path_search, then a blocker appears 8 chunks east, past where a path from the npc can reach. The
// search goes on knowing that the walled-in spots have no path, so it tries neither again.
void test_cover_script_no_path_far_change()
{
    cover_scene s{point{ch, {2, 8}, {}}};
    const auto t0 = start_no_path_search(s).before;
    const auto gen = s.w.pass_gen();
    add_blocker(s.w, global_coords{chunk_coords_{8, 0, 0}, {8, 8}}, {16, 16});
    fm_assert(s.w.pass_gen() != gen);
    s.finish_search();
    const auto t1 = s.status();
    fm_assert(t1.st == state::to_cover);
    fm_assert_equal(t0.num_dijkstra_calls + 3, t1.num_dijkstra_calls);
    fm_assert(Vector3i(t1.cover).x() - (int)body.x()/2 >= 510);
}

// The npc of cover_scene walks to cover, without searching while the threat walks east 1 px per frame. When the
// threat moves to the wall's west side, the npc retreats at once and hides behind its east face, never at hidden.
void test_cover_script_walk_checks(point hidden)
{
    cover_scene s;
    s.step();
    s.finish_search();
    const auto t0 = s.status();
    for (auto k = 0u; k < 10; k++)
    {
        s.step();
        fm_assert(s.status().st == state::to_cover);
        fm_assert_equal(t0.num_body_checks, s.status().num_body_checks);
    }
    for (auto k = 1u; k <= 15; k++)
    {
        s.move_threat(s.threat->position() + Vector2i{1, 0});
        s.step();
        const auto t = s.status();
        fm_assert(t.st == state::to_cover && t.cover == hidden);
        fm_assert_equal(1u, t.num_searches);
        fm_assert_equal(t0.num_rays, t.num_rays);
        fm_assert_equal(t0.num_body_checks + 2*k, t.num_body_checks);
    }

    const point threat_pos{ch, {2, 8}, {}};
    s.move_threat(threat_pos);
    fm_assert(s.seen_from(threat_pos, hidden));
    s.step();
    s.finish_search();
    const auto t = s.status();
    fm_assert_equal(2u, t.num_searches);
    fm_assert(t.st == state::to_cover && t.cover != hidden);
    for (auto frames = 0u; s.status().st != state::hidden; frames++)
    {
        fm_assert(frames < 60*s.fps);
        fm_assert(s.npc->position() != hidden);
        s.step();
    }
    fm_assert(s.npc->position() == t.cover);
    fm_assert(cant_be_shot_at(s.w, threat_pos, s.threat->id, t.cover, body, s.npc->id));
}

// The npc of cover_scene steps out toward peek. The threat moves 32 px along the wall away from the
// peek side, so the wall hides the gun, and the npc turns to a spot farther along without
// searching. When the threat then moves to the wall's west side, the npc searches and hides behind
// the wall's east face for 2 s.
void test_cover_script_peek_walk_checks(point hidden, point peek)
{
    cover_scene s;
    s.step_until(state::to_peek);
    const auto t0 = s.status();
    fm_assert(t0.has_peek && t0.peek == peek);
    const auto side = Vector3i(peek).y() > Vector3i(hidden).y() ? 1 : -1;
    s.step();
    fm_assert(s.npc->position() != hidden);

    const auto threat0 = s.threat->position() + Vector2i{1, 0};
    s.move_threat(threat0);
    fm_assert(s.seen_from(threat0, peek));
    s.step();
    const auto t = s.status();
    fm_assert_equal(t0.num_searches, t.num_searches);
    fm_assert_equal(t0.num_rays + 1, t.num_rays);
    fm_assert_equal(t0.num_body_checks + 1, t.num_body_checks);
    fm_assert(t.st == state::to_peek && t.peek == peek);

    const auto threat1 = s.threat->position() - Vector2i{0, 32*side};
    s.move_threat(threat1);
    fm_assert(!s.seen_from(threat1, peek));
    s.step();
    const auto t1 = s.status();
    fm_assert_equal(t0.num_searches, t1.num_searches);
    fm_assert(t1.st == state::to_peek && t1.has_peek);
    fm_assert((Vector3i(t1.peek).y() - Vector3i(peek).y())*side > 0);
    fm_assert(s.seen_from(threat1, t1.peek));

    const point threat2{ch, {2, 8}, {}};
    s.move_threat(threat2);
    fm_assert(s.seen_from(threat2, hidden));
    s.step();
    s.finish_search();
    const auto t2 = s.status();
    fm_assert_equal(t0.num_searches + 1, t2.num_searches);
    fm_assert(t2.st == state::to_cover && t2.cover != hidden);
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == t2.cover);
    const auto frames = s.stay(2*s.fps + 2);
    fm_assert(frames + 2 >= 2*s.fps);
}

// The npc of cover_scene stands at peek for a second. The threat then moves 32 px along the wall,
// away from the peek side, so the wall hides the cover and the gun, and the npc walks to a spot
// farther along. On the way the threat moves to the wall's west side, and the npc walks to cover
// behind the wall's east face and hides there for its full 2 s, the second left over not counting.
void test_cover_script_peek_again_flanked(point hidden, point peek)
{
    cover_scene s;
    s.step_until(peek);
    for (auto k = 0u; k < s.fps; k++)
        s.step();
    fm_assert(s.npc->position() == peek);
    const auto t0 = s.status();
    fm_assert(t0.st == state::exposed);

    const auto side = Vector3i(peek).y() > Vector3i(hidden).y() ? 1 : -1;
    const auto threat1 = s.threat->position() - Vector2i{0, 32*side};
    s.move_threat(threat1);
    fm_assert(!s.seen_from(threat1, peek) && !s.seen_from(threat1, hidden));
    s.step();
    const auto t1 = s.status();
    fm_assert_equal(t0.num_searches, t1.num_searches);
    fm_assert(t1.st == state::to_peek && t1.has_peek);
    fm_assert((Vector3i(t1.peek).y() - Vector3i(peek).y())*side > 0);

    const point threat2{ch, {2, 8}, {}};
    s.move_threat(threat2);
    fm_assert(s.seen_from(threat2, hidden));
    s.step();
    s.finish_search();
    const auto t2 = s.status();
    fm_assert_equal(t0.num_searches + 1, t2.num_searches);
    fm_assert(t2.st == state::to_cover && t2.cover != hidden);
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == t2.cover);
    const auto frames = s.stay(2*s.fps + 2);
    fm_assert(frames + 2 >= 2*s.fps);
}

// The threat walks east 1 px per frame past the hidden npc. With search_every_move the npc searches on every move,
// otherwise it tests its body at canonical and cover, and doesn't search. Either way it stays.
void test_cover_script_search_every_move(point hidden)
{
    for (const auto every : { false, true })
    {
        cover_scene s;
        auto o = cover_script_options{.search_every_move = every};
        // Without these, each search would cast peek_spot's rays and test its spot from where the threat can walk.
        if (every)
        {
            o.threat_reach = 0;
            o.needs_peek = false;
        }
        set_cover_script_options(*s.npc, o);
        s.step_until(state::hidden);
        // Without threat_reach, cover_end slides the spot past hidden.
        const auto spot = s.npc->position();
        fm_assert(every || spot == hidden);
        const auto t0 = s.status();
        for (auto k = 1u; k <= 15; k++)
        {
            s.move_threat(s.threat->position() + Vector2i{1, 0});
            s.step();
            const auto t = s.status();
            fm_assert(s.npc->position() == spot && t.st == state::hidden);
            fm_assert_equal(t0.num_searches + (every ? k : 0), t.num_searches);
            fm_assert_equal(t0.num_rays, t.num_rays);
            fm_assert_equal(t0.num_body_checks + (every ? 0 : 2*k), t.num_body_checks);
            fm_assert(t.last_update <= t.max_update && t.max_update <= t.total_update);
        }
        fm_assert(s.status().total_update > t0.total_update);
    }
}

// The npc of cover_scene hides at hidden. The threat moves 7 tiles north of the wall's middle,
// where hidden is in the open and the wall's shadow is too narrow for the body, so the npc finds no
// cover in the chunk and walks to a spot within 5 tiles of hidden that lies more than 3 tiles
// farther from the threat. It starts to search again after 1 s, and faces the threat from the
// update it arrives, also when the threat stands 2 tiles beside it for 3 updates halfway through.
void test_cover_script_retreat(point hidden)
{
    constexpr auto radius = 5*tile_size_xy;
    cover_scene s;
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == hidden);
    const auto t0 = s.status();

    const point threat_pos{ch, {7, 1}, {}};
    s.move_threat(threat_pos);
    fm_assert(s.seen_from(threat_pos, hidden));
    fm_assert(!find(s.w, hidden, threat_pos, chunk_size_xy, 8).found);
    s.step();
    s.finish_search();
    fm_assert(s.status().st == state::retreat);
    s.step_until(state::seek);
    fm_assert_equal(t0.num_searches + 1, s.status().num_searches);
    const auto spot = s.npc->position();
    fm_assert(point::distance_squared(spot, hidden) <= (u64)(radius*radius));
    const auto dist = [&](point p) { return std::sqrt((double)point::distance_squared(p, threat_pos)); };
    fm_assert(dist(spot) - dist(hidden) > 3*tile_size_xy);
    const auto r1 = nearest_of_8(threat_pos - spot);
    fm_assert(s.npc->r == r1);
    // toward the chunk's middle, where the threat's x is
    const auto side = Vector3i(spot).x() < Vector3i(threat_pos).x() ? 1 : -1;
    const auto beside = spot + Vector2i{side*2*tile_size_xy, 0};
    const auto r2 = nearest_of_8(beside - spot);
    fm_assert(r2 != r1);

    const auto moved = [&](u32 frame) { return frame >= s.fps/2 && frame < s.fps/2 + 3; };
    auto frames = 0u;
    for (; s.status().st == state::seek; frames++)
    {
        fm_assert(frames <= s.fps + 2);
        if (moved(frames) != moved(frames - 1))
            s.move_threat(moved(frames) ? beside : threat_pos);
        s.step();
        fm_assert(s.npc->position() == spot);
        fm_assert(s.npc->r == (moved(frames) ? r2 : r1));
    }
    fm_assert(frames + 2 >= s.fps);
    fm_assert(s.status().st == state::search);
}

// Walls a shot passes, 16 px thick around 40x52 px, closing in the 6 spots in the wall's shadow that the
// retreat from tile (7, 1) tries first.
void close_in_retreat(world& w, point hidden)
{
    const auto mid = hidden + Vector2i{18, 216};
    const Vector2i walls[][2] = {
        {{72, 16}, {0, -34}}, {{72, 16}, {0, 34}}, {{16, 84}, {-28, 0}}, {{16, 84}, {28, 0}},
    };
    for (const auto& [size, off] : walls)
    {
        const auto p = mid + off;
        add_blocker(w, p.local(), Vector2ub(size), p.offset(), pass_mode::shoot_through);
    }
}

// As test_cover_script_retreat, with close_in_retreat's walls. None of the 6 spots has a path, so at 4 Dijkstra
// searches per update the retreat takes the flank's update and at least one more.
void test_cover_script_retreat_closed_in(point hidden)
{
    cover_scene s;
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == hidden);
    const auto t0 = s.status();

    const point threat_pos{ch, {7, 1}, {}};
    close_in_retreat(s.w, hidden);
    s.move_threat(threat_pos);
    fm_assert(s.seen_from(threat_pos, hidden));
    s.step();
    fm_assert(s.status().st == state::find_retreat);
    fm_assert_equal(t0.num_dijkstra_calls + 4, s.status().num_dijkstra_calls);
    const auto retreat_updates = s.finish_search();
    const auto t1 = s.status();
    fm_assert(t1.st == state::retreat || t1.st == state::seek);
    fm_assert(retreat_updates >= 1);
    s.step_until(state::seek);
    fm_assert_equal(t0.num_searches + 1, s.status().num_searches);
}

// test_cover_script_retreat_closed_in's scene, up to the flank's update, in which 4 of the 6 enclosed spots
// had no path. Returns the status after it.
cover_script_status flank_closed_in(cover_scene& s, point hidden, point threat_pos)
{
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == hidden);
    close_in_retreat(s.w, hidden);
    s.move_threat(threat_pos);
    s.step();
    const auto t = s.status();
    fm_assert(t.st == state::find_retreat);
    fm_assert_equal(4u, t.num_failed_retreat_dijkstra_calls);
    return t;
}

// As test_cover_script_retreat_closed_in. A 1 px move of the threat keeps the retreat's progress. A 100 px
// move to another octant starts it over, and it picks a spot nearer than the enclosure that the wall hides.
void test_cover_script_retreat_restart(point hidden)
{
    const point threat_pos{ch, {7, 1}, {}};
    {
        cover_scene s;
        const auto t0 = flank_closed_in(s, hidden, threat_pos);
        s.move_threat(threat_pos + Vector2i{1, 0});
        s.step();
        const auto t1 = s.status();
        // The 2 enclosed spots left, then a spot with a path.
        fm_assert(t1.st == state::retreat && t1.num_retreat_restarts == t0.num_retreat_restarts);
        fm_assert_equal(t0.num_dijkstra_calls + 3, t1.num_dijkstra_calls);
        fm_assert_equal(6u, t1.num_failed_retreat_dijkstra_calls);
    }

    cover_scene s;
    const auto t1 = flank_closed_in(s, hidden, threat_pos);
    const auto moved = threat_pos + Vector2i{100, 0};
    s.move_threat(moved);
    s.step();
    const auto t2 = s.status();
    fm_assert_equal(t1.num_retreat_restarts + 1, t2.num_retreat_restarts);
    // One Dijkstra search, since the 4 spots without a path stay done.
    fm_assert(t2.st == state::retreat);
    fm_assert_equal(t1.num_dijkstra_calls + 1, t2.num_dijkstra_calls);
    s.step_until(state::seek);
    const auto spot = s.npc->position();
    // (12, 204) is the first spot test_cover_script_retreat's npc finds, and the first of the enclosure.
    fm_assert((spot - hidden).dot() < Vector2i{12, 204}.dot());
    fm_assert(!s.seen_from(moved, spot));
    const auto dist = [&](point p) { return std::sqrt((double)point::distance_squared(p, moved)); };
    fm_assert(dist(spot) - dist(hidden) > 3*tile_size_xy);
}

// As test_cover_script_retreat without the wall, the threat circling 64 px around the npc an octant per update.
// Each move restarts the retreat, and the npc walks off when the first pass's rays run out.
void test_cover_script_retreat_circling_threat(point hidden)
{
    cover_scene s;
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == hidden);
    s.w[ch].remove_object(*s.wall, s.wall->index());
    rebuild(s.w);
    s.move_threat(point{ch, {7, 1}, {}});
    s.step_until(state::find_retreat);
    const auto t0 = s.status();
    const auto here = s.npc->position();
    auto updates = 0u;
    for (; s.status().st == state::find_retreat; updates++)
    {
        fm_assert(updates < 5);
        const auto a = (updates + 1) * 2 * Math::Constants<double>::pi() / Cover::octant_count;
        s.move_threat(here + Vector2i(Math::round(Vector2d{std::cos(a), std::sin(a)} * 64.)));
        s.step();
    }
    const auto t = s.status();
    fm_assert(t.st == state::retreat && t.retreat_all);
    fm_assert_equal(t0.num_retreat_restarts + updates, t.num_retreat_restarts);
    fm_assert_equal(t0.num_dijkstra_calls + 1, t.num_dijkstra_calls);
}

// As test_cover_script_retreat_circling_threat, from the npc's search for cover at hidden. The search restarts
// only while a whole scan fits in search_tiles_left, so it still reaches its last tile, or a block 7 tiles away.
void test_cover_script_search_circling_threat(point hidden, bool far_block)
{
    cover_scene s;
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == hidden);
    s.w[ch].remove_object(*s.wall, s.wall->index());
    const point block{ch, {12, 12}, {}};
    if (far_block)
        add_blocker(s.w, block.local(), {64, 64});
    rebuild(s.w);
    const auto t0 = s.status();
    // Without the block the search ends during the retreat walk, or after it with the npc standing.
    const auto ended = [&] {
        return far_block ? s.status().st == state::to_cover : s.status().num_searches != t0.num_searches;
    };
    auto walked = false;
    for (auto updates = 0u; !ended(); updates++)
    {
        fm_assert(updates < 60*s.fps);
        const auto a = (updates + 1) * 2 * Math::Constants<double>::pi() / Cover::octant_count;
        s.move_threat(hidden + Vector2i(Math::round(Vector2d{std::cos(a), std::sin(a)} * 64.)));
        s.step();
        const auto st = s.status().st;
        walked |= st == state::retreat;
        fm_assert(ended() || st == state::search || st == state::find_retreat || st == state::retreat);
    }
    fm_assert(walked);
    fm_assert_equal(t0.num_searches + 1, s.status().num_searches);
    if (far_block)
        fm_assert(point::distance(s.status().cover, block) < 2*(u32)tile_size_xy);
    else
        fm_assert_equal(s.status().num_search_tiles, s.status().search_tile);
}

// As test_cover_script_retreat, with a block 7 tiles south of hidden that hides a spot from tile (7, 1). The npc
// walks off, then takes that spot, also when a block at (3, 2) hides a nearer one toward the threat.
void test_cover_script_retreat_finds_cover(point hidden, bool near_threat)
{
    cover_scene s;
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == hidden);
    const point block{ch, {7, 14}, {}};
    add_blocker(s.w, block.local(), {64, 64});
    if (near_threat)
        add_blocker(s.w, {3, 2}, {64, 64});
    rebuild(s.w);
    const auto t0 = s.status();

    const point threat_pos{ch, {7, 1}, {}};
    s.move_threat(threat_pos);
    fm_assert(s.seen_from(threat_pos, hidden));
    s.stay(20);
    auto walked = false;
    for (auto frames = 0u; s.status().st != state::hidden; frames++)
    {
        fm_assert(frames < 60*s.fps);
        const auto st = s.status().st;
        fm_assert(st == state::retreat || st == state::to_cover);
        walked |= st == state::retreat;
        s.step();
    }
    const auto t = s.status();
    fm_assert(walked);
    fm_assert(s.npc->position() == t.cover);
    fm_assert(point::distance(t.cover, hidden) > 5*(u32)tile_size_xy);
    fm_assert(point::distance(t.cover, block) < 2*(u32)tile_size_xy);
    fm_assert(cant_be_shot_at(s.w, threat_pos, s.threat->id, t.cover, body, s.npc->id));
    fm_assert_equal(t0.num_searches + 1, t.num_searches);
}

// As test_cover_script_retreat, with a block put in the way of the npc's retreat walk. The npc looks for a
// retreat once more from where it was stopped. A second block leaves it searching where it stands.
void test_cover_script_retreat_blocked(point hidden)
{
    cover_scene s;
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == hidden);
    s.move_threat(point{ch, {7, 1}, {}});
    s.step_until(state::retreat);
    // A 64 px block 8 px past the body's edge, along the npc's next step. Shots pass it, so it hides nothing.
    const auto block_ahead = [&] {
        const auto from = s.npc->position();
        s.step();
        const auto at = s.npc->position();
        fm_assert(s.status().st == state::retreat && at != from);
        const auto p = at + Math::sign(at - from)*(32 + 8 + (int)body.x()/2);
        add_blocker(s.w, p.local(), {64, 64}, p.offset(), pass_mode::shoot_through);
        rebuild(s.w);
    };
    block_ahead();
    const auto t0 = s.status();
    s.step_until(state::find_retreat);
    const auto t1 = s.status();
    fm_assert_equal(t0.num_retreat_restarts + 1, t1.num_retreat_restarts);
    s.step_until(state::retreat);
    fm_assert(s.status().num_dijkstra_calls > t1.num_dijkstra_calls);

    // Each retreat walk is longer than min_retreat_gain, so stopping this soon means a block.
    block_ahead();
    for (auto frames = 0u; s.status().st == state::retreat; frames++)
    {
        fm_assert(frames < s.fps/2);
        s.step();
    }
    fm_assert(s.status().st == state::search);
    for (auto frames = 0u; s.status().st != state::seek; frames++)
    {
        fm_assert(frames < 60*s.fps);
        fm_assert(s.status().st == state::search);
        s.step();
    }
    fm_assert_equal(t1.num_retreat_restarts, s.status().num_retreat_restarts);
}

// As test_cover_script_retreat. The threat moves mid-walk to where it sees the walk's end, still over 3 tiles
// farther than hidden. The npc looks for a retreat again, and a threat next to the second end starts no third.
void test_cover_script_retreat_end_seen(point hidden)
{
    const point threat_pos{ch, {7, 1}, {}}, moved{ch, {5, 1}, {-20, 0}};
    const auto dist = [&](point p, point from) { return std::sqrt((double)point::distance_squared(p, from)); };
    // test_cover_script_retreat's spot
    const auto end = hidden + Vector2i{12, 204};
    // Returns where the npc stood when it looked for a retreat again.
    const auto restart = [&](cover_scene& s) {
        s.step_until(state::hidden);
        fm_assert(s.npc->position() == hidden);
        s.move_threat(threat_pos);
        s.step_until(state::retreat);
        fm_assert(s.seen_from(moved, end));
        fm_assert(dist(end, moved) - dist(hidden, moved) > 3*tile_size_xy);
        const auto t0 = s.status();
        const auto from = s.npc->position();
        s.move_threat(moved);
        s.step();
        const auto t = s.status();
        fm_assert_equal(t0.num_retreat_restarts + 1, t.num_retreat_restarts);
        fm_assert(t.num_dijkstra_calls - t0.num_dijkstra_calls <= 4);
        return from;
    };
    point spot;
    {
        cover_scene s;
        const auto from = restart(s);
        s.step_until(state::retreat);
        for (auto frames = 0u; s.status().st == state::retreat; frames++)
        {
            fm_assert(frames < 60*s.fps);
            s.step();
        }
        fm_assert(s.status().st == state::seek || s.status().st == state::search);
        spot = s.npc->position();
        fm_assert(!s.seen_from(moved, spot));
        fm_assert(dist(spot, moved) - dist(from, moved) > 3*tile_size_xy);
    }

    cover_scene s;
    restart(s);
    s.step_until(state::retreat);
    const auto t1 = s.status();
    for (auto frames = 0u; s.status().st == state::retreat; frames++)
    {
        fm_assert(frames < 60*s.fps);
        s.move_threat(spot + Vector2i{64 + (int)(frames % 2), 0});
        s.step();
        const auto t = s.status();
        fm_assert(t.st != state::find_retreat);
        fm_assert_equal(t1.num_retreat_restarts, t.num_retreat_restarts);
    }
}

// A wall at tile (12, 8) whose west face, over 7 tiles from the npc, is the only cover from the
// threat at (15, 8). The npc stands while its search reaches there, filling at most one octant
// and running at most one Dijkstra search per update, then walks there and hides. With turned
// set, the threat moves 1 px north in each of the first 2 updates, which keeps its octant and the
// search's progress, then moves to the north-east, another octant, which starts the search over.
void test_cover_script_far_cover(bool turned)
{
    auto w = world();
    add_ground(w);
    add_blocker(w, {12, 8}, {16, 96});
    rebuild(w);
    const point threat_pos{ch, {15, 8}, {}}, npc_pos{ch, {4, 8}, {}};
    const auto threat = add_critter(w, threat_pos);
    auto npc = add_critter(w, npc_pos);
    w.init_scripts();
    auto A = astar{};
    npc->script.do_reassign(critter_script::make_cover_script(A, threat->id, 8), npc);
    const auto& pool = A.pool_registry().cover_pool(Cover::Params{8});
    const auto frame = Second / npc->atlas->info().fps;
    const auto status = [&] { return *get_cover_script_status(*npc); };
    const auto step = [&] {
        auto i = npc->index();
        npc->update(npc, i, frame);
    };
    const auto move_threat = [&](point pos) {
        auto i = threat->index();
        threat->teleport_to(i, pos, rotation_COUNT);
    };

    auto updates = 0u, last_tile = 0u;
    auto restarted = false;
    for (step(); status().st == state::search; updates++)
    {
        fm_assert(updates < 2000);
        fm_assert(npc->position() == npc_pos);
        const auto fills = pool.octant_fills(), num_dijkstra_calls = status().num_dijkstra_calls;
        if (turned && updates <= 2)
            move_threat(updates == 2 ? point{ch, {12, 1}, {}} : threat->position() - Vector2i{0, 1});
        step();
        const auto t = status();
        fm_assert(pool.octant_fills() - fills <= 1);
        fm_assert(t.num_dijkstra_calls - num_dijkstra_calls <= 1);
        if (t.st != state::search)
            break;
        if (turned && updates == 2)
        {
            fm_assert(t.search_tile < last_tile && t.search_tile <= 32);
            restarted = true;
        }
        else
            fm_assert(t.search_tile >= last_tile);
        last_tile = t.search_tile;
    }
    fm_assert(updates > 2);
    fm_assert(!turned || restarted);
    if (turned)
    {
        w.finish_scripts();
        return;
    }
    const auto t = status();
    fm_assert(t.st == state::to_cover && t.has_cover);
    fm_assert(point::distance(npc_pos, t.cover) > 7*(u32)tile_size_xy);
    for (auto frames = 0u; status().st != state::hidden; frames++)
    {
        fm_assert(frames < 2000);
        step();
    }
    fm_assert(npc->position() == t.cover);
    fm_assert(cant_be_shot_at(w, threat_pos, threat->id, t.cover, body, npc->id));
    w.finish_scripts();
}

// In test_cover_script_far_cover's world, options with div_size 16, set while the search runs past
// its first 32 tiles, start it over in the grids of that div_size, where it finds the far cover.
void test_cover_script_div_change()
{
    auto w = world();
    add_ground(w);
    add_blocker(w, {12, 8}, {16, 96});
    rebuild(w);
    const point threat_pos{ch, {15, 8}, {}}, npc_pos{ch, {4, 8}, {}};
    const auto threat = add_critter(w, threat_pos);
    auto npc = add_critter(w, npc_pos);
    w.init_scripts();
    auto A = astar{};
    npc->script.do_reassign(critter_script::make_cover_script(A, threat->id, 8), npc);
    const auto frame = Second / npc->atlas->info().fps;
    const auto status = [&] { return *get_cover_script_status(*npc); };
    const auto step = [&] {
        auto i = npc->index();
        npc->update(npc, i, frame);
    };
    const auto fills = [&](u32 div) { return A.pool_registry().cover_pool(Cover::Params{div}).octant_fills(); };

    for (auto updates = 0u; status().search_tile <= 32; updates++)
    {
        fm_assert(updates < 2000);
        step();
        fm_assert(status().st == state::search);
    }
    fm_assert(fills(16) == 0);
    const auto fills8 = fills(8);
    set_cover_script_options(*npc, {.div_size = 16});
    step();
    fm_assert(status().st == state::search && status().search_tile <= 32);
    for (auto updates = 0u; status().st == state::search; updates++)
    {
        fm_assert(updates < 2000);
        step();
    }
    fm_assert(fills(8) == fills8);
    fm_assert(fills(16) > 1);
    const auto t = status();
    fm_assert(t.st == state::to_cover && t.has_cover);
    fm_assert(point::distance(npc_pos, t.cover) > 7*(u32)tile_size_xy);
    w.finish_scripts();
}

// The npc hides behind the wall at one of its ends, then is sent to the last listed spot, at the
// other end, where it hides and peeks from find_cover_peek's spot for it. Sent to peek from the
// first, it walks to that spot's peek spot and hides at the first. A spot inside the wall has no
// path and no peek spot, and with the threat gone no spot has one, so sending the npc there
// changes nothing.
void test_cover_script_orders(point hidden)
{
    cover_scene s;
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == hidden);
    Array<cover_spot> spots;
    list_cover_spots(*s.npc, spots);
    fm_assert(spots.size() >= 2);
    fm_assert(spots[0].pos == s.status().canonical);
    const auto other = spots[spots.size() - 1], first = spots[0];

    const auto num_rays = s.status().num_rays;
    const auto p = find_cover_peek(*s.npc, other);
    fm_assert(p);
    fm_assert_equal(num_rays, s.status().num_rays);

    fm_assert(send_to_cover(*s.npc, other));
    const auto other_end = s.status().cover;
    {
        const auto t = s.status();
        fm_assert(t.st == state::to_cover);
        fm_assert(t.has_cover && t.canonical == other.pos && !t.has_peek);
    }
    s.step_until(other_end);
    fm_assert(s.status().st == state::hidden);
    s.step_until(state::exposed);
    {
        const auto t = s.status();
        fm_assert(s.npc->position() == p->pos);
        fm_assert(t.canonical == other.pos && t.peek == p->pos && t.gun == p->gun);
    }
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == other_end);

    const auto q = find_cover_peek(*s.npc, first);
    fm_assert(q);
    fm_assert(send_to_peek(*s.npc, first));
    {
        const auto t = s.status();
        fm_assert(t.st == state::to_peek);
        fm_assert(t.canonical == first.pos && t.has_peek && t.peek == q->pos && t.gun == q->gun);
        fm_assert(t.gun_ray_from == s.threat->position());
    }
    s.step_until(state::exposed);
    fm_assert(s.npc->position() == q->pos);
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == hidden);

    const cover_spot wall{point{ch, {7, 8}, {}}, {1, 0}};
    const auto t0 = s.status();
    fm_assert(!send_to_cover(*s.npc, wall));
    fm_assert(!send_to_peek(*s.npc, wall));
    const auto t1 = s.status();
    fm_assert(t1.st == t0.st && t1.cover == t0.cover);
    fm_assert(t1.has_peek == t0.has_peek && t1.peek == t0.peek);

    s.w[ch].kill_object(*s.threat, s.threat->index());
    fm_assert(!find_cover_peek(*s.npc, other));
    fm_assert(!send_to_peek(*s.npc, other));
}

// True when the threat at from sees a pixel of the npc's body centered on pos, as the npc's script tests it.
bool edge_seen(cover_scene& s, point from, point pos)
{
    static cover_point finder;
    return !finder.hidden_at(s.w, from, {pos, axis_toward(from - pos)}, body);
}

// True when the threat at from sees a pixel of the npc where it stands or at its canonical, either of which makes
// it leave.
bool flanked_from(cover_scene& s, point from)
{
    return edge_seen(s, from, s.npc->position()) || edge_seen(s, from, s.status().canonical);
}

// Replaces out with the points of path's segments, a px apart.
void path_points(ArrayView<const point> path, Array<point>& out)
{
    auto count = 1u;
    for (auto k = 1u; k < path.size(); k++)
        count += (u32)Math::ceil(Vector2d(path[k] - path[k-1]).length());
    arrayClear(out);
    arrayReserve(out, count);
    arrayAppend(out, path.front());
    for (auto k = 1u; k < path.size(); k++)
    {
        const auto d = Vector2d(path[k] - path[k-1]);
        const auto n = (u32)Math::ceil(d.length());
        for (auto j = 1u; j <= n; j++)
            arrayAppend(out, path[k-1] + Vector2i(Math::round(d*j/n)));
    }
}

double path_length(ArrayView<const point> path)
{
    auto len = 0.;
    for (auto k = 1u; k < path.size(); k++)
        len += Vector2d(path[k] - path[k-1]).length();
    return len;
}

// The threat's path to where flanked_from(), true from no point farther than a tile from its end. It ends half a
// tile past where flanked_from() first holds, on its way to a spot 2 to 5 tiles from the npc or a tile center.
path_search_result flank_route(cover_scene& s)
{
    const auto npc = s.npc->position(), from = s.threat->position();
    const auto size = Vector2ui(s.threat->bbox_size);
    const auto lo = 2*(int)tile_size_xy, hi = (int)((TILE_MAX_DIM - 3)*tile_size_xy);
    Array<point> pts;
    const auto route_to = [&](point p) -> Optional<path_search_result> {
        const auto px = Vector3i(p).xy();
        if (p.chunk3() != ch || (px < Vector2i{lo}).any() || (px > Vector2i{hi}).any())
            return {};
        if (!passable(s.w, p, body) || !flanked_from(s, p))
            return {};
        const auto toward = s.A.Dijkstra<0, false>(s.w, from, p, chunk_size_xy, size, Search::without_critters());
        if (!toward.is_found())
            return {};
        path_points(toward.path(), pts);
        auto first = 0u;
        while (first < pts.size() && !flanked_from(s, pts[first]))
            first++;
        if (first == 0 || first == pts.size())
            return {};
        const auto end = pts[Math::min(first + half_tile<u32>, (u32)pts.size() - 1)];
        auto route = s.A.Dijkstra<0, false>(s.w, from, end, chunk_size_xy, size, Search::without_critters());
        if (!route.is_found() || !flanked_from(s, end))
            return {};
        path_points(route.path(), pts);
        auto ok = true;
        for (const auto& x : pts)
            ok &= point::distance(x, end) <= tile_size_xy || !flanked_from(s, x);
        if (!ok)
            return {};
        return move(route);
    };
    const auto v = Vector2d(from - npc);
    const auto a0 = std::atan2(v.y(), v.x());
    for (const auto r : { 5., 4., 3., 2. })
        for (const auto k : { 2, 6, 3, 5, 4, 1, 7 })
        {
            const auto a = a0 + k * Math::Constants<double>::pi() / 4;
            const auto p = npc + Vector2i(Math::round(Vector2d{std::cos(a), std::sin(a)} * r * tile_size_xy));
            if (auto route = route_to(p))
                return move(*route);
        }
    for (auto y = 2u; y <= TILE_MAX_DIM - 3; y++)
        for (auto x = 2u; x <= TILE_MAX_DIM - 3; x++)
            if (auto route = route_to(point{ch, {x, y}, {}}))
                return move(*route);
    fm_abort("no flank route");
}

// cover_scene with 64 px blocks around its wall. 16 times, the threat walks a flank_route(), and from the update
// it first sees a pixel of the hidden npc, the npc must hide again within 1.25 times its shortest walk there.
void test_cover_script_rehides()
{
    using walk_result = critter::walk_result;
    cover_scene s{point{ch, {10, 8}, {}}};
    const local_coords blocks[] = { {3, 3}, {8, 3}, {12, 3}, {3, 8}, {12, 8}, {3, 12}, {8, 12}, {12, 12} };
    for (const auto tile : blocks)
        add_blocker(s.w, tile, {64, 64});
    rebuild(s.w);
    auto o = cover_script_options{};
    o.hold = cover_script_options::hold_phase::hidden;
    set_cover_script_options(*s.npc, o);
    s.step_until(state::hidden);
    for (auto i = 0u; i < 16; i++)
    {
        auto route = flank_route(s);
        const auto end = route.path().back();
        const auto route_len = path_length(route.path());
        critter_walk walk{move(route)};
        auto arrived = false, flanked = false;
        auto walked = 0u, updates = 0u, hidden_after = 0u, retreating = 0u, seen = 0u;
        point flanked_at;
        for (auto frames = 0u; ; frames++)
        {
            fm_assert(frames < 60*s.fps);
            if (!arrived)
            {
                auto ti = s.threat->index();
                const auto r = walk.update(s.threat, ti, s.frame);
                fm_assert(r == walk_result::none || r == walk_result::arrived);
                arrived = r == walk_result::arrived;
                walked++;
            }
            if (!flanked && flanked_from(s, s.threat->position()))
            {
                flanked = true;
                flanked_at = s.npc->position();
                fm_assert(point::distance(s.threat->position(), end) <= tile_size_xy);
            }
            const auto was_hidden = s.status().st == state::hidden;
            s.step();
            const auto t = s.status();
            if (!flanked)
            {
                fm_assert(t.st == state::hidden);
                continue;
            }
            updates++;
            retreating += t.st == state::find_retreat || t.st == state::retreat;
            seen += edge_seen(s, s.threat->position(), s.npc->position());
            const auto hides = t.st == state::hidden && s.npc->position() == t.cover;
            if (hides && !was_hidden)
                hidden_after = updates;
            if (arrived && hides)
                break;
        }
        const auto cover = s.status().cover;
        const auto shortest = s.A.Dijkstra<0, false>(s.w, flanked_at, cover, chunk_size_xy, body,
                                                     Search::without_critters());
        fm_assert(shortest.is_found());
        // The threat and the npc are made alike, so they walk as fast.
        const auto px_per_update = route_len / walked;
        const auto limit = 1.25 * path_length(shortest.path()) / px_per_update;
        fm_assert(hidden_after <= limit + 5);
        fm_assert(retreating < s.fps/4);
        fm_assert(seen < s.fps/6);
        fm_assert(cant_be_shot_at(s.w, end, s.threat->id, cover, body, s.npc->id));
    }
}

using fallback = cover_script_options::fallback;

void move_npc(cover_scene& s, point pos)
{
    auto i = s.npc->index();
    s.npc->teleport_to(i, pos, rotation_COUNT);
}

// True when a point within r px of the threat, on an 8 px grid, can shoot the npc at pos.
bool seen_within(cover_scene& s, point pos, int r)
{
    const auto n = r/8;
    for (auto j = -n; j <= n; j++)
        for (auto i = -n; i <= n; i++)
            if (i*i + j*j <= n*n &&
                !cant_be_shot_at(s.w, s.threat->position() + Vector2i{i, j}*8, s.threat->id, pos, body, s.npc->id))
                return true;
    return false;
}

// Replaces cover_scene's wall with one 448 px long, centered 2 tiles west of it.
void add_long_wall(cover_scene& s)
{
    s.w[ch].remove_object(*s.wall, s.wall->index());
    add_blocker(s.w, {5, 8}, {16, 240}, {0, -104});
    add_blocker(s.w, {5, 8}, {16, 240}, {0, 104});
    rebuild(s.w);
}

// With needs_peek, the npc skips the spots near add_long_wall's middle, which have no peek spot, and hides nearer
// an end. When the option is turned off during the search, the npc hides at the middle.
void test_cover_script_needs_peek(bool turned_off)
{
    cover_scene s;
    add_long_wall(s);
    auto o = cover_script_options{};
    // cover_end would move a spot at the middle to the wall's end, which has a peek spot.
    o.max_cover_end_dist = 0;
    set_cover_script_options(*s.npc, o);
    if (turned_off)
    {
        s.step();
        fm_assert(s.status().st == state::search);
        o.needs_peek = false;
        set_cover_script_options(*s.npc, o);
    }
    s.step_until(state::hidden);
    const auto t = s.status();
    fm_assert(s.npc->position() == t.cover);
    fm_assert(turned_off != !!find_cover_peek(*s.npc, {t.canonical, {1, 0}}));
}

// The threat stands 56 px east of cover_scene's wall and the npc as far west of it, with a 64 px block 4 tiles
// west of the npc. With arms, walls run 2 tiles east from the wall's ends. Returns where the npc hides.
point threat_reach_hides(bool reach, bool arms)
{
    cover_scene s{point{ch, {8, 8}, {}}};
    move_npc(s, point{ch, {6, 8}, {}});
    add_blocker(s.w, {2, 8}, {64, 64});
    if (arms)
    {
        add_blocker(s.w, {8, 7}, {136, 16}, {-4, 8});
        add_blocker(s.w, {8, 9}, {136, 16}, {-4, -8});
    }
    rebuild(s.w);
    auto o = cover_script_options{};
    // The wall hides the gun of an npc behind the block from the threat.
    o.needs_peek = false;
    if (!reach)
        o.threat_reach = 0;
    set_cover_script_options(*s.npc, o);
    s.step_until(state::hidden);
    const auto pos = s.npc->position();
    fm_assert(cant_be_shot_at(s.w, s.threat->position(), s.threat->id, pos, body, s.npc->id));
    // A point north-west of the wall's end sees the spot behind the wall. Without arms, the threat can walk there.
    fm_assert(!arms || seen_within(s, pos, 2*tile_size_xy));
    return pos;
}

// With threat_reach, the npc hides behind threat_reach_hides's block. With 0, it hides behind the wall.
void test_cover_script_threat_reach_skips()
{
    const point wall{ch, {7, 8}, {}}, block{ch, {2, 8}, {}};
    fm_assert(point::distance(threat_reach_hides(true, false), block) < tile_size_xy);
    fm_assert(point::distance(threat_reach_hides(false, false), wall) < 2*(u32)tile_size_xy);
}

// With threat_reach_hides's arms, the npc hides behind the wall though a point within threat_reach px of the
// threat sees it there, as the threat would walk farther to get to that point.
void test_cover_script_threat_reach_wall()
{
    const point wall{ch, {7, 8}, {}};
    fm_assert(point::distance(threat_reach_hides(true, true), wall) < tile_size_xy);
}

// With threat_reach, no point within that many px of cover_scene's threat sees the npc at its cover. With 0,
// cover_end moves the npc along the wall to where such points see it.
void test_cover_script_cover_end_reach(bool reach)
{
    cover_scene s;
    if (!reach)
        set_cover_script_options(*s.npc, {.threat_reach = 0});
    s.step_until(state::hidden);
    const auto pos = s.npc->position();
    fm_assert(cant_be_shot_at(s.w, s.threat->position(), s.threat->id, pos, body, s.npc->id));
    fm_assert(reach != seen_within(s, pos, 2*tile_size_xy));
}

// Replaces cover_scene's wall with one 584 px long, 56 px west of a threat at tile (6, 8). The wall hides the gun
// of an npc behind it within max_peek_dist of any spot there, so no spot has a peek spot.
void add_longer_wall(cover_scene& s)
{
    s.w[ch].remove_object(*s.wall, s.wall->index());
    for (const auto y : { (u8)5, (u8)8, (u8)11 })
        add_blocker(s.w, {5, y}, {16, 200});
    rebuild(s.w);
}

// Steps until the npc's search ends, and returns whether the npc moved before. With may_retreat, the npc must
// retreat while the search runs, and without it never.
bool moves_during_search(cover_scene& s, bool may_retreat)
{
    const auto t0 = s.status();
    const auto start = s.npc->position();
    auto moved = false, retreated = false;
    for (auto frames = 0u; s.status().num_searches == t0.num_searches; frames++)
    {
        fm_assert(frames < 60*s.fps);
        s.step();
        const auto t = s.status();
        const auto retreating = t.st == state::find_retreat || t.st == state::retreat;
        fm_assert(may_retreat || !retreating);
        retreated |= retreating && t.search_tile < t.num_search_tiles;
        moved |= s.npc->position() != start;
    }
    fm_assert(retreated == may_retreat);
    return moved;
}

// No spot behind add_longer_wall's wall has a peek spot. With no_peek, the npc hides behind it when the search
// ends, and with none it seeks again. The threat doesn't see the npc where it starts, so it doesn't retreat.
void test_cover_script_last_resort_no_peek(fallback last_resort)
{
    cover_scene s{point{ch, {6, 8}, {}}};
    add_longer_wall(s);
    set_cover_script_options(*s.npc, {.last_resort = last_resort});
    fm_assert(!moves_during_search(s, false));
    fm_assert(s.status().st == (last_resort == fallback::none ? state::seek : state::to_cover));
    if (last_resort == fallback::none)
        return;
    s.step_until(state::hidden);
    const auto pos = s.npc->position();
    fm_assert(Vector3i(pos).x() + (int)body.x()/2 == 312);
    fm_assert(cant_be_shot_at(s.w, s.threat->position(), s.threat->id, pos, body, s.npc->id));
}

// As test_cover_script_last_resort_no_peek, the npc starting where the threat sees it. It retreats once the search
// keeps a spot with no peek spot, and moves before the search ends. It hides at that spot afterwards.
void test_cover_script_last_resort_seen()
{
    cover_scene s{point{ch, {6, 8}, {}}};
    add_longer_wall(s);
    move_npc(s, point{ch, {8, 8}, {}});
    fm_assert(s.seen_from(s.threat->position(), s.npc->position()));
    fm_assert(moves_during_search(s, true));
    fm_assert(s.status().st == state::to_cover);
    s.step_until(state::hidden);
    const auto pos = s.npc->position();
    fm_assert(Vector3i(pos).x() + (int)body.x()/2 == 312);
    fm_assert(cant_be_shot_at(s.w, s.threat->position(), s.threat->id, pos, body, s.npc->id));
}

// The threat stands 56 px east of cover_scene's wall and can walk around either end within threat_reach. With
// in_reach, the npc hides behind the wall when the search ends, and with no_peek it seeks again.
void test_cover_script_last_resort_in_reach(fallback last_resort)
{
    cover_scene s{point{ch, {8, 8}, {}}};
    set_cover_script_options(*s.npc, {.last_resort = last_resort});
    fm_assert(!moves_during_search(s, false));
    fm_assert(s.status().st == (last_resort == fallback::in_reach ? state::to_cover : state::seek));
    if (last_resort != fallback::in_reach)
        return;
    s.step_until(state::hidden);
    const auto pos = s.npc->position();
    fm_assert(point::distance(pos, point{ch, {7, 8}, {}}) < tile_size_xy);
    fm_assert(cant_be_shot_at(s.w, s.threat->position(), s.threat->id, pos, body, s.npc->id));
}

// As test_cover_script_last_resort_no_peek with no_peek, the threat walking 1 px per update north and back over
// 4 px. The search still ends, and the npc hides behind the wall.
void test_cover_script_last_resort_walking_threat()
{
    cover_scene s{point{ch, {6, 8}, {}}};
    add_longer_wall(s);
    set_cover_script_options(*s.npc, {.last_resort = fallback::no_peek});
    const auto num_searches = s.status().num_searches;
    for (auto frames = 0u; s.status().num_searches == num_searches; frames++)
    {
        fm_assert(frames < 60*s.fps);
        s.move_threat(s.threat->position() + Vector2i{0, frames % 8 < 4 ? -1 : 1});
        s.step();
    }
    fm_assert(s.status().st == state::to_cover);
    s.step_until(state::hidden);
    const auto pos = s.npc->position();
    fm_assert(Vector3i(pos).x() + (int)body.x()/2 == 312);
    fm_assert(cant_be_shot_at(s.w, s.threat->position(), s.threat->id, pos, body, s.npc->id));
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
    test_octants_beside_bearing();
    test_touching_blockers();
    test_wall_out_of_radius();
    test_spot_taken();
    test_lists_cover_spots();
    test_max_fills();
    test_lists_past_rejected();
    test_lists_by_area(0);
    test_lists_by_area(3);
    test_octants_wall_middle();
    test_octants_turned_threat_sees();
    for (const auto octants : { u8{0}, u8{3} })
    {
        test_moves_to_cover_end(octants, false);
        test_moves_to_cover_end(octants, true);
    }
    test_lists_by_area(3, 2*tile_size_xy);
    test_lists_by_area_long_wall();
    for (const auto size : { Vector2ui{23, 23}, Vector2ui{25, 25}, Vector2ui{23, 25}, Vector2ui{25, 23} })
    {
        test_narrow_pillar_rejected(size);
        test_gap_between_pillars_rejected(size);
        test_side_wall_corner_seen(size);
        test_shift_to_only_spot(3, false, size);
        test_shift_to_only_spot(-1, false, size);
    }
    const auto [hidden, peek] = test_cover_script();
    test_cover_script_at_wall_end(hidden);
    // cover_scene's gun is at the body's center, and test_cover_script's is not, so they peek from
    // different spots.
    const auto c0 = peek_with_gun(0);
    fm_assert(c0.hidden == hidden);
    test_cover_script_hidden_checks(hidden);
    test_cover_script_flanked(hidden, c0.peek);
    test_cover_script_holds_hidden(hidden, c0.peek);
    test_cover_script_holds_exposed(hidden, c0.peek);
    test_cover_script_hold_follows_orders(hidden, c0.peek);
    test_cover_script_flanked_retreat(c0.peek);
    test_cover_script_flanked_searches_from_retreat_end(c0.peek);
    test_cover_script_seek_no_path();
    test_cover_script_no_path_ties();
    test_cover_script_no_path_threat_moves(point{ch, {2, 8}, {0, -1}}, true);
    test_cover_script_no_path_threat_moves(point{ch, {2, 8}, {0, -14}}, false);
    test_cover_script_no_path_opened();
    test_cover_script_no_path_far_change();
    test_cover_script_walk_checks(hidden);
    test_cover_script_peek_walk_checks(hidden, c0.peek);
    test_cover_script_peek_again_flanked(hidden, c0.peek);
    test_cover_script_search_every_move(hidden);
    test_cover_script_retreat(hidden);
    test_cover_script_retreat_closed_in(hidden);
    test_cover_script_retreat_restart(hidden);
    test_cover_script_retreat_circling_threat(hidden);
    test_cover_script_search_circling_threat(hidden, false);
    test_cover_script_search_circling_threat(hidden, true);
    test_cover_script_retreat_finds_cover(hidden, false);
    test_cover_script_retreat_finds_cover(hidden, true);
    test_cover_script_retreat_blocked(hidden);
    test_cover_script_retreat_end_seen(hidden);
    test_cover_script_far_cover(false);
    test_cover_script_far_cover(true);
    test_cover_script_div_change();
    test_cover_script_orders(hidden);
    test_cover_script_keeps_peeking(hidden, c0.peek);
    test_cover_script_walking_threat(hidden);
    test_cover_script_follows_while_hidden();
    test_cover_script_threat_walks_along_wall(false);
    test_cover_script_threat_walks_along_wall(true);
    test_cover_script_gun_offset();
    test_cover_script_peek_beside_critter();
    test_cover_script_peeks_diagonally();
    test_cover_script_peeks_from_corner();
    test_cover_script_faces_threat();
    test_cover_script(hidden);
    // 1 px past the hidden body on the side it peeked from, so only the other side is left
    const auto side = Vector2i{0, Vector3i(peek).y() > Vector3i(hidden).y() ? 1 : -1};
    const auto c = test_cover_script(hidden + side*((int)body.y() + 1));
    fm_assert(c.hidden == hidden);
    fm_assert((Vector3i(c.peek).y() - Vector3i(c.hidden).y())*side.y() < 0);
    test_cover_script_rehides();
    test_cover_script_needs_peek(false);
    test_cover_script_needs_peek(true);
    test_cover_script_threat_reach_skips();
    test_cover_script_threat_reach_wall();
    test_cover_script_cover_end_reach(true);
    test_cover_script_cover_end_reach(false);
    test_cover_script_last_resort_no_peek(fallback::none);
    test_cover_script_last_resort_no_peek(fallback::no_peek);
    test_cover_script_last_resort_seen();
    test_cover_script_last_resort_in_reach(fallback::no_peek);
    test_cover_script_last_resort_in_reach(fallback::in_reach);
    test_cover_script_last_resort_walking_threat();
}

} // namespace floormat
