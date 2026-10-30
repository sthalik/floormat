#include "app.hpp"
#include "compat/borrowed-ptr.inl"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include "src/cover-point.hpp"
#include "src/critter.hpp"
#include "src/critter-script.hpp"
#include "src/critter-script-cover.hpp"
#include "src/anim-atlas.hpp"
#include "src/nanosecond.inl"
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

bool passable(world& w, point pt, Vector2ui size, const Search::pred& pred = Search::without_critters())
{
    const auto min = Vector2(pt.intra().center_shifted() - Vector2i(size/2u));
    return Search::is_passable(w, pt.chunk3(), Range2D{min, min + Vector2(size)}, pred);
}

cover_point_result find(world& w, point npc, point threat, uint32_t radius, uint32_t div_size, Vector2ui size = body,
                        const Search::pred& pred = Search::without_critters(), bool list_spots = false,
                        uint8_t octants = 0, uint32_t max_slide = 0)
{
    static cover_point finder;
    Grid::PoolRegistry pools;
    return finder.find_cover_point(w, pools, Cover::Params{div_size}, {npc, size, threat, radius, octants, max_slide},
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
    // PoolRegistry's pools leave critters out of the grid, and raycast() skips the cells it marks empty.
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

// Puts a critter at pos and shoots at it.
bool cant_be_shot(world& w, point threat, point pos, Vector2ui size = body)
{
    return cant_be_shot_at(w, threat, 0, pos, size, add_critter(w, pos, size)->id);
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
    add_blocker(w, {6, 8}, {100, 16}, {(int8_t)(18 + d.x()), (int8_t)(32 + d.y())});
    add_blocker(w, {7, 8}, {16, 64}, {(int8_t)(8 + d.x()), (int8_t)(-14 + d.y())});
    rebuild(w);

    const point npc{ch, {7, 8}, {-12, 12}}, threat{ch, {12, 10}, {-20, -24}};
    const auto r = find(w, npc, threat, 48, 8, size);
    fm_assert(r.candidates > 0);
    fm_assert(!r.found || cant_be_shot(w, threat, r.pos, size));
}

// The pillar is as tall as the body, and a wall along its south end stops the body sliding past it.
void test_shift_to_only_spot(int8_t pillar_dy, bool split = false, Vector2ui size = body)
{
    auto w = world();
    add_ground(w);
    if (split)
    {
        add_blocker(w, {7, 8}, {16, 17}, {8, (int8_t)(pillar_dy - 4)});
        add_blocker(w, {7, 8}, {16, 7}, {8, (int8_t)(pillar_dy + 8)});
    }
    else
        add_blocker(w, {7, 8}, {16, (uint8_t)size.y()}, {8, pillar_dy});
    add_blocker(w, {5, 8}, {192, 16}, {32, (int8_t)(pillar_dy + (int)(size.y() - size.y()/2) + 8)});
    rebuild(w);

    const point npc{ch, {6, 8}, {20, -4}}, threat{ch, {14, 8}, {}};
    const auto r = find(w, npc, threat, 48, 8, size);
    fm_assert(r.found);
    fm_assert(cant_be_shot(w, threat, r.pos, size));
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
cover_point_result find_listed(world& w, point npc, point threat, uint32_t radius)
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
    Grid::PoolRegistry pools;
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
void check_lists_by_area(world& w, point npc, point threat, uint8_t octants, uint32_t max_slide, uint32_t min_spots)
{
    constexpr auto radius = 4*(uint32_t)tile_size_xy;
    cover_point finder;
    Grid::PoolRegistry pools;
    const auto search = [&](Optional<Range2Di> area) {
        return finder.find_cover_point(w, pools, Cover::Params{8}, {npc, body, threat, radius, octants, max_slide, area},
                                       Search::without_critters(), true);
    };
    const auto whole = search({});
    Array<cover_spot> spots;
    arrayAppend(spots, whole.spots);
    fm_assert(spots.size() > min_spots);

    Array<point> tiled;
    uint32_t candidates = 0;
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

// Slid spots of one face end at its two ends, so a slide leaves fewer spots.
void test_lists_by_area(uint8_t octants, uint32_t max_slide = 0)
{
    auto w = world();
    add_ground(w);
    add_blocker(w, {7, 8}, {16, 96});
    add_blocker(w, {9, 4}, {96, 16});
    add_blocker(w, {4, 11}, {16, 16});
    rebuild(w);
    check_lists_by_area(w, point{ch, {6, 7}, {4, 4}}, point{ch, {12, 8}, {}}, octants, max_slide, max_slide ? 1 : 3);
}

// A wall of eight blockers 64 px long end to end, x [440, 456) and y [224, 736), between the npc
// and the threat. A search of one tile collects the rects of the blockers near that tile, and a
// slide of 2 tiles along the wall reaches blockers past them, which hide the body there.
void test_lists_by_area_long_wall()
{
    auto w = world();
    add_ground(w);
    for (auto y = 4; y <= 11; y++)
        add_blocker(w, {7, (uint8_t)y}, {16, 64});
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
    fm_assert_equal(kept, (uint32_t)spots3.size());
    fm_assert(kept < spots0.size());
}

// The npc stands west of the wall near its north end, and the threat east of it, a little south.
// Without octants, each spot found lies at the north end, where the wall hides it from the threat.
// The threat turned an octant north sees each of them. With 3 octants the cover grid still passes
// as many candidates, so the test from the turned threat is what rejects them.
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
    const auto candidates0 = r0.candidates;
    const auto r3 = find(w, npc, at_px(T), radius, 8, body, Search::without_critters(), true, 3);
    fm_assert(!r3.found);
    fm_assert_equal(candidates0, r3.candidates);
    fm_assert_equal(r3.candidates, r3.rejected);
    for (const auto p : spots0)
    {
        fm_assert(hidden_from(w, T, p));
        fm_assert(!hidden_from(w, turned_threat(T, Vector3i(p).xy(), -1), p));
    }
}

// At div_size 16, a 24 px body centered on a cell's center lies in 3x3 cells, and the outer
// cells' centers lie 4 px outside it, so the cover grid reads octants at points the body doesn't
// cover. The npc stands west of the wall near its north
// end, and the threat south-east of it. Without octants, the one candidate is pushed to a spot
// that neither the threat nor the threat turned an octant either way sees. With 3 octants the
// cover grid rejects that candidate.
void test_octants_grid_rejects()
{
    auto w = world();
    add_ground(w);
    add_blocker(w, {7, 8}, {16, 96});
    rebuild(w);

    const point npc{ch, {4, 6}, {}};
    const auto T = Vector2i{560, 522};
    constexpr auto radius = 3*tile_size_xy;
    const auto r0 = find(w, npc, at_px(T), radius, 16);
    fm_assert(r0.found);
    const auto pos0 = r0.pos;
    const auto candidates0 = r0.candidates;
    const auto r3 = find(w, npc, at_px(T), radius, 16, body, Search::without_critters(), false, 3);
    fm_assert(!r3.found);
    fm_assert(r3.candidates < candidates0);
    const auto v = Vector3i(pos0).xy();
    fm_assert(hidden_from(w, T, pos0));
    fm_assert(hidden_from(w, turned_threat(T, v, -1), pos0));
    fm_assert(hidden_from(w, turned_threat(T, v, 1), pos0));
}

// True when neither the threat nor the threat turned about pos by up to octants/2 octants either
// way hits a body centered on pos.
bool hidden_from_turned(world& w, Vector2i threat, point pos, uint8_t octants)
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
// npc west of it, 12 px north of its middle. The spot behind the wall slides north, the nearer
// end, to where the body 1 px farther north is hit from the threat or a turned threat. When
// north_leg, a wall going west from the north end stops the slide north before that, and the spot
// slides to the south end. The npc stands nearer the wall's face than the leg's west end, which
// is cover too.
void test_slides_to_cover_end(uint8_t octants, bool north_leg)
{
    auto w = world();
    add_ground(w);
    add_blocker(w, {7, 8}, {16, 160});
    if (north_leg)
        add_blocker(w, {7, 7}, {48, 16}, {-32, -8});
    rebuild(w);

    const point npc{ch, {6, 8}, {0, -12}}, threat{ch, {12, 8}, {}};
    const auto T = Vector3i(threat).xy();
    constexpr auto radius = 3*(uint32_t)tile_size_xy, max_slide = 2*(uint32_t)tile_size_xy;
    const auto r0 = find(w, npc, threat, radius, 8, body, Search::without_critters(), false, octants);
    const auto r = find(w, npc, threat, radius, 8, body, Search::without_critters(), false, octants, max_slide);
    fm_assert(r0.found && r.found && r.axis == r0.axis && r.axis == Vector2i{1, 0});
    const auto p0 = Vector3i(r0.pos).xy(), p = Vector3i(r.pos).xy();
    fm_assert(p.x() == p0.x());
    fm_assert(north_leg ? p.y() > p0.y() : p.y() < p0.y());
    fm_assert(Math::abs(p.y() - p0.y()) <= (int)max_slide);
    fm_assert(passable(w, r.pos, body) && !passable(w, r.pos + r.axis, body));
    const auto farther = r.pos + Vector2i{0, north_leg ? 1 : -1};
    fm_assert(hidden_from_turned(w, T, r.pos, octants));
    fm_assert(!hidden_from_turned(w, T, farther, octants));
}

// test_cover_script's wall and threat. The npc hid at hidden, a spot the threat and the threat
// turned an octant either way don't see, at one of the wall's ends: 1 px farther along the wall on
// one side, one of them sees the body.
void test_cover_script_at_wall_end(point hidden)
{
    auto w = world();
    add_ground(w);
    add_blocker(w, {7, 8}, {16, 96});
    rebuild(w);
    const auto T = Vector3i(point{ch, {12, 8}, {}}).xy();
    fm_assert(hidden_from_turned(w, T, hidden, 3));
    fm_assert(!hidden_from_turned(w, T, hidden + Vector2i{0, 1}, 3) || !hidden_from_turned(w, T, hidden - Vector2i{0, 1}, 3));
}

struct cover_cycle { point hidden, peek; };

// Where cover_script puts the gun of a body centered on pos: offset px to the right of pos as it
// faces threat, square to the exact direction to threat and rounded to whole px. gun_point() in
// critter-script-cover.cpp has the same float expression, and release builds compile both with
// fast math, so their last bits may differ. That changes the rounded result only near a half
// pixel, so each component must lie at least 1e-4 px from one.
point gun_at(point pos, point threat, int offset)
{
    if (pos == threat)
        return pos;
    const auto f = Vector2(threat - pos);
    const auto v = Vector2{-f.y(), f.x()} * ((float)offset / f.length());
    fm_assert((Math::abs(v - Math::floor(v) - Vector2{.5f}) >= Vector2{1e-4f}).all());
    return pos + Vector2i(Math::round(v));
}

// The npc walks behind the wall and stays hidden for 2 s. It then slides along the wall the least
// that lets a ray from the threat reach its gun, half the body's width to its right as it faces the
// threat. It stays out 2 s and walks back to the same spot. Another critter stands at other_pos
// throughout.
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
    struct rest { point pos; uint32_t frames; };
    Array<rest> rests;
    auto last = npc->position();
    auto still = 0u;
    for (auto frames = 0u; rests.size() < 4; frames++)
    {
        fm_assert(frames < 60*fps);
        auto i = npc->index();
        npc->update(npc, i, frame);
        if (npc->position() == last)
        {
            still++;
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
    const auto near_2s = [&](uint32_t n) { return n + 2 >= 2*fps && n <= 2*fps + 2; };
    fm_assert(near_2s(hidden.frames) && near_2s(peek.frames) && near_2s(back.frames));
    fm_assert(cant_be_shot_at(w, threat_pos, threat->id, hidden.pos, body, npc->id));
    fm_assert(back.pos == hidden.pos && peek2.pos == peek.pos);

    const auto slide = Vector3i(peek.pos).xy() - Vector3i(hidden.pos).xy();
    fm_assert(slide.x() == 0 && slide.y() != 0);
    Grid::Pass::Pool pool{Pass::Params{8, pass_mode::shoot_through}, Search::without_critters()};
    // The slide is along y, so the body's width along the wall is body.y().
    const auto seen = [&](point p) {
        const auto gun = gun_at(p, threat_pos, (int)body.y()/2);
        return raycast(w, threat_pos, gun, threat->id, pool, Search::without_critters()).success;
    };
    fm_assert(seen(peek.pos));
    fm_assert(!seen(peek.pos - Vector2i{0, slide.y() > 0 ? 1 : -1}));
    w.finish_scripts();
    return { hidden.pos, peek.pos };
}

// The wall and the npc of test_cover_script, with the threat at threat_pos, and another critter at
// other_pos when it is given. The npc's bbox_size is npc_size while it faces north, as it starts.
// Its gun is gun_offset px to its right, or where the 3-argument make_cover_script puts it when
// gun_offset is empty. With the default 0 the gun is at the body's center, so
// seen_from(threat_pos, npc->position()) is the check the npc makes. Each step() updates the npc
// by one frame of its animation.
struct cover_scene
{
    world w;
    astar A;
    Grid::Pass::Pool ray_pool{Pass::Params{8, pass_mode::shoot_through}, Search::without_critters()};
    bptr<critter> threat, npc;
    uint32_t fps = 0;
    Ns frame;

    explicit cover_scene(point threat_pos = point{ch, {12, 8}, {}}, Optional<int> gun_offset = 0,
                         const Optional<point>& other_pos = {}, Vector2ui npc_size = body);
    ~cover_scene() noexcept;
    void step();
    bool seen_from(point threat_pos, point pos);
    point next_stop();
    uint32_t stay(uint32_t max, Vector2i threat_step = {});
    void move_threat(point pos);
    cover_script_status status();
    void step_until(point pos);
    void step_until(cover_script_status::state st);
};

cover_scene::cover_scene(point threat_pos, Optional<int> gun_offset, const Optional<point>& other_pos,
                         Vector2ui npc_size)
{
    add_ground(w);
    add_blocker(w, {7, 8}, {16, 96});
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
uint32_t cover_scene::stay(uint32_t max, Vector2i threat_step)
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
// threat sees. It stands there for what was left of its 2 s out, which the walk doesn't spend,
// then hides behind the wall's east face.
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
    // frame of the wait, and follow_threat started the walk in it. As in test_cover_script, a wait
    // may end a frame early or late. A walk that arrives on an update without a move, as a
    // walk_mode::path walk does when its last step uses up dt, adds that update to still. The walk
    // takes at least half a second, far more than that slack, so the check fails when the walk
    // spends the wait.
    const auto standing = out + 1 + still;
    fm_assert(standing + 2 >= 2*s.fps && standing <= 2*s.fps + 2);
    fm_assert(seen - still >= s.fps/2);
    const auto hidden2 = s.next_stop();
    fm_assert(cant_be_shot_at(s.w, threat_pos, s.threat->id, hidden2, body, s.npc->id));
}

// Half a second after the npc is back at cover from its first peek, the threat moves north-east of
// the wall, from where it can shoot the npc at the cover spot. The npc walks south, past the
// wall's south end, to a spot just below it that the wall hides from there. It stands there for
// what was left of its 2 s hidden, which the walk doesn't spend, then steps out. The wall is too
// narrow for that spot to stay hidden from the threat turned an octant either way, so the test
// sets octants to 0, and the search checks the spot only from where the threat stands. The cover
// slides to the wall's end, which is then 4 px farther out than with octants.
void test_cover_script_follows_while_hidden()
{
    cover_scene s;
    set_cover_script_options(*s.npc, {.search_every_move = true, .octants = 0});
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
    // The first update after the teleport started with the npc standing at cover, so it spent a
    // frame of the wait, and follow_threat started the walk in it. The slack and the walk's least
    // length are test_cover_script_keeps_peeking's, for the same reasons.
    const auto standing = before + 1 + still;
    fm_assert(standing + 2 >= 2*s.fps && standing <= 2*s.fps + 2);
    fm_assert(walk >= s.fps/2);
    fm_assert(spot_hidden);
}

// The threat stands 56 px east of the wall's east face, and the npc's cover 28 px west of it. A
// ray from the threat to a spot the npc peeks from then passes the face's corner two thirds of
// the way, so the spot moves 1 px for each 2 px the threat moves along the wall. With a second of
// the npc's 2 s out left, the threat walks 1 px per frame along the wall, away from the side the
// npc peeks on and back, over 4 px. The npc follows the spot and still turns back to cover when
// its 2 s are up. Without search_every_move, it follows by moving along the face only when the
// wall hides its gun, and never searches for cover, since the wall still hides that.
void test_cover_script_threat_walks_along_wall(bool search_every_move)
{
    cover_scene s{point{ch, {8, 8}, {}}};
    set_cover_script_options(*s.npc, {.search_every_move = search_every_move});
    const auto hidden = s.next_stop();
    (void)s.stay(2*s.fps);
    const auto peek = s.next_stop();
    const auto slide = Vector3i(peek).xy() - Vector3i(hidden).xy();
    fm_assert(slide.x() == 0 && slide.y() != 0);
    const auto side = slide.y() > 0 ? 1 : -1;
    const auto searches = s.status().searches;

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
        fm_assert(search_every_move || s.status().searches == searches);
    }
    fm_assert(farthest > 0);
}

// From the npc's arrival at cover, the threat walks east 1 px per frame, away from the wall, so
// the same spot stays hidden. The npc still steps out when its 2 s there are up.
void test_cover_script_walking_threat(point hidden)
{
    cover_scene s;
    for (auto frames = 0u; s.npc->position() != hidden; frames++)
    {
        fm_assert(frames < 60*s.fps);
        s.step();
    }
    const auto frames = s.stay(2*s.fps + 2, Vector2i{1, 0});
    fm_assert(frames + 2 >= 2*s.fps);
}

// The npc's cover and peek spots in a cover_scene made with these arguments. It asserts that the
// threat sees the npc's gun at the peek spot and not with the slide 1 px shorter. The gun is
// gun_offset px to the npc's right, or where the 3-argument make_cover_script puts it when
// gun_offset is empty. max_slide is the script's option.
cover_cycle peek_with_gun(Optional<int> gun_offset, const Optional<point>& other_pos = {}, Vector2ui npc_size = body,
                          uint32_t max_slide = cover_script_options{}.max_slide)
{
    cover_scene s{point{ch, {12, 8}, {}}, gun_offset, other_pos, npc_size};
    set_cover_script_options(*s.npc, {.max_slide = max_slide});
    const auto hidden = s.next_stop();
    // The slide is along y, so the default is half of bbox_size.y(). find_peek() reads bbox_size
    // at hidden, and each walk turns the npc, which rotates bbox_size.
    const auto offset = gun_offset ? *gun_offset : (int)s.npc->bbox_size.y()/2;
    (void)s.stay(2*s.fps);
    const auto peek = s.next_stop();
    const auto slide = Vector3i(peek).xy() - Vector3i(hidden).xy();
    fm_assert(slide.x() == 0 && slide.y() != 0);
    const auto side = slide.y() > 0 ? 1 : -1;
    const auto threat = s.threat->position();
    fm_assert(s.seen_from(threat, gun_at(peek, threat, offset)));
    fm_assert(!s.seen_from(threat, gun_at(peek - Vector2i{0, side}, threat, offset)));
    return { hidden, peek };
}

// The npc faces the threat in the east, and north is -y, so its right is +y. Compared with a gun at
// its center, a gun to its right shortens the slide to its right and lengthens the one to its left,
// and a gun to its left does the reverse. The default puts the gun half the body's width to its
// right. A 30 by 24 px body's width along the wall differs from its depth toward the threat,
// whichever way it is turned, so peek_with_gun's check of the default tells the two apart. The
// cover doesn't slide, so it stays behind the wall's middle, with room to peek on both sides.
void test_cover_script_gun_offset()
{
    constexpr auto right = 1, left = -1;
    const auto hidden = peek_with_gun(0, {}, body, 0).hidden;
    // A critter stands 1 px past the body at hidden on the side not tested.
    const auto slide = [&](int offset, int side) {
        const auto c = peek_with_gun(offset, hidden - Vector2i{0, side*((int)body.y() + 1)}, body, 0);
        fm_assert(c.hidden == hidden);
        const auto dy = Vector3i(c.peek).y() - Vector3i(c.hidden).y();
        fm_assert(dy*side > 0);
        return Math::abs(dy);
    };
    const auto r0 = slide(0, right), l0 = slide(0, left);
    const auto r7 = slide(7, right), l7 = slide(7, left);
    const auto r11 = slide(-11, right), l11 = slide(-11, left);
    fm_assert(r7 < r0 && r0 < r11);
    fm_assert(l7 > l0 && l0 > l11);

    const auto d = peek_with_gun({}, {}, body, 0);
    fm_assert(d.hidden == hidden);
    fm_assert(d.peek == peek_with_gun((int)body.y()/2, {}, body, 0).peek);
    fm_assert(Vector3i(d.peek).y() > Vector3i(hidden).y());
    (void)peek_with_gun({}, {}, Vector2ui{30, 24}, 0);
}

// A critter stands 1 px past the body at the npc's peek spot, on the far side from the cover, so
// every longer slide runs into it. The slide there, 44 px, is no multiple of find_peek's step of
// half the body, so the first step past it is blocked. The npc still peeks from that spot. The
// cover doesn't slide, so it stays behind the wall's middle.
void test_cover_script_peek_beside_critter()
{
    const auto c = peek_with_gun({}, {}, body, 0);
    const auto side = Vector3i(c.peek).y() > Vector3i(c.hidden).y() ? 1 : -1;
    const auto c2 = peek_with_gun({}, c.peek + Vector2i{0, side*((int)body.y() + 1)}, body, 0);
    fm_assert(c2.hidden == c.hidden && c2.peek == c.peek);
}

// The one of the 8 rotations nearest v's direction, from v's angle clockwise from north, which is
// -y. The angle must lie at least 1° from a boundary between two of them, so that atan2's rounding
// can't change the result.
rotation nearest_of_8(Vector2i v)
{
    const auto octants = std::atan2((double)v.x(), (double)-v.y()) / (Math::Constants<double>::pi()/4);
    const auto k = std::round(octants);
    fm_assert(.5 - std::abs(octants - k) >= 1./45);
    return rotation((uint8_t)(((int)k + 8) % 8));
}

// The npc's gun is 23 px to its left, and a critter stands 1 px north of the body at hidden, so the
// npc peeks past the wall's south end. From the update it arrives there, it faces the threat in the
// east. Half a second later the threat moves to 13 px east and 3 px north of the wall's south-east
// corner. From there the gun at the same spot is still the first the threat sees, so the npc stays
// there, and it faces the threat's new direction from the next update until its 2 s out are up.
// The wall still hides the cover, so the npc never searches. It casts two rays in the update after
// the threat moves, to its gun and to its cover, and none while the threat stands still. From that
// update on, the status's gun ray goes from the threat's new spot to the gun point for it. The
// cover doesn't slide, so it stays behind the wall's middle.
void test_cover_script_faces_threat()
{
    constexpr auto gun_offset = -23;
    const auto hidden = peek_with_gun(0, {}, body, 0).hidden;
    const auto north = hidden - Vector2i{0, (int)body.y() + 1};
    const auto peek = peek_with_gun(gun_offset, north, body, 0).peek;
    cover_scene s{point{ch, {12, 8}, {}}, gun_offset, north};
    set_cover_script_options(*s.npc, {.max_slide = 0});
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
    fm_assert(t0.gun_seen_from == threat0 && t0.gun == gun_at(peek, threat0, gun_offset));
    for (auto k = 0u; k < out; k++)
    {
        s.step();
        fm_assert(s.npc->position() == peek && s.npc->r == r1);
        fm_assert_equal(t0.rays, s.status().rays);
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
        fm_assert_equal(t0.searches, t.searches);
        fm_assert_equal(t0.rays + 2, t.rays);
        if (s.npc->position() != peek)
            break;
        fm_assert(s.npc->r == r2);
        fm_assert(t.gun_seen_from == threat_pos && t.gun == gun2);
        fm_assert(++frames <= 2*s.fps + 2);
    }
    // As in test_cover_script, a wait may end a frame early or late. The update in which the wait
    // runs out starts the walk back without a move, so frames counts it.
    const auto standing = out + frames;
    fm_assert(standing + 2 >= 2*s.fps && standing <= 2*s.fps + 2);
}

using state = cover_script_status::state;

// The npc of cover_scene hides at hidden, having cast no rays, since the threat stood still. While
// the threat walks east 1 px per frame, the npc casts one ray per update, to its body's center,
// which the wall hides, and doesn't search. Then the threat moves north-west of the wall, from
// where that ray reaches the center. The npc searches in the next update and walks to cover that
// hides it from there.
void test_cover_script_hidden_checks(point hidden)
{
    cover_scene s;
    fm_assert(!get_cover_script_status(*s.threat));
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == hidden);
    const auto t0 = s.status();
    fm_assert(t0.has_cover && t0.cover == hidden && !t0.has_peek);
    fm_assert_equal(1u, t0.searches);
    fm_assert_equal(1u, t0.paths);
    for (auto k = 0u; k < 10; k++)
    {
        s.step();
        fm_assert_equal(0u, s.status().rays);
    }
    for (auto k = 1u; k <= 15; k++)
    {
        s.move_threat(s.threat->position() + Vector2i{1, 0});
        s.step();
        const auto t = s.status();
        fm_assert(s.npc->position() == hidden && t.st == state::hidden);
        fm_assert_equal(1u, t.searches);
        fm_assert_equal(k, t.rays);
        fm_assert_equal(1u, t.paths);
    }

    const point threat_pos{ch, {2, 4}, {}};
    s.move_threat(threat_pos);
    fm_assert(s.seen_from(threat_pos, hidden));
    s.step();
    const auto t = s.status();
    fm_assert_equal(2u, t.searches);
    fm_assert_equal(2u, t.paths);
    fm_assert(t.st == state::to_cover && t.cover != hidden);
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == t.cover);
    fm_assert(cant_be_shot_at(s.w, threat_pos, s.threat->id, t.cover, body, s.npc->id));
}

using stay_phase = cover_script_options::stay_phase;

// The npc of cover_scene steps through stay's values, staying put 4 s in each kept phase:
// - Kept hidden at hidden, it steps out in the update after stay becomes exposed, and stays at peek.
// - Back to alternate, it peeks for its whole 2 s out, which the kept phase didn't spend, then
//   hides again at hidden.
// - Kept hidden there, it still walks to new cover when the threat moves to where it sees hidden,
//   as test_cover_script_hidden_checks, and stays hidden there.
void test_cover_script_stays_hidden(point hidden, point peek)
{
    cover_scene s;
    const auto stays_at = [&](point pos, state st) {
        for (auto k = 0u; k < 4*s.fps; k++)
        {
            s.step();
            fm_assert(s.npc->position() == pos && s.status().st == st);
        }
    };
    set_cover_script_options(*s.npc, {.stay = stay_phase::hidden});
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == hidden);
    stays_at(hidden, state::hidden);

    set_cover_script_options(*s.npc, {.stay = stay_phase::exposed});
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

    set_cover_script_options(*s.npc, {.stay = stay_phase::hidden});
    const point threat_pos{ch, {2, 4}, {}};
    s.move_threat(threat_pos);
    fm_assert(s.seen_from(threat_pos, hidden));
    s.step();
    const auto t = s.status();
    fm_assert(t.st == state::to_cover && t.cover != hidden);
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == t.cover);
    stays_at(t.cover, state::hidden);
}

