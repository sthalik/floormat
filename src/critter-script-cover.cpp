#include "critter-script.hpp"
#include "critter-script-cover.hpp"
#include "critter-walk.hpp"
#include "cover-point.hpp"
#include "critter.hpp"
#include "grid-registry.hpp"
#include "nanosecond.inl"
#include "point.inl"
#include "intra-coord.inl"
#include "raycast.hpp"
#include "search.hpp"
#include "search-astar.hpp"
#include "search-result.hpp"
#include "tile-defs.hpp"
#include "timer.hpp"
#include "world.hpp"
#include "entity/name-of.hpp"
#include "compat/floor-divmod.hpp"
#include "compat/function2.hpp"
#include <algorithm>
#include <cr/GrowableArray.h>
#include <cr/Optional.h>
#include <mg/Functions.h>
#include <mg/Range.h>

namespace floormat {

namespace {

constexpr Ns hidden_time = 2*Second, exposed_time = 2*Second;
// before looking for cover again when there was none, or the walk there was blocked twice
constexpr Ns retry_time = Second;
// A retreat ends at a spot within retreat_radius of the npc that is over min_retreat_gain px farther from the
// threat. As the gain can't exceed the distance, that spot is over min_retreat_gain px from the npc too.
constexpr u32 retreat_radius = 5*(u32)tile_size_xy;
constexpr int min_retreat_gain = 3*tile_size_xy;
constexpr u32 search_radius = 2*chunk_size_xy;
constexpr u32 path_slack = 5*(u32)tile_size_xy;
// The work a search for cover or a retreat may do in one update. Counts, not time, so that the npc
// behaves the same on every machine.
// TODO: each critter has its own limits, so n critters searching in different chunks can do n
// octant fills in one frame.

// Tiles of search_tiles(), each listed by one Cover::Point::finder::find call over the tile's area.
constexpr u32 max_search_tiles_per_update = 32;
// An octant fill computes how far a shot travels from each cell of a chunk's cover grid along one of 32
// directions. A stale grid's rebuild counts as one. A rebuild and a fill took 0.66 ms at div 8.
constexpr u32 max_search_octant_fills_per_update = 1;
// path_to calls, each one Dijkstra search. One that found a path took up to 0.71 ms among dense scenery.
constexpr u32 max_search_dijkstra_calls_per_update = 1;
// Listed cover spots that hidden_from_reach doesn't reject. Each gets cover_end, up to 0.1 ms among dense
// scenery, and with needs_peek a search for its peek spot, which casts rays and took up to 42 microseconds.
constexpr u32 max_peek_spot_searches_per_update = 1;
// hidden_at calls made by hidden_from_reach, each up to 6 microseconds among dense scenery. Rejecting a spot
// may take a single call, so a spot that hidden_from_reach rejects counts against this cap only.
constexpr u32 max_search_body_checks_per_update = 64;
constexpr u32 max_retreat_dijkstra_calls_per_update = 4;
// sees() calls, each one ray from the threat to a spot's center.
constexpr u32 max_retreat_rays_per_update = 256;
// Search::is_passable calls for c's body at a spot, each an R-tree search.
constexpr u32 max_retreat_is_passable_calls_per_update = 512;
constexpr int max_peek_dist = 2*tile_size_xy;
// A path_to call that finds no path costs a Dijkstra search over every node within max_dist, so a search for
// cover or a retreat stops after this many of them.
constexpr u32 max_failed_dijkstra_calls = 8;

struct cover_script;
constexpr StringView script_name = name_of<cover_script>;
using ScriptPtr = Pointer<critter_script>;
using walk_result = critter::walk_result;

// path_to's max_dist: twice the distance plus path_slack, as the way around a blocker can be many
// times longer than the straight line.
u32 max_search_path_length(point from, point to) { return point::distance(from, to)*2 + path_slack; }

// Every collider but the critter itself blocks, other critters included, as when it moves.
auto blocks_all_but(object_id self)
{
    return [self](chunk&, collision_data x, Range2D) {
        return x.id == self ? path_search_continue::pass : path_search_continue::blocked;
    };
}

// Where the gun is when c's body is centered on pos and c faces threat: offset px to its right, or
// half of c's bbox_size along across without offset. A negative offset puts it on the left.
point gun_point(const critter& c, point pos, point threat, Vector2i across, Optional<int> offset)
{
    if (pos == threat)
        return pos;
    const auto dist = offset ? *offset : Math::dot(Vector2i(c.bbox_size), across)/2;
    const auto f = Vector2(threat - pos);
    return pos + Vector2i(Math::round(Vector2{-f.y(), f.x()} * ((float)dist / f.length())));
}

// A query's pos lies up to half a tile from its tile's center on each axis, so the list reaches a
// tile past search_radius.
const Array<Vector2i>& search_tiles()
{
    static const auto tiles = [] {
        Array<Vector2i> a;
        Cover::Point::nearest_tiles(search_radius + tile_size_xy, a);
        return a;
    }();
    return tiles;
}

// Takes cover from a critter and steps out of it and back, each phase lasting until its wait runs out.
// Only updates that start with the critter standing spend the wait.
struct cover_script final : critter_script
{
    using state = Cover::Script::status::state;

    StringView name() const override;
    const void* id() const override;
    void on_init(const bptr<critter>& c) override;
    void on_update(const bptr<critter>& c, usize& i, const Ns& dt) override;
    void on_destroy(const bptr<critter>& c, script_destroy_reason reason) override;
    void delete_self() noexcept override;

    cover_script(astar& A, object_id threat, u32 div_size, Optional<int> gun_offset);

    Cover::Script::status status() const;
    Cover::Point::result list_spots(critter& c, const Cover::Point::query& q, Array<Cover::Point::spot>& out);
    Optional<Cover::Script::peek> peek_for(critter& c, const Cover::Point::spot& s);
    bool send_to(critter& c, const Cover::Point::spot& s, bool peek);

    Cover::Script::options options;
    void set_options(critter& c, const Cover::Script::options& o);

private:
    struct retreat_spot { Vector2i offset; bool done; };
    // end is cover_end() of spot, and path leads there. path is empty when c stands on spot or end.
    struct reachable_cover { Cover::Point::spot spot; point end; path_search_result path; };
    // What end_search does with cover: hide there for hidden_time, for the wait left with relocate, or peek
    // from it with follow. Without cover, seek waits and the others retreat.
    enum class search_then : u8 { seek, relocate, follow, };

