#include "app.hpp"
#include "compat/borrowed-ptr.inl"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include "src/cover-point.hpp"
#include "src/critter.hpp"
#include "src/critter-script.hpp"
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
#include <cr/GrowableArray.h>
#include <cr/Optional.h>
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
                        const Search::pred& pred = Search::without_critters(), bool list_spots = false)
{
    static cover_point finder;
    Grid::PoolRegistry pools;
    return finder.find_cover_point(w, pools, Cover::Params{div_size}, {npc, size, threat, radius}, pred, list_spots);
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
    fm_assert(!r.found || r.spots[0] == r.pos);
    for (auto i = 0u; i < r.spots.size(); i++)
    {
        for (auto j = i + 1; j < r.spots.size(); j++)
            fm_assert(r.spots[i] != r.spots[j]);
        fm_assert(passable(w, r.spots[i], body));
        // Removed after the shot, so it can't stop the rays aimed at the next spot.
        const auto c = add_critter(w, r.spots[i]);
        fm_assert(cant_be_shot_at(w, threat, 0, r.spots[i], body, c->id));
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
        fm_assert(r.spots[i] == r1.spots[i]);
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
// what was left of its 2 s hidden, which the walk doesn't spend, then steps out.
void test_cover_script_follows_while_hidden(point hidden)
{
    cover_scene s;
    for (auto k = 0u; k < 2; k++)
    {
        (void)s.next_stop();
        (void)s.stay(2*s.fps);
    }
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
// its 2 s are up.
void test_cover_script_threat_walks_along_wall()
{
    cover_scene s{point{ch, {8, 8}, {}}};
    const auto hidden = s.next_stop();
    (void)s.stay(2*s.fps);
    const auto peek = s.next_stop();
    const auto slide = Vector3i(peek).xy() - Vector3i(hidden).xy();
    fm_assert(slide.x() == 0 && slide.y() != 0);
    const auto side = slide.y() > 0 ? 1 : -1;

    // next_stop() returned after a second at peek.
    const auto left = s.fps;
    constexpr auto amplitude = 4u;
    for (auto frames = 0u; ; frames++)
    {
        const auto dir = frames % (2*amplitude) < amplitude ? -side : side;
        auto i = s.threat->index();
        s.threat->teleport_to(i, s.threat->position() + Vector2i{0, dir}, rotation_COUNT);
        s.step();
        // The spots it peeks from lie on the far side of peek from the cover, so this is the walk back.
        if ((Vector3i(s.npc->position()).y() - Vector3i(peek).y())*side < 0)
        {
            // Those spots span less than amplitude px, and the walk back is 1 px per frame along
            // the face, so it crosses peek within amplitude + 1 frames.
            fm_assert(frames + 2 >= left && frames <= left + amplitude + 2);
            break;
        }
        fm_assert(frames <= left + amplitude + 2);
    }
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
// gun_offset is empty.
cover_cycle peek_with_gun(Optional<int> gun_offset, const Optional<point>& other_pos = {}, Vector2ui npc_size = body)
{
    cover_scene s{point{ch, {12, 8}, {}}, gun_offset, other_pos, npc_size};
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
// whichever way it is turned, so peek_with_gun's check of the default tells the two apart. hidden
// is the cover spot test_cover_script returned.
void test_cover_script_gun_offset(point hidden)
{
    constexpr auto right = 1, left = -1;
    // A critter stands 1 px past the body at hidden on the side not tested.
    const auto slide = [&](int offset, int side) {
        const auto c = peek_with_gun(offset, hidden - Vector2i{0, side*((int)body.y() + 1)});
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

    const auto d = peek_with_gun({});
    fm_assert(d.hidden == hidden);
    fm_assert(d.peek == peek_with_gun((int)body.y()/2).peek);
    fm_assert(Vector3i(d.peek).y() > Vector3i(hidden).y());
    (void)peek_with_gun({}, {}, Vector2ui{30, 24});
}

// A critter stands 1 px past the body at the npc's peek spot, on the far side from the cover, so
// every longer slide runs into it. The slide there, 44 px, is no multiple of find_peek's step of
// half the body, so the first step past it is blocked. The npc still peeks from that spot.
void test_cover_script_peek_beside_critter(point hidden)
{
    const auto c = peek_with_gun({});
    const auto side = Vector3i(c.peek).y() > Vector3i(hidden).y() ? 1 : -1;
    const auto c2 = peek_with_gun({}, c.peek + Vector2i{0, side*((int)body.y() + 1)});
    fm_assert(c2.hidden == hidden && c2.peek == c.peek);
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
    for (const auto size : { Vector2ui{23, 23}, Vector2ui{25, 25}, Vector2ui{23, 25}, Vector2ui{25, 23} })
    {
        test_narrow_pillar_rejected(size);
        test_gap_between_pillars_rejected(size);
        test_side_wall_corner_seen(size);
        test_shift_to_only_spot(3, false, size);
        test_shift_to_only_spot(-1, false, size);
    }
    const auto [hidden, peek] = test_cover_script();
    // cover_scene's gun is at the body's center, and test_cover_script's is not, so they peek from
    // different spots.
    const auto c0 = peek_with_gun(0);
    fm_assert(c0.hidden == hidden);
    test_cover_script_keeps_peeking(hidden, c0.peek);
    test_cover_script_walking_threat(hidden);
    test_cover_script_follows_while_hidden(hidden);
    test_cover_script_threat_walks_along_wall();
    test_cover_script_gun_offset(hidden);
    test_cover_script_peek_beside_critter(hidden);
    test_cover_script(hidden);
    // 1 px past the hidden body on the side it peeked from, so only the other side is left
    const auto side = Vector2i{0, Vector3i(peek).y() > Vector3i(hidden).y() ? 1 : -1};
    const auto c = test_cover_script(hidden + side*((int)body.y() + 1));
    fm_assert(c.hidden == hidden);
    fm_assert((Vector3i(c.peek).y() - Vector3i(c.hidden).y())*side.y() < 0);
}

} // namespace floormat