// The npc of cover_scene, kept exposed from the start, hides at hidden for one update and steps
// out to peek. Half a second later the threat moves as in test_cover_script_flanked, and the npc,
// flanked, leaves when its 2 s out are up for the cover its search found. It hides there for one
// update and steps out again. Kept hidden from there, it walks back to the new cover in the next
// update and stays.
void test_cover_script_stays_exposed(point hidden, point peek)
{
    cover_scene s;
    set_cover_script_options(*s.npc, {.stay = stay_phase::exposed});
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
    const auto frames = s.stay(2*s.fps + 3);
    fm_assert(frames + 2 >= 2*s.fps);
    const auto cover = s.status().cover;
    fm_assert(cover != hidden);
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == cover);
    s.step();
    fm_assert(s.status().st == state::to_peek);
    s.step_until(state::exposed);
    const auto peek2 = s.npc->position();
    fm_assert(peek2 != cover);

    set_cover_script_options(*s.npc, {.stay = stay_phase::hidden});
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

// Half a second after the npc of cover_scene reaches peek, the threat moves to the wall's west
// side, from where the cover spot is in the open. In the next update the npc searches and finds
// cover behind the wall's east face, but stays out until its 2 s are up. Then it walks there
// around the wall, without searching again, and hides.
void test_cover_script_flanked(point hidden, point peek)
{
    cover_scene s;
    s.step_until(peek);
    const auto out = s.fps/2;
    for (auto k = 0u; k < out; k++)
        s.step();
    fm_assert(s.npc->position() == peek);
    const auto t0 = s.status();
    fm_assert(t0.st == state::exposed && t0.cover == hidden && t0.has_peek && t0.peek == peek);

    const point threat_pos{ch, {2, 8}, {}};
    s.move_threat(threat_pos);
    fm_assert(s.seen_from(threat_pos, hidden));
    auto frames = 0u;
    for (;;)
    {
        s.step();
        const auto t = s.status();
        fm_assert_equal(t0.searches + 1, t.searches);
        fm_assert(t.cover != hidden && Vector3i(t.cover).x() > Vector3i(hidden).x());
        if (s.npc->position() != peek)
            break;
        // The update in which the wait runs out starts the walk without a move.
        fm_assert(t.st == state::exposed || t.st == state::to_cover);
        fm_assert(++frames <= 2*s.fps + 2);
    }
    // As in test_cover_script_faces_threat.
    const auto standing = out + frames;
    fm_assert(standing + 2 >= 2*s.fps && standing <= 2*s.fps + 2);
    const auto cover = s.status().cover;
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == cover);
    fm_assert_equal(t0.searches + 1, s.status().searches);
    fm_assert(cant_be_shot_at(s.w, threat_pos, s.threat->id, cover, body, s.npc->id));
}