    void update(const bptr<critter>& c, usize& i, const Ns& dt);
    void go_on(const bptr<critter>& c, usize& i, const Ns& dt);
    bool wait_for(const Ns& dt);
    Ns time_in(state phase) const;
    bool holds() const;
    bool sees(critter& c, point from, point to);
    bool sees_gun(critter& c, point pos, point threat);
    bool hidden_at(critter& c, point threat, point pos, Vector2i axis);
    ArrayView<const point> reach_positions(critter& c, point threat);
    bool hidden_from_reach(critter& c, point threat, point pos, Vector2i axis);
    point cover_end(critter& c, const Cover::Point::spot& s, point threat);
    bool refresh_cover(critter& c);
    Vector2i across() const;
    bool start_search(critter& c, search_then next);
    void begin_search(critter& c, search_then next);
    void flee(critter& c);
    void restart_search(critter& c);
    void search_step(critter& c);
    Optional<reachable_cover> take_last_resort(critter& c);
    void end_search(critter& c, Optional<reachable_cover> r);
    bool joins_unreachable(critter& c, point to);
    u64 reach_pass_gen(critter& c) const;
    path_search_result path_to(critter& c, point to);
    void begin_retreat(critter& c);
    void retreat_step(critter& c);
    bool walk_to(critter& c, point to, state then);
    void walk_to_cover(critter& c, path_search_result path);
    void retry_later();
    void seek(const bptr<critter>& c);
    void follow_threat(critter& c);
    void check_hidden(critter& c, point threat);
    void check_exposed(critter& c, point threat);
    void check_walk(critter& c);
    bool check_retreat(critter& c);
    void peek_again(critter& c);
    void step_out(const bptr<critter>& c);
    void leave_peek(critter& c);
    void take_cover(const bptr<critter>& c);
    void walk_update(const bptr<critter>& c, usize& i, const Ns& dt);
    void face_threat(critter& c, usize i, point threat);
    Optional<Cover::Script::peek> peek_spot(critter& c, point from, Vector2i axis, point threat);
    Optional<point> find_peek_pos(critter& c, point threat);

    astar& A;
    critter_walk walk;
    Cover::Point::finder finder;
    Cover::Point::query query = {};
    Array<retreat_spot> retreat_spots;
    // The search's spots that have no path, or that joins_unreachable ties to one.
    Array<point> unreachable;
    // The spots of the search's tile search_tile, once tile_listed, of which tile_spot is next. They
    // were listed with the threat at tile_threat.
    Array<Cover::Point::spot> tile_spots;
    // The spots of tile search_tile that search_step skipped, which a tile listed again skips without testing.
    Array<Cover::Point::spot> tile_skipped;
    point tile_threat;
    // The first spots the search skipped for having no peek spot and for threat_reach, as options.last_resort keeps.
    Optional<Cover::Point::spot> peekless, in_reach;
    // reach_positions() for the threat at reach_from while reach_known, which each update clears. The others
    // are its fill's scratch.
    Array<point> reach;
    Array<u16> reach_dist;
    Array<u8> reach_open;
    Array<u64> reach_heap;
    point reach_from;
    bool reach_known = false;
    // How far from search_from the Dijkstra searches that put spots in unreachable could reach, and
    // reach_pass_gen()'s value once the last of them ended.
    u32 unreachable_reach = 0;
    u64 search_pass_gen = 0;
    u32 search_tile = 0, tile_spot = 0, num_failed_dijkstra_calls = 0, search_octant = 0;
    // Tiles the search may still pass, from 3/2 of search_tiles().size(), and 0 once it ended. A restart sends
    // search_tile to 0 only while more than a scan fits, so a moving threat can't keep the search near c.
    u32 search_tiles_left = 0;
    // Where the search's tiles and the threat's octant are measured from. The last restart to tile 0 set it to c's
    // position, or during a retreat walk to the walk's end, so that cover back toward the threat comes later.
    point search_from;
    // In state::find_retreat: the next of retreat_spots, this pass's path_to calls that found no path, and whether
    // the pass tries every spot left or only those a ray from threat_pos doesn't reach.
    u32 retreat_next = 0, num_failed_retreat_dijkstra_calls = 0;
    // The threat's octant seen from c in state::find_retreat, and how often a change of it, a blocked retreat
    // walk or check_retreat sent retreat_next to 0.
    u32 retreat_octant = 0, num_retreat_restarts = 0;
    // Rays the first pass may still cast, starting at one per spot farther than min_retreat_gain from c. That
    // many suffice without a restart, so running out ends the pass early only when the threat changed octant.
    u32 retreat_rays = 0;
    bool retreat_all = false;
    search_then then = search_then::seek;
    // A retreat started since start_search, so a kept last_resort spot starts no other.
    bool search_retreated = false;
    bool tile_listed = false;
    // canonical is the spot the search found, and cover where c hides, canonical moved along its face as far as
    // the threat alone sees no pixel of c. threat_pos is where the threat stood at the last check or search.
    point cover, canonical, threat_pos, peek_pos, gun_pos, gun_ray_from;
    Vector2i axis;
    Ns wait{}, last_update{}, max_update{}, total_update{};
    object_id threat_id;
    u32 num_searches = 0, num_rays = 0, num_body_checks = 0, num_dijkstra_calls = 0;
    u32 num_find_cover_point_calls = 0;
    Optional<int> gun_offset;
    state st = state::seek;
    u8 replans = 0;
    bool has_cover = false, has_peek = false, has_query = false;
};

StringView cover_script::name() const { return "cover"_s; }
const void* cover_script::id() const { return &script_name; }
void cover_script::delete_self() noexcept { delete this; }

// A may be destroyed already, at world teardown.
void cover_script::on_destroy(const bptr<critter>& c, script_destroy_reason) { c->clear_auto_movement(); }

cover_script::cover_script(astar& A, object_id threat, u32 div_size, Optional<int> gun_offset) :
    A{A}, threat_id{threat}, gun_offset{gun_offset}
{
    options.div_size = div_size;
    arrayReserve(unreachable, 16);
    arrayReserve(reach, 16);
    arrayReserve(reach_heap, 16);
}

Cover::Script::status cover_script::status() const
{
    return {
        .cover = cover, .canonical = canonical, .peek = peek_pos, .gun = gun_pos,
        .gun_ray_from = gun_ray_from, .query = query, .num_searches = num_searches, .num_rays = num_rays,
        .num_body_checks = num_body_checks, .num_dijkstra_calls = num_dijkstra_calls,
        .search_tiles_left = search_tiles_left, .search_tile = search_tile,
        .num_search_tiles = (u32)search_tiles().size(),
        .retreat_spot = retreat_next, .num_retreat_spots = (u32)retreat_spots.size(),
        .num_failed_retreat_dijkstra_calls = num_failed_retreat_dijkstra_calls, .retreat_all = retreat_all,
        .num_retreat_restarts = num_retreat_restarts, .num_find_cover_point_calls = num_find_cover_point_calls,
        .last_update = last_update, .max_update = max_update, .total_update = total_update,
        .st = st, .has_cover = has_cover, .has_peek = has_peek, .has_query = has_query,
    };
}

Cover::Point::result cover_script::list_spots(critter& c, const Cover::Point::query& q, Array<Cover::Point::spot>& out)
{
    arrayClear(out);
    const auto r = finder.find(c.world(), A.pool_registry(), Grid::Cover::Params{options.div_size}, q,
                                           blocks_all_but(c.id), true);
    const auto n = r.spots.size();
    if (arrayCapacity(out) < n)
        arrayReserve(out, n + n/2);
    arrayAppend(out, r.spots);
    return r;
}

void cover_script::on_init(const bptr<critter>& c)
{
    // find and Dijkstra take the body's center as the critter's position.
    fm_assert(c->bbox_offset == Vector2b{});
    c->moves.AUTO = true;
}

void cover_script::on_update(const bptr<critter>& c, usize& i, const Ns& dt)
{
    if (c->maybe_stop_auto_movement())
    {
        c->clear_auto_movement();
        c->script.do_clear(c);
        return;
    }
    // do_clear deletes the script, so the clock starts after it.
    auto t = Time::now();
    reach_known = false;
    update(c, i, dt);
    // threat_pos is where the threat stood at the last check or search, and in seek c checks
    // nothing. A retreat ends in seek with c facing away from a threat that may not have moved.
    if (st == state::seek)
    {
        if (const auto threat = c->world().find_object(threat_id))
            face_threat(*c, i, threat->position());
    }
    last_update = t.update();
    max_update = std::max(max_update, last_update);
    total_update += last_update;
}

void cover_script::update(const bptr<critter>& c, usize& i, const Ns& dt)
{
    switch (st)
    {
    case state::seek:
        if (wait_for(dt))
            seek(c);
        return;
    case state::search:
        search_step(*c);
        // This runs after search_step, so cover found in this update needs no retreat.
        if (st == state::search && (peekless || in_reach) && !search_retreated &&
            !hidden_at(*c, threat_pos, c->position(), Cover::Point::axis_toward(threat_pos - c->position())))
            begin_retreat(*c);
        go_on(c, i, dt);
        return;
    case state::to_cover:
    case state::to_peek:
        if (!options.search_every_move)
            check_walk(*c);
        go_on(c, i, dt);
        return;
    case state::find_retreat:
        retreat_step(*c);
        go_on(c, i, dt);
        return;
    case state::retreat:
        if ((options.search_every_move || !check_retreat(*c)) && search_tiles_left)
            search_step(*c);
        go_on(c, i, dt);
        return;
    case state::hidden:
    case state::exposed:
        if (!holds() && wait_for(dt))
        {
            if (st == state::hidden)
                step_out(c);
            else
                leave_peek(*c);
            return;
        }
        if (const auto threat = c->world().find_object(threat_id); !threat)
            retry_later();
        else if (const auto pos = threat->position(); pos != threat_pos)
        {
            // This update started with c standing, so its dt was spent on the wait above, and a
            // walk the checks start moves by the same dt.
            if (options.search_every_move)
                follow_threat(*c);
            else if (st == state::hidden)
                check_hidden(*c, pos);
            else
                check_exposed(*c, pos);
            go_on(c, i, dt);
        }
        return;
    }
}

// The rest of an update in which the checks or a search may have changed st: a walk moves by dt,
// and a c that stays exposed or arrived on its peek spot at once turns toward the threat.
void cover_script::go_on(const bptr<critter>& c, usize& i, const Ns& dt)
{
    if (st == state::to_cover || st == state::to_peek || st == state::retreat)
        walk_update(c, i, dt);
    else if (st == state::exposed)
        face_threat(*c, i, threat_pos);
}

bool cover_script::wait_for(const Ns& dt)
{
    if (dt < wait)
    {
        wait -= dt;
        return false;
    }
    wait = Ns{};
    return true;
}

// The wait c starts phase with, hidden or exposed: hidden_time or exposed_time, or none when
// options.hold keeps c in the other phase.
Ns cover_script::time_in(state phase) const
{
    using hold_phase = Cover::Script::options::hold_phase;
    if (phase == state::hidden)
        return options.hold == hold_phase::exposed ? Ns{} : hidden_time;
    return options.hold == hold_phase::hidden ? Ns{} : exposed_time;
}

// True when options.hold keeps c in the phase it stands in, so its wait isn't spent.
bool cover_script::holds() const
{
    using hold_phase = Cover::Script::options::hold_phase;
    return options.hold == (st == state::hidden ? hold_phase::hidden : hold_phase::exposed);
}

void cover_script::set_options(critter& c, const Cover::Script::options& o)
{
    using hold_phase = Cover::Script::options::hold_phase;
    const auto old = options.hold;
    options = o;
    reach_known = false;
    if (search_tiles_left && (st == state::search || st == state::find_retreat || st == state::retreat))
        restart_search(c);
    if (o.hold == old)
        return;
    const auto hiding = st == state::hidden || st == state::to_cover;
    const auto peeking = st == state::exposed || st == state::to_peek;
    if (hiding && o.hold == hold_phase::exposed || peeking && o.hold == hold_phase::hidden)
        wait = Ns{};
}

// True when a ray from `from` reaches `to` before any collider a shot can't pass. Critters don't
// stop it, c included. Every ray the script casts goes through here, so num_rays counts them all.
bool cover_script::sees(critter& c, point from, point to)
{
    num_rays++;
    auto& pool = A.pool_registry().pass_pool(Pass::Params{options.div_size, pass_mode::shoot_through});
    return raycast(c.world(), from, to, c.id, pool, Search::without_critters()).success;
}

// True when a ray from threat reaches gun_point() of c's body centered on pos. gun_pos and
// gun_ray_from then hold the ray's ends, for the status.
bool cover_script::sees_gun(critter& c, point pos, point threat)
{
    const auto gun = gun_point(c, pos, threat, across(), gun_offset);
    if (!sees(c, threat, gun))
        return false;
    gun_pos = gun;
    gun_ray_from = threat;
    return true;
}

// True when threat sees no pixel of c's body centered on pos, flush against a blocker along axis.
bool cover_script::hidden_at(critter& c, point threat, point pos, Vector2i axis)
{
    num_body_checks++;
    return finder.hidden_at(c.world(), threat, {pos, axis}, Vector2ui(c.bbox_size));
}

// threat, then the centers of the cells of its walking pass grid that a Dijkstra fill from its cell reaches within
// options.threat_reach px and that border a cell the fill doesn't reach.
ArrayView<const point> cover_script::reach_positions(critter& c, point threat)
{
    if (reach_known && reach_from == threat)
        return reach;
    reach_known = true;
    reach_from = threat;
    arrayClear(reach);
    arrayAppend(reach, threat);
    auto& w = c.world();
    const auto obj = w.find_object(threat_id);
    const auto R = (int)options.threat_reach;
    if (!R || !obj)
        return reach;
    fm_assert(R < (u16)-1);

    const auto bbox = Math::max((u32)obj->bbox_size.x(), (u32)obj->bbox_size.y());
    auto& pool = A.pool_registry().pass_pool(Pass::Params{astar::default_div_size, bbox, pass_mode::pass});
    pool.maybe_mark_stale_all(w);
    const auto div = (int)pool.params().div_size(), dc = (int)pool.params().div_count;
    // A cell farther than n from the threat's cell lies over R px away.
    const auto n = R/div + 1, side = 2*n + 1;
    const auto count = (u32)(side*side);
    const auto g = Vector3i(threat).xy();
    const auto cell_of = [div](int px) { return floor_div(px + half_tile<int>, div); };
    const auto origin = Vector2i{cell_of(g.x()), cell_of(g.y())} - Vector2i{n};
    const auto z = threat.chunk3().z;
    if (arrayCapacity(reach_dist) < count)
        arrayReserve(reach_dist, count + count/2);
    if (arrayCapacity(reach_open) < count)
        arrayReserve(reach_open, count + count/2);
    arrayResize(reach_dist, NoInit, count);
    arrayResize(reach_open, NoInit, count);

    Optional<Pass::Grid> grid;
    chunk_coords_ grid_ch;
    bool looked = false;
    for (auto y = 0; y < side; y++)
        for (auto x = 0; x < side; x++)
        {
            const auto cell = origin + Vector2i{x, y};
            const auto ch = Vector2i{floor_div(cell.x(), dc), floor_div(cell.y(), dc)};
            const auto cc = chunk_coords_{(i16)ch.x(), (i16)ch.y(), z};
            if (!looked || cc != grid_ch)
            {
                looked = true;
                grid_ch = cc;
                grid = {};
                if (auto* p = w.at(cc))
                {
                    grid.emplace(pool[*p]);
                    grid->build_if_stale();
                }
            }
            const auto local = Vector2ui(cell - ch*dc);
            const auto i = (u32)(y*side + x);
            reach_open[i] = grid && grid->bit(Pass::Grid::get_bitmask_index(local.x(), local.y(), (u32)dc));
            reach_dist[i] = (u16)-1;
        }

    const auto diag = (int)(div*Math::sqrt(2.) + .5);
    const auto open = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < side && y < side && reach_open[y*side + x];
    };
    arrayClear(reach_heap);
    // A bit is coarser than a point, so the threat's own cell is reached whatever its bit.
    reach_dist[n*side + n] = 0;
    arrayAppend(reach_heap, (u64)(n*side + n));
    while (!reach_heap.isEmpty())
    {
        std::pop_heap(reach_heap.begin(), reach_heap.end(), std::greater<>{});
        const auto key = reach_heap.back();
        arrayRemoveSuffix(reach_heap, 1);
        const auto d = (int)(key >> 32), i = (int)(u32)key;
        if (d > reach_dist[i])
            continue;
        const auto x = i % side, y = i / side;
        for (auto dy = -1; dy <= 1; dy++)
            for (auto dx = -1; dx <= 1; dx++)
            {
                if (!dx && !dy || !open(x + dx, y + dy))
                    continue;
                // A diagonal step needs both cells beside it, as the body would clip their corners.
                if (dx && dy && (!open(x + dx, y) || !open(x, y + dy)))
                    continue;
                const auto nd = d + (dx && dy ? diag : div);
                const auto j = (y + dy)*side + x + dx;
                if (nd > R || nd >= reach_dist[j])
                    continue;
                reach_dist[j] = (u16)nd;
                arrayAppend(reach_heap, (u64)nd << 32 | (u32)j);
                std::push_heap(reach_heap.begin(), reach_heap.end(), std::greater<>{});
            }
    }