// As in test_cover_script_flanked, but the threat moves to test_cover_script_retreat's spot north
// of the wall, from where nothing within 5 tiles hides the npc. The npc stays out until its 2 s
// are up, then walks to a spot more than 4 tiles farther from the threat, and looks for cover
// again there.
void test_cover_script_flanked_retreat(point peek)
{
    cover_scene s;
    s.step_until(peek);
    const auto out = s.fps/2;
    for (auto k = 0u; k < out; k++)
        s.step();
    const auto t0 = s.status();
    fm_assert(t0.st == state::exposed);

    const point threat_pos{ch, {7, 1}, {}};
    s.move_threat(threat_pos);
    fm_assert(s.seen_from(threat_pos, t0.cover));
    auto frames = 0u;
    for (;;)
    {
        s.step();
        fm_assert_equal(t0.searches + 1, s.status().searches);
        if (s.npc->position() != peek)
            break;
        fm_assert(++frames <= 2*s.fps + 2);
    }
    const auto standing = out + frames;
    fm_assert(standing + 2 >= 2*s.fps && standing <= 2*s.fps + 2);
    fm_assert(s.status().st == state::retreat);
    s.step_until(state::seek);
    const auto dist = [&](point p) { return std::sqrt((double)point::distance_squared(p, threat_pos)); };
    fm_assert(dist(s.npc->position()) - dist(peek) > 4*tile_size_xy);
}