    // A ray from inside the reached cells to a body outside them leaves through a border cell, which sees the
    // body as well, to within a cell.
    const auto reached = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < side && y < side && reach_dist[y*side + x] <= R;
    };
    for (auto y = 0; y < side; y++)
        for (auto x = 0; x < side; x++)
        {
            if (!reached(x, y))
                continue;
            bool border = false;
            for (auto dy = -1; dy <= 1; dy++)
                for (auto dx = -1; dx <= 1; dx++)
                    border |= !reached(x + dx, y + dy);
            if (border)
                arrayAppend(reach, threat + ((origin + Vector2i{x, y})*div + Vector2i{div/2 - half_tile<int>} - g));
        }
    return reach;
}

// True when no point of reach_positions() for threat sees a pixel of c's body centered on pos, flush along axis.
bool cover_script::hidden_from_reach(critter& c, point threat, point pos, Vector2i axis)
{
    for (const auto& t : reach_positions(c, threat))
        if (!hidden_at(c, t, pos, axis))
            return false;
    return true;
}

// s moved along its face as far as no point of reach_positions() for threat sees a pixel of c there.
point cover_script::cover_end(critter& c, const Cover::Point::spot& s, point threat)
{
    const Cover::Point::query q{s.pos, Vector2ui(c.bbox_size), threat, 0, 0, options.max_cover_end_dist};
    const auto from = q.max_cover_end_dist ? reach_positions(c, threat).exceptPrefix(1) : ArrayView<const point>{};
    return finder.move_to_cover_end(c.world(), q, s.axis, blocks_all_but(c.id), from);
}

// Replaces a cover the threat at threat_pos sees with cover_end() of canonical, and returns whether it did.
bool cover_script::refresh_cover(critter& c)
{
    if (cover == canonical || hidden_at(c, threat_pos, cover, axis))
        return false;
    cover = cover_end(c, {canonical, axis}, threat_pos);
    return true;
}

Vector2i cover_script::across() const { return Cover::Point::across_of(axis); }

// Puts c in state::search, which stands while it looks for cover within search_radius. False, with c
// seeking after retry_time, when the threat is gone.
bool cover_script::start_search(critter& c, search_then next)
{
    const auto threat = c.world().find_object(threat_id);
    if (!threat)
    {
        retry_later();
        return false;
    }
    st = state::search;
    then = next;
    num_failed_dijkstra_calls = 0;
    peekless = {};
    in_reach = {};
    search_retreated = false;
    arrayClear(unreachable);
    unreachable_reach = 0;
    search_pass_gen = 0;
    const auto n = (u32)search_tiles().size();
    search_tiles_left = n + n/2;
    threat_pos = threat->position();
    restart_search(c);
    return true;
}

// start_search, then the search's first step in the same update.
void cover_script::begin_search(critter& c, search_then next)
{
    if (start_search(c, next))
        search_step(c);
}

// c looks for a retreat at once, and the search for cover runs during the walk, measured from its end.
void cover_script::flee(critter& c)
{
    if (!start_search(c, search_then::seek))
        return;
    begin_retreat(c);
    retreat_step(c);
}

// Keeps unreachable and num_failed_dijkstra_calls, so a spot with no path from where c stood stays skipped as c
// walks. unreachable_reach grows by search_from's move, to hold every chunk those Dijkstra searches could cross.
void cover_script::restart_search(critter& c)
{
    if (search_tiles_left > search_tiles().size())
    {
        const auto from = st == state::retreat ? walk.dest() : c.position();
        if (!unreachable.isEmpty())
            unreachable_reach += point::distance(search_from, from);
        search_from = from;
        search_tile = 0;
    }
    search_octant = Cover::Point::nearest_octant(Vector2(threat_pos - search_from));
    tile_listed = false;
    arrayClear(tile_skipped);
}