// As in test_cover_script_flanked, the threat moves to the wall's west side half a second after
// the npc of cover_scene reaches peek, and the npc finds cover behind the wall's east face. A
// pillar stands halfway between peek and north_west, a spot north-west of the wall. Half a second
// later the threat moves there, from where the wall hides the new cover and the pillar hides the
// gun at peek. The npc neither searches nor moves. Half a second later the threat moves back east of the
// wall, from where it sees the new cover, and the npc searches again in the next update. It
// still stays out until its 2 s are up, then hides at the cover that search found.
void test_cover_script_flanked_moves(point hidden, point peek)
{
    const point west{ch, {2, 8}, {}}, north_west{ch, {2, 2}, {}}, east{ch, {12, 8}, {}};
    cover_scene s;
    const auto pillar = peek + (north_west - peek)/2;
    fm_assert(pillar.chunk3() == ch);
    add_blocker(s.w, pillar.local(), {16, 16}, pillar.offset());
    rebuild(s.w);
    s.step_until(peek);
    const auto out = s.fps/2;
    for (auto k = 0u; k < out; k++)
        s.step();
    const auto t0 = s.status();
    fm_assert(t0.st == state::exposed && t0.cover == hidden);

    s.move_threat(west);
    s.step();
    const auto t1 = s.status();
    fm_assert_equal(t0.searches + 1, t1.searches);
    fm_assert(t1.st == state::exposed && Vector3i(t1.cover).x() > Vector3i(hidden).x());
    for (auto k = 0u; k < out; k++)
        s.step();

    s.move_threat(north_west);
    fm_assert(!s.seen_from(north_west, t1.cover) && !s.seen_from(north_west, peek));
    for (auto k = 0u; k < out; k++)
    {
        s.step();
        const auto t = s.status();
        fm_assert(s.npc->position() == peek && t.st == state::exposed && t.cover == t1.cover);
        fm_assert_equal(t1.searches, t.searches);
    }

    s.move_threat(east);
    fm_assert(s.seen_from(east, t1.cover));
    auto frames = 0u;
    for (;;)
    {
        s.step();
        const auto t = s.status();
        fm_assert_equal(t1.searches + 1, t.searches);
        if (s.npc->position() != peek)
            break;
        // The update in which the wait runs out starts the walk without a move.
        fm_assert(t.st == state::exposed || t.st == state::to_cover);
        fm_assert(++frames <= s.fps + 2);
    }
    // As in test_cover_script_faces_threat.
    const auto standing = 3*out + 1 + frames;
    fm_assert(standing + 2 >= 2*s.fps && standing <= 2*s.fps + 2);
    const auto cover = s.status().cover;
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == cover);
    fm_assert_equal(t1.searches + 1, s.status().searches);
    fm_assert(cant_be_shot_at(s.w, east, s.threat->id, cover, body, s.npc->id));
}