// Tries up to max_search_tiles_per_update tiles, each with its area and the threat's current position.
// A new threat octant or changed pass grid within a failed path search's reach restarts the search.
void cover_script::search_step(critter& c)
{
    using fallback = Cover::Script::options::fallback;
    auto& w = c.world();
    const auto threat = w.find_object(threat_id);
    if (!threat)
    {
        retry_later();
        return;
    }
    const auto here = c.position();
    threat_pos = threat->position();
    // Listed spots hold only for the threat position they were listed for, and a Dijkstra search's result
    // only for the pass grids it ran in.
    const auto k = Cover::Point::nearest_octant(Vector2(threat_pos - search_from));
    if (reach_pass_gen(c) != search_pass_gen)
    {
        num_failed_dijkstra_calls = 0;
        arrayClear(unreachable);
        unreachable_reach = 0;
        search_pass_gen = 0;
        restart_search(c);
    }
    else if (k != search_octant)
        restart_search(c);
    else if (threat_pos != tile_threat)
        tile_listed = false;

    const auto& tiles = search_tiles();
    // With a spot kept for last_resort, the search ends in the update after its last spot, since that spot may
    // have used the update's one Dijkstra call.
    if (search_tile == tiles.size() || num_failed_dijkstra_calls == max_failed_dijkstra_calls)
    {
        end_search(c, take_last_resort(c));
        return;
    }
    // the offset of search_from's tile's center, so that each area is a tile of the world
    const auto base = point{search_from.coord(), {}} - search_from;
    const auto body = Vector2ui(c.bbox_size);
    const auto pred = blocks_all_but(c.id);
    auto fills = max_search_octant_fills_per_update, dijkstra_left = max_search_dijkstra_calls_per_update;
    auto peek_spot_searches_left = max_peek_spot_searches_per_update;
    const auto checks_at_start = num_body_checks;
    for (auto n = 0u; n < max_search_tiles_per_update; n++)
    {
        if (!tile_listed)
        {
            const auto mid = base + tiles[search_tile]*tile_size_xy;
            const Cover::Point::query q{search_from, body, threat_pos, search_radius, options.octants,
                                options.listed_cover_end_dist(),
                                Range2Di{mid - Vector2i{half_tile<int>}, mid + Vector2i{half_tile<int>}}, fills};
            const auto r = finder.find(w, A.pool_registry(), Grid::Cover::Params{options.div_size}, q, pred, true);
            num_find_cover_point_calls++;
            fills -= r.fills;
            if (r.out_of_fills)
                return;
            const auto count = r.spots.size();
            if (arrayCapacity(tile_spots) < count)
                arrayReserve(tile_spots, count + count/2);
            arrayClear(tile_spots);
            arrayAppend(tile_spots, r.spots);
            if (arrayCapacity(tile_skipped) < tile_skipped.size() + count)
                arrayReserve(tile_skipped, tile_skipped.size() + count);
            tile_spot = 0;
            tile_listed = true;
            tile_threat = threat_pos;
        }
        for (; tile_spot < tile_spots.size(); tile_spot++)
        {
            const auto s = tile_spots[tile_spot];
            bool skipped = false;
            for (const auto& x : tile_skipped)
                skipped |= x.pos == s.pos && x.axis == s.axis;
            if (skipped)
                continue;
            const auto at_spot = s.pos == here;
            if (!at_spot && joins_unreachable(c, s.pos))
            {
                // A tile listed again lists its unreachable spots again.
                bool known = false;
                for (const auto& u : unreachable)
                    known |= u == s.pos;
                if (!known)
                    arrayAppend(unreachable, s.pos);
                continue;
            }
            const auto num_checks = num_body_checks - checks_at_start;
            if (!peek_spot_searches_left || num_checks >= max_search_body_checks_per_update ||
                !at_spot && !dijkstra_left)
                return;
            if (options.threat_reach && !hidden_from_reach(c, threat_pos, s.pos, s.axis))
            {
                if (options.last_resort >= fallback::in_reach && !in_reach)
                    in_reach = s;
                arrayAppend(tile_skipped, s);
                continue;
            }
            peek_spot_searches_left--;
            // A path to s.pos could pass end and come back.
            const auto end = cover_end(c, s, threat_pos);
            if (options.needs_peek && !peek_spot(c, end, s.axis, threat_pos))
            {
                if (options.last_resort >= fallback::no_peek && !peekless)
                    peekless = s;
                arrayAppend(tile_skipped, s);
                continue;
            }
            if (at_spot || end == here)
            {
                end_search(c, reachable_cover{s, end, {}});
                return;
            }
            dijkstra_left--;
            // TODO: the path may lead toward the threat. Avoiding it needs A.Dijkstra predicate changes and maybe
            // a path to the threat, or another measure of how close the path comes to it.
            if (auto path = path_to(c, end); path.is_found())
            {
                end_search(c, reachable_cover{s, end, move(path)});
                return;
            }
            // c can walk straight from s.pos to end, so s.pos has no path either.
            arrayAppend(unreachable, s.pos);
            const auto reach = max_search_path_length(here, end) + point::distance(here, search_from);
            unreachable_reach = Math::max(unreachable_reach, reach);
            search_pass_gen = reach_pass_gen(c);
            if (++num_failed_dijkstra_calls == max_failed_dijkstra_calls)
            {
                if (!peekless && !in_reach)
                    end_search(c, {});
                return;
            }
        }
        tile_listed = false;
        arrayClear(tile_skipped);
        search_tile++;
        fm_debug_assert(search_tiles_left);
        search_tiles_left--;
        if (search_tile == tiles.size())
        {
            if (!peekless && !in_reach)
                end_search(c, {});
            return;
        }
    }
}

// peekless if the threat at threat_pos and where it can walk still see none of c there, else in_reach if the threat
// alone doesn't, with its cover_end() and a path there. Clears both. At most one path_to call, as in search_step.
auto cover_script::take_last_resort(critter& c) -> Optional<reachable_cover>
{
    Optional<Cover::Point::spot> spot;
    if (peekless && hidden_from_reach(c, threat_pos, peekless->pos, peekless->axis))
        spot = peekless;
    else if (in_reach && hidden_at(c, threat_pos, in_reach->pos, in_reach->axis))
        spot = in_reach;
    peekless = {};
    in_reach = {};
    if (!spot)
        return {};
    const auto s = *spot;
    const auto here = c.position();
    const auto end = cover_end(c, s, threat_pos);
    if (s.pos == here || end == here)
        return reachable_cover{s, end, {}};
    if (auto path = path_to(c, end); path.is_found())
        return reachable_cover{s, end, move(path)};
    return {};
}

// Takes r's cover, or continues the retreat, retreats or waits as `then` says when r is empty.
void cover_script::end_search(critter& c, Optional<reachable_cover> r)
{
    search_tiles_left = 0;
    const auto radius = r ? Math::min(search_radius, point::distance(search_from, r->spot.pos) + (u32)tile_size_xy)
                          : search_radius;
    query = {search_from, Vector2ui(c.bbox_size), threat_pos, radius, options.octants,
             options.listed_cover_end_dist()};
    has_query = true;
    num_searches++;
    if (!r)
    {
        if (st == state::retreat)
            return;
        if (then == search_then::seek)
            retry_later();
        else
            begin_retreat(c);
        return;
    }
    canonical = r->spot.pos;
    axis = r->spot.axis;
    cover = r->end;
    has_cover = true;
    has_peek = false;
    switch (then)
    {
    case search_then::relocate:
        walk_to_cover(c, move(r->path));
        return;
    case search_then::seek:
        walk_to_cover(c, move(r->path));
        wait = time_in(state::hidden);
        return;
    case search_then::follow:
        if (const auto p = find_peek_pos(c, threat_pos))
        {
            if (!walk_to(c, *p, state::exposed))
                begin_retreat(c);
        }
        else
        {
            walk_to_cover(c, move(r->path));
            wait = time_in(state::hidden);
        }
        return;
    }
}

// True when c's body centered on `to` can move in a straight line to a spot in unreachable no more
// than a tile away on each axis. Then path_to finds no path to `to` either, unless its max_dist
// is larger.
bool cover_script::joins_unreachable(critter& c, point to)
{
    const auto body = Vector2i(c.bbox_size);
    const auto pred = blocks_all_but(c.id);
    for (const auto& u : unreachable)
    {
        const auto d = to - u;
        if (Math::abs(d.x()) > tile_size_xy || Math::abs(d.y()) > tile_size_xy)
            continue;
        const auto a = u.intra().center_shifted(), b = a + d;
        const auto lo = Math::min(a, b) - body/2;
        const auto hi = Math::max(a, b) - body/2 + body;
        if (Search::is_passable(c.world(), u.chunk3(), Range2D{Vector2(lo), Vector2(hi)}, pred))
            return true;
    }
    return false;
}

// The newest chunk::pass_gen() among the chunks within unreachable_reach px of search_from,
// and one ring more, since a pass grid goes stale when a neighbour's stamp is newer. 0 with
// unreachable empty, so a change anywhere starts over no search that has a path to every spot.
u64 cover_script::reach_pass_gen(critter& c) const
{
    if (unreachable.isEmpty())
        return 0;
    const auto& w = c.world();
    const auto ch = search_from.chunk3();
    // search_from lies anywhere in its chunk, so the reach spans one chunk more than its length.
    const auto n = (int)(unreachable_reach / chunk_size_xy) + 2;
    u64 gen = 0;
    for (auto y = -n; y <= n; y++)
        for (auto x = -n; x <= n; x++)
            if (const auto* nb = w.at(chunk_coords_{(i16)(ch.x + x), (i16)(ch.y + y), ch.z}))
                gen = Math::max(gen, nb->pass_gen());
    return gen;
}

// A Dijkstra path from c to `to`, smoothed when string_pull can. Not found when no path within
// max_search_path_length() leads there.
path_search_result cover_script::path_to(critter& c, point to)
{
    auto& w = c.world();
    const auto from = c.position();
    const auto body = Vector2ui(c.bbox_size);
    const auto pred = blocks_all_but(c.id);
    const auto max_dist = max_search_path_length(from, to);
    num_dijkstra_calls++;
    auto path = A.Dijkstra<0, false>(w, from, to, max_dist, body, pred);
    if (path.is_found())
        (void)A.string_pull(w, path, body, pred);
    return path;
}

// Puts c in state::find_retreat, standing while it looks for a path to the nearest spot within retreat_radius
// over min_retreat_gain px farther from threat_pos than c. Spots a ray from threat_pos doesn't reach go first.
void cover_script::begin_retreat(critter& c)
{
    // finish_search() in test/cover-point.cpp expects retreat_next to go back only on a restart.
    fm_debug_assert(st != state::find_retreat);
    const auto body = Vector2i(c.bbox_size);
    const auto step = Math::max(body/2, Vector2i{1});
    const auto n = Vector2i{(int)retreat_radius} / step;
    const auto count = (u32)((2*n.x() + 1)*(2*n.y() + 1));
    if (arrayCapacity(retreat_spots) < count)
        arrayReserve(retreat_spots, count);
    arrayClear(retreat_spots);
    retreat_rays = 0;
    for (auto y = -n.y(); y <= n.y(); y++)
        for (auto x = -n.x(); x <= n.x(); x++)
            if (const auto off = Vector2i{x, y} * step; off.dot() <= (int)(retreat_radius*retreat_radius))
            {
                arrayAppend(retreat_spots, retreat_spot{off, false});
                retreat_rays += off.dot() > min_retreat_gain*min_retreat_gain;
            }
    // std::sort leaves the order of equal elements to the standard library, so the offset breaks
    // the tie and every library picks the same spot.
    std::sort(retreat_spots.begin(), retreat_spots.end(), [](const retreat_spot& a, const retreat_spot& b) {
        if (const auto da = a.offset.dot(), db = b.offset.dot(); da != db)
            return da < db;
        return a.offset.y() != b.offset.y() ? a.offset.y() < b.offset.y() : a.offset.x() < b.offset.x();
    });
    retreat_next = 0;
    num_failed_retreat_dijkstra_calls = 0;
    retreat_all = false;
    retreat_octant = Cover::Point::nearest_octant(Vector2(threat_pos - c.position()));
    st = state::find_retreat;
    // A search still running then ends without starting another retreat.
    then = search_then::seek;
    search_retreated = true;
    replans = 0;
}

// One update's work of the retreat, within the max_retreat_*_per_update limits. Each of its two passes ends
// after max_failed_dijkstra_calls spots with no path, the first also when retreat_rays runs out.
void cover_script::retreat_step(critter& c)
{
    auto& w = c.world();
    const auto threat = w.find_object(threat_id);
    if (!threat)
    {
        retry_later();
        return;
    }
    const auto from = c.position();
    threat_pos = threat->position();
    if (const auto k = Cover::Point::nearest_octant(Vector2(threat_pos - from)); k != retreat_octant)
    {
        // Spots the threat saw from its old octant may be hidden now. The pass and its failed Dijkstra calls stay,
        // so a moving threat can't cause more than 2*max_failed_dijkstra_calls of them.
        retreat_octant = k;
        retreat_next = 0;
        num_retreat_restarts++;
    }
    const auto here = Math::sqrt((double)point::distance_squared(from, threat_pos));
    const auto body = Vector2i(c.bbox_size);
    const auto pred = blocks_all_but(c.id);
    auto dijkstra_left = max_retreat_dijkstra_calls_per_update, rays_left = max_retreat_rays_per_update;
    auto passable_left = max_retreat_is_passable_calls_per_update;
    for (;;)
    {
        if (retreat_next == retreat_spots.size() ||
            num_failed_retreat_dijkstra_calls == max_failed_dijkstra_calls || !retreat_all && !retreat_rays)
        {
            if (retreat_all)
                break;
            retreat_all = true;
            retreat_next = 0;
            num_failed_retreat_dijkstra_calls = 0;
            continue;
        }
        auto& s = retreat_spots[retreat_next];
        const auto to = from + s.offset;
        if (s.done)
        {
            retreat_next++;
            continue;
        }
        if (Math::sqrt((double)point::distance_squared(to, threat_pos)) - here <= min_retreat_gain)
        {
            retreat_next++;
            continue;
        }
        // Dijkstra finds no path into a chunk that doesn't exist.
        if (!w.at(to.chunk3()))
        {
            s.done = true;
            retreat_next++;
            continue;
        }
        if (!passable_left)
            return;
        passable_left--;
        const auto lo = Vector2(to.intra().center_shifted() - body/2);
        if (!Search::is_passable(w, to.chunk3(), Range2D{lo, lo + Vector2(body)}, pred))
        {
            s.done = true;
            retreat_next++;
            continue;
        }
        if (!dijkstra_left)
            return;
        if (!retreat_all)
        {
            if (!rays_left)
                return;
            rays_left--;
            retreat_rays--;
            if (sees(c, threat_pos, to))
            {
                retreat_next++;
                continue;
            }
        }
        dijkstra_left--;
        s.done = true;
        retreat_next++;
        if (auto path = path_to(c, to); path.is_found())
        {
            walk = critter_walk{move(path)};
            st = state::retreat;
            // The search goes on measured from the walk's end.
            if (search_tiles_left)
                restart_search(c);
            return;
        }
        num_failed_retreat_dijkstra_calls++;
    }
    if (search_tiles_left)
        st = state::search;
    else
        retry_later();
}

// Sets st to `then` when c stands at `to`, else starts the walk there along path_to's path. False
// when there is no path.
bool cover_script::walk_to(critter& c, point to, state then)
{
    if (c.position() == to)
    {
        st = then;
        return true;
    }
    auto path = path_to(c, to);
    if (!path.is_found())
        return false;
    walk = critter_walk{move(path)};
    st = then == state::hidden ? state::to_cover : then == state::exposed ? state::to_peek : state::retreat;
    return true;
}