// As in test_cover_script_flanked_retreat, the threat moves north of the wall half a second after
// the npc of cover_scene reaches peek, and the npc finds only a spot to retreat to. Half a second
// later the threat moves back to where it started, from where the wall hides the cover again and
// the threat sees the gun at peek. The npc stays out until its 2 s are up, then walks straight
// back to the cover and hides there.
void test_cover_script_flanked_hidden_again(point hidden, point peek)
{
    const point north{ch, {7, 1}, {}};
    cover_scene s;
    const auto start = s.threat->position();
    s.step_until(peek);
    const auto out = s.fps/2;
    for (auto k = 0u; k < out; k++)
        s.step();
    const auto t0 = s.status();
    fm_assert(t0.st == state::exposed && t0.cover == hidden);

    s.move_threat(north);
    fm_assert(s.seen_from(north, hidden));
    fm_assert(!find(s.w, peek, north, 5*tile_size_xy, 8).found);
    s.step();
    fm_assert_equal(t0.searches + 1, s.status().searches);
    for (auto k = 0u; k < out; k++)
        s.step();

    s.move_threat(start);
    fm_assert(!s.seen_from(start, hidden) && s.seen_from(start, peek));
    auto frames = 0u;
    for (;;)
    {
        s.step();
        fm_assert_equal(t0.searches + 1, s.status().searches);
        if (s.npc->position() != peek)
            break;
        fm_assert(++frames <= s.fps + 2);
    }
    const auto standing = 2*out + 1 + frames;
    fm_assert(standing + 2 >= 2*s.fps && standing <= 2*s.fps + 2);
    fm_assert(s.status().st == state::to_cover);
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == hidden);
}

// As in test_cover_script_flanked, the threat moves to the wall's west side half a second after
// the npc of cover_scene reaches peek, and the npc finds cover behind the wall's east face. Half a
// second later the threat moves north of the wall, from where it sees that cover too, and the
// npc finds only a spot to retreat to. Half a second later the threat moves back west. The npc
// searches again, since the threat sees the cover it peeks from, and finds the same cover behind
// the east face. When its 2 s are up, it walks there around the wall and hides.
void test_cover_script_flanked_retreat_cover(point hidden, point peek)
{
    const point west{ch, {2, 8}, {}}, north{ch, {7, 1}, {}};
    cover_scene s;
    s.step_until(peek);
    const auto out = s.fps/2;
    for (auto k = 0u; k < out; k++)
        s.step();
    const auto t0 = s.status();
    fm_assert(t0.st == state::exposed && t0.cover == hidden);

    s.move_threat(west);
    s.step();
    const auto t1 = s.status();
    fm_assert_equal(t0.searches + 1, t1.searches);
    fm_assert(Vector3i(t1.cover).x() > Vector3i(hidden).x());
    for (auto k = 0u; k < out; k++)
        s.step();

    s.move_threat(north);
    fm_assert(s.seen_from(north, t1.cover));
    fm_assert(!find(s.w, peek, north, 5*tile_size_xy, 8).found);
    s.step();
    fm_assert_equal(t0.searches + 2, s.status().searches);
    for (auto k = 0u; k < out; k++)
        s.step();

    s.move_threat(west);
    s.step();
    const auto t3 = s.status();
    fm_assert_equal(t0.searches + 3, t3.searches);
    fm_assert(t3.st == state::exposed && t3.cover == t1.cover);
    for (auto frames = 0u; s.npc->position() == peek; frames++)
    {
        fm_assert(frames <= s.fps + 2);
        s.step();
    }
    // A walk that is blocked would send the npc to seek.
    for (auto frames = 0u; s.status().st != state::hidden; frames++)
    {
        fm_assert(frames < 60*s.fps);
        fm_assert(s.status().st == state::to_cover);
        s.step();
    }
    fm_assert(s.npc->position() == t1.cover);
}

// Three blockers north, east and south of the cover behind cover_scene's wall's east face. With the
// wall they close every way there, since each gap they leave is narrower than the body. Dijkstra
// doesn't see critters between its ends, so critters in their place would leave the path open.
// The wall spans x 440 to 456 and y 464 to 560. The blockers span x 452 to 510 and y 440 to 456,
// x 494 to 510 and y 440 to 592, and x 452 to 510 and y 575 to 591.
void wall_in_east_cover(world& w)
{
    add_blocker(w, {8, 7}, {58, 16}, {-31, 0});
    add_blocker(w, {8, 8}, {16, 152}, {-10, 4});
    add_blocker(w, {8, 9}, {58, 16}, {-31, 7});
    rebuild(w);
}

bool walled_in(point pos)
{
    const auto v = Vector3i(pos).xy();
    const auto h = Vector2i(body)/2;
    return v.x() - h.x() >= 456 && v.x() + h.x() <= 494 && v.y() - h.y() >= 456 && v.y() + h.y() <= 575;
}