// Walks c along end_search's path to cover, or straight there from canonical, where the path is empty.
void cover_script::walk_to_cover(critter& c, path_search_result path)
{
    if (path.is_found())
        walk = critter_walk{move(path)};
    else if (c.position() != cover)
        walk = critter_walk{cover};
    else
    {
        st = state::hidden;
        return;
    }
    st = state::to_cover;
}

void cover_script::retry_later()
{
    st = state::seek;
    wait = retry_time;
}

void cover_script::seek(const bptr<critter>& c) { begin_search(*c, search_then::seek); }

// options.search_every_move's search on each move of the threat. A hidden c relocates. An exposed c
// walks to find_peek_pos's spot for the new cover, or hides at it for hidden_time without one.
void cover_script::follow_threat(critter& c)
{
    begin_search(c, st == state::hidden ? search_then::relocate : search_then::follow);
}

// c flees when `threat`, which stands elsewhere than at the last check, sees a pixel of c at canonical. If it
// sees one only at cover, c walks to a new cover_end().
void cover_script::check_hidden(critter& c, point threat)
{
    threat_pos = threat;
    if (!hidden_at(c, threat, canonical, axis))
        flee(c);
    else if (refresh_cover(c))
    {
        walk = critter_walk{cover};
        st = state::to_cover;
    }
}

// A ray from `threat`, which stands elsewhere than at the last check, to c's gun. A pixel of c at canonical
// seen makes c stop peeking and flee. A blocked gun ray sends c to a new peek spot.
void cover_script::check_exposed(critter& c, point threat)
{
    threat_pos = threat;
    if (!hidden_at(c, threat, canonical, axis))
    {
        flee(c);
        return;
    }
    if (!sees_gun(c, c.position(), threat))
        peek_again(c);
}

// The checks of a walk update in which the threat stands elsewhere than at the last check, as in check_hidden.
// A blocked ray to the gun at the peek spot sends c to a new one.
void cover_script::check_walk(critter& c)
{
    const auto threat = c.world().find_object(threat_id);
    if (!threat || threat->position() == threat_pos)
        return;
    const auto pos = threat->position();
    threat_pos = pos;
    if (!hidden_at(c, pos, canonical, axis))
    {
        flee(c);
        return;
    }
    if (st == state::to_cover && refresh_cover(c) && walk.straight())
        walk = critter_walk{cover};
    if (st == state::to_peek && !sees_gun(c, walk.dest(), pos))
        peek_again(c);
}

// True when, after a threat move, the end of c's retreat walk is no longer min_retreat_gain px farther than its
// start, or is seen when the first pass chose it. c then looks for a retreat again, once per walk, as on a block.
bool cover_script::check_retreat(critter& c)
{
    if (replans)
        return false;
    const auto threat = c.world().find_object(threat_id);
    if (!threat || threat->position() == threat_pos)
        return false;
    threat_pos = threat->position();
    fm_debug_assert(retreat_next > 0);
    const auto to = walk.dest(), from = to - retreat_spots[retreat_next - 1].offset;
    const auto dist = [&](point p) { return Math::sqrt((double)point::distance_squared(p, threat_pos)); };
    if (dist(to) - dist(from) > min_retreat_gain && (retreat_all || !sees(c, threat_pos, to)))
        return false;
    begin_retreat(c);
    num_retreat_restarts++;
    replans = 1;
    retreat_step(c);
    return true;
}

// Walks c to find_peek_pos's spot for its cover with the threat at threat_pos, or to the cover
// itself to hide there for all of hidden_time when there is none. With no path, c seeks after
// retry_time.
void cover_script::peek_again(critter& c)
{
    if (const auto p = find_peek_pos(c, threat_pos))
    {
        if (!walk_to(c, *p, state::exposed))
            retry_later();
        return;
    }
    refresh_cover(c);
    if (walk_to(c, cover, state::hidden))
        wait = time_in(state::hidden);
    else
        retry_later();
}

// With no peek spot, c hides for hidden_time more. Held exposed, a wait of time_in()'s none would
// call find_peek_pos in every update.
void cover_script::step_out(const bptr<critter>& c)
{
    if (const auto p = find_peek_pos(*c, threat_pos))
    {
        walk = critter_walk{*p};
        st = state::to_peek;
        wait = time_in(state::exposed);
    }
    else
        wait = hidden_time;
}

// Ends c's time out with a straight walk back, along the line from cover that find_peek_pos checked
// clear unless refresh_cover moves cover.
void cover_script::leave_peek(critter& c)
{
    refresh_cover(c);
    walk = critter_walk{cover};
    st = state::to_cover;
    wait = time_in(state::hidden);
}

// Walks c to cover along a Dijkstra path and hides it there for hidden_time. It serves where c may
// be off the line from its cover that find_peek_pos checked clear, so a straight walk back could
// run into the blocker. With no path, c seeks after retry_time.
void cover_script::take_cover(const bptr<critter>& c)
{
    refresh_cover(*c);
    if (walk_to(*c, cover, state::hidden))
        wait = time_in(state::hidden);
    else
        retry_later();
}

void cover_script::walk_update(const bptr<critter>& c, usize& i, const Ns& dt)
{
    const auto ret = walk.update(c, i, dt);
    if (ret == walk_result::none)
        return;
    if (ret == walk_result::arrived)
    {
        replans = 0;
        if (st == state::to_cover && c->position() != cover)
            walk = critter_walk{cover};
        else if (st == state::to_cover)
            st = state::hidden;
        else if (st == state::retreat && search_tiles_left)
        {
            st = state::search;
            face_threat(*c, i, threat_pos);
        }
        else if (st == state::retreat)
            retry_later();
        else
        {
            st = state::exposed;
            face_threat(*c, i, threat_pos);
        }
        return;
    }
    if (st == state::to_peek)
    {
        take_cover(c);
        return;
    }
    if (st == state::retreat && !replans)
    {
        // Once per retreat, so walks that keep getting blocked can't rerun find_retreat's Dijkstra searches.
        const auto threat = c->world().find_object(threat_id);
        if (!threat)
        {
            retry_later();
            return;
        }
        threat_pos = threat->position();
        begin_retreat(*c);
        num_retreat_restarts++;
        replans = 1;
        return;
    }
    if (st == state::retreat && search_tiles_left)
    {
        st = state::search;
        return;
    }
    st = state::seek;
    wait = replans++ ? retry_time : Ns{};
}

// Turns c to the one of the 8 rotations nearest the direction from its center to threat, unless
// threat is at its center or can_rotate finds its box overlapping a collider. rotate() swaps
// bbox_size's x and y between {N, NE, S, SW} and {E, SE, W, NW}, which later searches then read.
void cover_script::face_threat(critter& c, usize i, point threat)
{
    const auto v = threat - c.position();
    if (v.isZero())
        return;
    const auto r = nearest_rotation(v);
    if (r != c.r && c.can_rotate(r))
        c.rotate(i, r);
}