// As in test_cover_script_flanked, but once the npc stands at peek, wall_in_east_cover closes the
// cover behind the wall's east face. The search from peek with the threat on the wall's west side
// and the script's octants still finds that cover first. With no path to it, a second search lists
// the others, and the npc takes the first with a path, east of the blockers. It stays out until its
// 2 s are up, then hides there.
void test_cover_script_flanked_no_path(point peek)
{
    const point west{ch, {2, 8}, {}};
    cover_scene s;
    s.step_until(peek);
    const auto out = s.fps/2;
    for (auto k = 0u; k < out; k++)
        s.step();
    const auto t0 = s.status();
    fm_assert(t0.st == state::exposed);
    wall_in_east_cover(s.w);

    s.move_threat(west);
    const auto r = find(s.w, peek, west, 5*tile_size_xy, 8, body, Search::never_continue(), false,
                        cover_script_options{}.octants);
    fm_assert(r.found && walled_in(r.pos));
    auto frames = 0u;
    for (;;)
    {
        s.step();
        fm_assert_equal(t0.searches + 2, s.status().searches);
        if (s.npc->position() != peek)
            break;
        fm_assert(++frames <= 2*s.fps + 2);
    }
    const auto standing = out + frames;
    fm_assert(standing + 2 >= 2*s.fps && standing <= 2*s.fps + 2);
    const auto t1 = s.status();
    fm_assert(t1.st == state::to_cover);
    fm_assert(Vector3i(t1.cover).x() - (int)body.x()/2 >= 510);
    s.step_until(t1.cover);
    fm_assert(s.status().st == state::hidden);
    fm_assert(!s.seen_from(west, t1.cover));
}

// The npc of cover_scene hides behind the wall's west face. wall_in_east_cover closes the cover
// behind its east face, and the threat moves to the wall's west side, from where it sees the npc.
// The nearest cover is the one walled in, with no path, so a second search lists the others, and
// the npc walks to the first with a path, east of the blockers, and hides there.
void test_cover_script_hidden_no_path(point hidden)
{
    const point west{ch, {2, 8}, {}};
    cover_scene s;
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == hidden);
    const auto t0 = s.status();
    wall_in_east_cover(s.w);

    s.move_threat(west);
    fm_assert(s.seen_from(west, hidden));
    const auto r = find(s.w, hidden, west, 5*tile_size_xy, 8, body, Search::never_continue(), false,
                        cover_script_options{}.octants);
    fm_assert(r.found && walled_in(r.pos));
    s.step();
    const auto t1 = s.status();
    fm_assert_equal(t0.searches + 2, t1.searches);
    fm_assert(t1.st == state::to_cover);
    fm_assert(Vector3i(t1.cover).x() - (int)body.x()/2 >= 510);
    s.step_until(t1.cover);
    fm_assert(s.status().st == state::hidden);
    fm_assert(!s.seen_from(west, t1.cover));
}

// The npc of cover_scene walks to cover, casting no rays while the threat stands still. While the
// threat walks east 1 px per frame, the npc casts one ray per update, to the cover's center, which
// the wall hides, and doesn't search. Then the threat moves to the wall's west side, from where the
// cover is in the open. The npc searches in the next update and walks to cover behind the wall's
// east face without passing the old one.
void test_cover_script_walk_checks(point hidden)
{
    cover_scene s;
    for (auto k = 0u; k < 10; k++)
    {
        s.step();
        fm_assert(s.status().st == state::to_cover);
        fm_assert_equal(0u, s.status().rays);
    }
    for (auto k = 1u; k <= 15; k++)
    {
        s.move_threat(s.threat->position() + Vector2i{1, 0});
        s.step();
        const auto t = s.status();
        fm_assert(t.st == state::to_cover && t.cover == hidden);
        fm_assert_equal(1u, t.searches);
        fm_assert_equal(k, t.rays);
    }

    const point threat_pos{ch, {2, 8}, {}};
    s.move_threat(threat_pos);
    fm_assert(s.seen_from(threat_pos, hidden));
    s.step();
    const auto t = s.status();
    fm_assert_equal(2u, t.searches);
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

// The npc of cover_scene steps out toward peek. On the way, the threat moves 1 px east, from where
// it still sees the npc's gun at peek, and the npc casts its two rays and walks on. Then the threat
// moves 32 px along the wall away from the side the npc peeks on, so the wall hides the gun at
// peek. In the next update the npc turns to a spot farther along, from where the threat sees the
// gun, without searching. Then the threat moves to the wall's west side, from where the cover is
// in the open. The npc searches in the next update and walks to cover behind the wall's east
// face, where it hides for 2 s.
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
    fm_assert_equal(t0.searches, t.searches);
    fm_assert_equal(t0.rays + 2, t.rays);
    fm_assert(t.st == state::to_peek && t.peek == peek);

    const auto threat1 = s.threat->position() - Vector2i{0, 32*side};
    s.move_threat(threat1);
    fm_assert(!s.seen_from(threat1, peek));
    s.step();
    const auto t1 = s.status();
    fm_assert_equal(t0.searches, t1.searches);
    fm_assert(t1.st == state::to_peek && t1.has_peek);
    fm_assert((Vector3i(t1.peek).y() - Vector3i(peek).y())*side > 0);
    fm_assert(s.seen_from(threat1, t1.peek));

    const point threat2{ch, {2, 8}, {}};
    s.move_threat(threat2);
    fm_assert(s.seen_from(threat2, hidden));
    s.step();
    const auto t2 = s.status();
    fm_assert_equal(t0.searches + 1, t2.searches);
    fm_assert(t2.st == state::to_cover && t2.cover != hidden);
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == t2.cover);
    const auto frames = s.stay(2*s.fps + 2);
    fm_assert(frames + 2 >= 2*s.fps);
}

// The npc of cover_scene stands at peek for a second. Then the threat moves 32 px along the wall,
// away from the side the npc peeks on, so the wall hides both the cover and the gun at peek. The
// npc walks to a spot farther along, with a second of its 2 s out left. On the way there, the
// threat moves to the wall's west side, from where the cover is in the open. The npc walks to
// cover behind the wall's east face and hides there for all of its 2 s, the second left over
// not counting.
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
    fm_assert_equal(t0.searches, t1.searches);
    fm_assert(t1.st == state::to_peek && t1.has_peek);
    fm_assert((Vector3i(t1.peek).y() - Vector3i(peek).y())*side > 0);

    const point threat2{ch, {2, 8}, {}};
    s.move_threat(threat2);
    fm_assert(s.seen_from(threat2, hidden));
    s.step();
    const auto t2 = s.status();
    fm_assert_equal(t0.searches + 1, t2.searches);
    fm_assert(t2.st == state::to_cover && t2.cover != hidden);
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == t2.cover);
    const auto frames = s.stay(2*s.fps + 2);
    fm_assert(frames + 2 >= 2*s.fps);
}

// From the npc's arrival at cover, the threat walks east 1 px per frame, and the wall keeps hiding
// the same spot. With search_every_move, the npc searches in each of those updates and casts no
// rays. Without it, it casts one ray per update and doesn't search. Either way it stays.
void test_cover_script_search_every_move(point hidden)
{
    for (const auto every : { false, true })
    {
        cover_scene s;
        set_cover_script_options(*s.npc, {.search_every_move = every});
        s.step_until(state::hidden);
        fm_assert(s.npc->position() == hidden);
        const auto t0 = s.status();
        for (auto k = 1u; k <= 15; k++)
        {
            s.move_threat(s.threat->position() + Vector2i{1, 0});
            s.step();
            const auto t = s.status();
            fm_assert(s.npc->position() == hidden && t.st == state::hidden);
            fm_assert_equal(t0.searches + (every ? k : 0), t.searches);
            fm_assert_equal(t0.rays + (every ? 0 : k), t.rays);
            fm_assert(t.last_update <= t.max_update && t.max_update <= t.total_update);
        }
        fm_assert(s.status().total_update > t0.total_update);
    }
}

// The npc of cover_scene hides at hidden. Then the threat moves 7 tiles north of the wall's middle,
// from where hidden is in the open, and the wall's shadow is too narrow for the body near the
// wall, where a cover spot must be. The npc searches in the next update, finds no cover within 5
// tiles, and walks to a spot within 5 tiles of hidden that lies more than 4 tiles farther from the
// threat. It stands there and looks for cover again after 1 s. From the update it arrives, it faces
// the threat it walked away from. Halfway through the wait the threat stands 2 tiles beside it for
// 3 updates, and it faces the threat there too.
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
    fm_assert(!find(s.w, hidden, threat_pos, radius, 8).found);
    s.step();
    const auto t1 = s.status();
    fm_assert_equal(t0.searches + 1, t1.searches);
    fm_assert(t1.st == state::retreat);
    s.step_until(state::seek);
    const auto spot = s.npc->position();
    fm_assert(point::distance_squared(spot, hidden) <= (uint64_t)(radius*radius));
    const auto dist = [&](point p) { return std::sqrt((double)point::distance_squared(p, threat_pos)); };
    fm_assert(dist(spot) - dist(hidden) > 4*tile_size_xy);
    const auto r1 = nearest_of_8(threat_pos - spot);
    fm_assert(s.npc->r == r1);
    // toward the chunk's middle, where the threat's x is
    const auto side = Vector3i(spot).x() < Vector3i(threat_pos).x() ? 1 : -1;
    const auto beside = spot + Vector2i{side*2*tile_size_xy, 0};
    const auto r2 = nearest_of_8(beside - spot);
    fm_assert(r2 != r1);

    const auto searches = s.status().searches;
    const auto moved = [&](uint32_t frame) { return frame >= s.fps/2 && frame < s.fps/2 + 3; };
    auto frames = 0u;
    for (; s.status().searches == searches; frames++)
    {
        fm_assert(frames <= s.fps + 2);
        if (moved(frames) != moved(frames - 1))
            s.move_threat(moved(frames) ? beside : threat_pos);
        s.step();
        fm_assert(s.npc->position() == spot);
        fm_assert(s.npc->r == (moved(frames) ? r2 : r1));
    }
    fm_assert(frames + 2 >= s.fps);
    fm_assert(s.status().st == state::seek);
}

// The npc hides behind the wall at one of its ends. Sent to the last listed spot, at the other end,
// it walks there, hides, peeks from find_cover_peek's spot for it and hides there again. Sent to
// peek from the first, it walks to that spot's peek spot, stays out and hides at the first. A spot
// inside the wall has no path and no peek spot, and sending the npc there changes nothing. With the
// threat gone, no spot has a peek spot.
void test_cover_script_orders(point hidden)
{
    cover_scene s;
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == hidden);
    Array<cover_spot> spots;
    list_cover_spots(*s.npc, spots);
    fm_assert(spots.size() >= 2);
    fm_assert(spots[0].pos == hidden);
    const auto other = spots[spots.size() - 1], first = spots[0];

    const auto rays = s.status().rays;
    const auto p = find_cover_peek(*s.npc, other);
    fm_assert(p);
    fm_assert_equal(rays, s.status().rays);

    fm_assert(send_to_cover(*s.npc, other));
    {
        const auto t = s.status();
        fm_assert(t.st == state::to_cover);
        fm_assert(t.has_cover && t.cover == other.pos && !t.has_peek);
    }
    s.step_until(other.pos);
    fm_assert(s.status().st == state::hidden);
    s.step_until(state::exposed);
    {
        const auto t = s.status();
        fm_assert(s.npc->position() == p->pos);
        fm_assert(t.cover == other.pos && t.peek == p->pos && t.gun == p->gun);
    }
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == other.pos);

    const auto q = find_cover_peek(*s.npc, first);
    fm_assert(q);
    fm_assert(send_to_peek(*s.npc, first));
    {
        const auto t = s.status();
        fm_assert(t.st == state::to_peek);
        fm_assert(t.cover == first.pos && t.has_peek && t.peek == q->pos && t.gun == q->gun);
        fm_assert(t.gun_seen_from == s.threat->position());
    }
    s.step_until(state::exposed);
    fm_assert(s.npc->position() == q->pos);
    s.step_until(state::hidden);
    fm_assert(s.npc->position() == first.pos);

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
    test_lists_past_rejected();
    test_lists_by_area(0);
    test_lists_by_area(3);
    test_octants_wall_middle();
    test_octants_turned_threat_sees();
    test_octants_grid_rejects();
    for (const auto octants : { uint8_t{0}, uint8_t{3} })
    {
        test_slides_to_cover_end(octants, false);
        test_slides_to_cover_end(octants, true);
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
    test_cover_script_stays_hidden(hidden, c0.peek);
    test_cover_script_stays_exposed(hidden, c0.peek);
    test_cover_script_flanked_retreat(c0.peek);
    test_cover_script_flanked_moves(hidden, c0.peek);
    test_cover_script_flanked_hidden_again(hidden, c0.peek);
    test_cover_script_flanked_retreat_cover(hidden, c0.peek);
    test_cover_script_flanked_no_path(c0.peek);
    test_cover_script_hidden_no_path(hidden);
    test_cover_script_walk_checks(hidden);
    test_cover_script_peek_walk_checks(hidden, c0.peek);
    test_cover_script_peek_again_flanked(hidden, c0.peek);
    test_cover_script_search_every_move(hidden);
    test_cover_script_retreat(hidden);
    test_cover_script_orders(hidden);
    test_cover_script_keeps_peeking(hidden, c0.peek);
    test_cover_script_walking_threat(hidden);
    test_cover_script_follows_while_hidden();
    test_cover_script_threat_walks_along_wall(false);
    test_cover_script_threat_walks_along_wall(true);
    test_cover_script_gun_offset();
    test_cover_script_peek_beside_critter();
    test_cover_script_faces_threat();
    test_cover_script(hidden);
    // 1 px past the hidden body on the side it peeked from, so only the other side is left
    const auto side = Vector2i{0, Vector3i(peek).y() > Vector3i(hidden).y() ? 1 : -1};
    const auto c = test_cover_script(hidden + side*((int)body.y() + 1));
    fm_assert(c.hidden == hidden);
    fm_assert((Vector3i(c.peek).y() - Vector3i(c.hidden).y())*side.y() < 0);
}

} // namespace floormat