// The spot nearest `from` along one of 8 directions (±across, ±axis and the diagonals), within
// max_peek_dist px, where a ray from threat reaches gun_point() of the body centered there, and that
// gun point. A direction is stepped by half a body, then bisected, so a seen stretch shorter than a
// step is missed. Empty when no direction has such a spot.
Optional<Cover::Script::peek> cover_script::peek_spot(critter& c, point from, Vector2i axis, point threat)
{
    auto& w = c.world();
    const auto body = Vector2i(c.bbox_size);
    const auto across = Cover::Point::across_of(axis);
    const auto pred = blocks_all_but(c.id);
    // The spot is flush against a blocker along axis, so the last three run into it on the first
    // step and cast no ray.
    const Vector2i dirs[] = {
        across, -across, -axis, across - axis, -across - axis, axis, across + axis, axis - across,
    };

    Vector2i best;
    int best_len2 = 0;
    for (const auto dir : dirs)
    {
        const auto diagonal = dir.x() && dir.y();
        const auto step = Math::max(1, (diagonal ? Math::min(body.x(), body.y()) : Math::dot(body, Math::abs(dir)))/2);
        const auto len2 = [&](int t) { return t*t*Math::dot(dir, dir); };
        const auto seen = [&](int t) {
            return sees(c, threat, gun_point(c, from + dir*t, threat, across, gun_offset));
        };
        // A walk from `from` along dir sweeps no more than the box spanning both ends. On a
        // diagonal, that box also holds two corners the walk never reaches, so a collider there
        // rejects a clear walk.
        const auto clear = [&](int t) {
            const auto lo = from.intra().center_shifted() - body/2 + Math::min(dir*t, Vector2i{});
            const auto size = body + Math::abs(dir*t);
            return Search::is_passable(w, from.chunk3(), Range2D{Vector2(lo), Vector2(lo + size)}, pred);
        };

        int lo = 0, hi = 0;
        for (auto t = step; len2(t) <= max_peek_dist*max_peek_dist && !hi; t += step)
        {
            if (!clear(t))
            {
                // A longer walk's box holds a shorter one's, so the clear walks end before t.
                auto clear_t = lo, blocked_t = t;
                while (blocked_t - clear_t > 1)
                {
                    const auto mid = (clear_t + blocked_t)/2;
                    if (clear(mid))
                        clear_t = mid;
                    else
                        blocked_t = mid;
                }
                if (clear_t > lo && seen(clear_t))
                    hi = clear_t;
                break;
            }
            if (seen(t))
                hi = t;
            else
            {
                lo = t;
                if (best_len2 && len2(t) >= best_len2)
                    break;
            }
        }
        if (!hi)
            continue;
        while (hi - lo > 1)
        {
            const auto mid = (lo + hi)/2;
            if (seen(mid))
                hi = mid;
            else
                lo = mid;
        }
        if (!best_len2 || len2(hi) < best_len2)
        {
            best = dir*hi;
            best_len2 = len2(hi);
        }
    }
    if (!best_len2)
        return {};
    const auto pos = from + best;
    return Cover::Script::peek{pos, gun_point(c, pos, threat, across, gun_offset)};
}

// peek_spot for c's cover. peek_pos and gun_pos hold the spot and its gun point when found, and
// gun_ray_from holds threat.
Optional<point> cover_script::find_peek_pos(critter& c, point threat)
{
    const auto p = peek_spot(c, cover, axis, threat);
    has_peek = !!p;
    if (!p)
        return {};
    peek_pos = p->pos;
    gun_pos = p->gun;
    gun_ray_from = threat;
    return peek_pos;
}

Optional<Cover::Script::peek> cover_script::peek_for(critter& c, const Cover::Point::spot& s)
{
    const auto threat = c.world().find_object(threat_id);
    if (!threat)
        return {};
    reach_known = false;
    // sees() counts every ray, and the counters leave this out.
    const auto n = num_rays;
    const auto p = peek_spot(c, cover_end(c, s, threat->position()), s.axis, threat->position());
    num_rays = n;
    return p;
}

// send_to_cover, and send_to_peek with peek. The path is found before anything changes, so a spot
// with no path leaves the script as it was.
bool cover_script::send_to(critter& c, const Cover::Point::spot& s, bool peek)
{
    const auto threat = c.world().find_object(threat_id);
    reach_known = false;
    const auto end = threat ? cover_end(c, s, threat->position()) : s.pos;
    Optional<Cover::Script::peek> p;
    if (peek)
    {
        if (!threat)
            return false;
        p = peek_spot(c, end, s.axis, threat->position());
        if (!p)
            return false;
    }
    const auto to = p ? p->pos : end;
    const auto here = c.position() == to;
    path_search_result path;
    if (!here)
    {
        path = path_to(c, to);
        if (!path.is_found())
            return false;
    }
    canonical = s.pos;
    cover = end;
    axis = s.axis;
    has_cover = true;
    replans = 0;
    if (threat)
        threat_pos = threat->position();
    has_peek = !!p;
    if (p)
    {
        peek_pos = p->pos;
        gun_pos = p->gun;
        gun_ray_from = threat_pos;
    }
    using hold_phase = Cover::Script::options::hold_phase;
    if (options.hold != hold_phase::none)
        options.hold = p ? hold_phase::exposed : hold_phase::hidden;
    wait = time_in(p ? state::exposed : state::hidden);
    if (!here)
    {
        walk = critter_walk{move(path)};
        st = p ? state::to_peek : state::to_cover;
    }
    else if (p)
    {
        st = state::exposed;
        face_threat(c, c.index(), threat_pos);
    }
    else
        st = state::hidden;
    return true;
}

cover_script* cover_script_of(critter& c)
{
    if (c.script.state() != script_lifecycle::created || c.script->id() != &script_name)
        return nullptr;
    return static_cast<cover_script*>(c.script.operator->());
}

} // namespace

ScriptPtr critter_script::make_cover_script(astar& A, object_id threat, u32 div_size)
{
    return ScriptPtr(new cover_script{A, threat, div_size, {}});
}

ScriptPtr critter_script::make_cover_script(astar& A, object_id threat, u32 div_size, int gun_offset)
{
    return ScriptPtr(new cover_script{A, threat, div_size, gun_offset});
}

} // namespace floormat

namespace floormat::Cover::Script {

Optional<status> get_status(critter& c)
{
    if (auto* s = cover_script_of(c))
        return s->status();
    return {};
}

void set_options(critter& c, const options& options)
{
    if (auto* s = cover_script_of(c))
        s->set_options(c, options);
}

void list_spots(critter& c, Array<Cover::Point::spot>& out)
{
    if (const auto s = get_status(c); s && s->has_query)
        list_spots(c, s->query, out);
    else
        arrayClear(out);
}

Cover::Point::result list_spots(critter& c, const Cover::Point::query& q, Array<Cover::Point::spot>& out)
{
    if (auto* s = cover_script_of(c))
        return s->list_spots(c, q, out);
    arrayClear(out);
    return {};
}

Optional<peek> find_peek(critter& c, const Cover::Point::spot& s)
{
    if (auto* x = cover_script_of(c))
        return x->peek_for(c, s);
    return {};
}

bool send_to_cover(critter& c, const Cover::Point::spot& s)
{
    auto* x = cover_script_of(c);
    return x && x->send_to(c, s, false);
}

bool send_to_peek(critter& c, const Cover::Point::spot& s)
{
    auto* x = cover_script_of(c);
    return x && x->send_to(c, s, true);
}

} // namespace floormat::Cover::Script
