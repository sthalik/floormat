#include "critter-script-cover.hpp"
#include "critter-script.hpp"
#include "critter-walk.hpp"
#include "critter-script-cover-movement-range.hpp"
#include "critter-script-cover-peek-spot.hpp"
#include "critter-script-cover-retreat-finder.hpp"
#include "critter-script-cover-unreachable-spots.hpp"
#include "cover-point.hpp"
#include "critter.hpp"
#include "grid-registry.hpp"
#include "point.inl"
#include "intra-coord.inl"
#include "raycast.hpp"
#include "search.hpp"
#include "search-astar.hpp"
#include "search-result.hpp"
#include "tile-defs.hpp"
#include "world.hpp"
#include "entity/name-of.hpp"
#include "compat/floor-divmod.hpp"
#include "compat/function2.hpp"
#include "compat/nanosecond.inl"
#include "compat/timer.hpp"
#include <algorithm>
#include <array>
#include <cr/GrowableArray.h>
#include <cr/Optional.h>
#include <mg/Functions.h>
#include <mg/Range.h>

namespace floormat {

namespace {

constexpr Ns hidden_time = 2*Second, exposed_time = 2*Second;
// before looking for cover again when there was none, or the walk there was blocked twice
constexpr Ns retry_time = Second;
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

// A query's pos lies up to half a tile from its tile's center on each axis, so the list reaches a
// tile past search_radius.
const Array<Vector2i>& search_tiles()
{
    static const auto tiles = [] {
        Array<Vector2i> a;
        Cover::Point::nearest_tiles(search_radius + tile_size_xy, a);
        fm_assert(a.size() <= UINT16_MAX);
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
    // end is cover_end() of spot, and path leads there. path is empty when c stands on spot or end.
    struct reachable_cover { Cover::Point::spot spot; point end; path_search_result path; };
    struct skipped_spot { Cover::Point::spot spot; u32 octant; };
    // What end_search does with cover: hide there for hidden_time, for the wait left with relocate, or peek
    // from it with follow. Without cover, seek waits and the others retreat.
    enum class search_then : u8 { seek, relocate, follow, };

    void update(const bptr<critter>& c, usize& i, const Ns& dt);
    void go_on(const bptr<critter>& c, usize& i, const Ns& dt);
    bool wait_for(critter& c) const;
    Ns time_in(state phase) const;
    bool holds() const;
    bptr<object> find_threat(critter& c);
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
    void resume_search(critter& c);
    void search_step(critter& c);
    Optional<reachable_cover> take_last_resort(critter& c);
    void end_search(critter& c, Optional<reachable_cover> r);
    path_search_result path_to(critter& c, point to);
    void begin_retreat(critter& c, bool restart = false);
    void retreat_step(critter& c);
    bool walk_to(critter& c, point to, state then);
    void walk_to_cover(critter& c, path_search_result path);
    void retry_later(critter& c);
    void seek(const bptr<critter>& c);
    void follow_threat(critter& c);
    void check_hidden(critter& c, point threat);
    void check_exposed(critter& c, point threat);
    void check_walk(critter& c);
    bool check_retreat(critter& c);
    void peek_again(critter& c);
    void step_out(const bptr<critter>& c);
    void leave_peek(critter& c);
    void take_cover(critter& c);
    void walk_update(const bptr<critter>& c, usize& i, const Ns& dt);
    void face_threat(critter& c, usize i, point threat);
    Optional<Cover::Script::peek> peek_spot(critter& c, point from, Vector2i axis, point threat);
    Optional<point> find_peek_pos(critter& c, point threat);

    astar& A;
    critter_walk walk;
    Cover::Point::finder finder;
    Cover::Point::query query = {};
    unreachable_spots unreachable;
    // The spots of the search's tile search_tile, once tile_listed, of which tile_spot is next. They
    // were listed with the threat at tile_threat.
    Array<Cover::Point::spot> tile_spots;
    // The spots search_step skipped, each with its octant, in the tile that octant's scan has reached. A tile
    // listed again skips without testing those it skipped in the current octant.
    Array<skipped_spot> tile_skipped;
    point tile_threat;
    // The first spots the search skipped for having no peek spot and for threat_reach, as options.last_resort keeps.
    Optional<Cover::Point::spot> peekless, in_reach;
    movement_range threat_range;
    retreat_finder retreat;
    u32 search_tile = 0, tile_spot = 0, search_octant = 0;
    // Tiles the search may still pass, and 0 once it ended. A restart or a change of octant moves search_tile
    // back only while the rest of the scan fits, so a moving threat can't keep the search near c.
    u32 search_tiles_left = 0;
    std::array<u16, Grid::Cover::octant_count> octant_search_tiles{};
    // Where the search's tiles and the threat's octant are measured from. The last restart to tile 0 set it to c's
    // position, or during a retreat walk to the walk's end, so that cover back toward the threat comes later.
    point search_from;
    search_then then = search_then::seek;
    // A retreat started since start_search, so a kept last_resort spot starts no other.
    bool search_retreated = false;
    bool tile_listed = false;
    // canonical is the spot the search found, and cover where c hides, canonical moved along its face as far as
    // the threat alone sees no pixel of c. threat_pos is where the threat stood at the last check or search.
    point cover, canonical, threat_pos, peek_pos, gun_pos, gun_ray_from;
    Vector2i axis;
    Ns wait_until{}, prev_game_time{}, last_update{}, max_update{}, total_update{};
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
}

Cover::Script::status cover_script::status() const
{
    return {
        .cover = cover, .canonical = canonical, .axis = axis, .peek = peek_pos, .gun = gun_pos,
        .gun_ray_from = gun_ray_from, .query = query, .num_searches = num_searches, .num_rays = num_rays,
        .num_body_checks = num_body_checks, .num_dijkstra_calls = num_dijkstra_calls,
        .search_tiles_left = search_tiles_left, .search_tile = search_tile,
        .num_search_tiles = (u32)search_tiles().size(),
        .retreat_spot = retreat.next(), .num_retreat_spots = retreat.num_spots(),
        .num_failed_retreat_dijkstra_calls = retreat.num_failed_dijkstra_calls(), .retreat_all = retreat.all(),
        .num_retreat_restarts = retreat.num_restarts(), .num_find_cover_point_calls = num_find_cover_point_calls,
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
    prev_game_time = c->world().game_time();
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
    update(c, i, dt);
    // threat_pos is where the threat stood at the last check or search, and in seek c checks
    // nothing. A retreat ends in seek with c facing away from a threat that may not have moved.
    if (st == state::seek)
    {
        if (const auto threat = find_threat(*c))
            face_threat(*c, i, threat->position());
    }
    last_update = t.update();
    max_update = Math::max(max_update, last_update);
    total_update += last_update;
}

void cover_script::update(const bptr<critter>& c, usize& i, const Ns& dt)
{
    // Game time that passes while c gets no updates, as outside the chunks the editor updates, doesn't count
    // toward a wait, so wait_until moves forward by the game time since c's last update, less this update's dt.
    const auto now = c->world().game_time();
    wait_until += Math::max(now - prev_game_time, dt) - dt;
    prev_game_time = now;
    if (st != state::seek && (st != state::hidden && st != state::exposed || holds()))
        wait_until += dt;
    switch (st)
    {
    case state::seek:
        if (wait_for(*c))
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
        if (!holds() && wait_for(*c))
        {
            if (st == state::hidden)
                step_out(c);
            else
                leave_peek(*c);
            return;
        }
        if (const auto threat = find_threat(*c); !threat)
            retry_later(*c);
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
    fm_assert(false);
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

bool cover_script::wait_for(critter& c) const
{
    return c.world().game_time() >= wait_until;
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
    using fallback = Cover::Script::options::fallback;
    const auto old = options.hold;
    options = o;
    // Spots kept for a last_resort the new options don't allow, which neither the retreat nor take_last_resort
    // checks again.
    switch (o.last_resort)
    {
    case fallback::none:
        peekless = {};
        in_reach = {};
        goto done;
    case fallback::no_peek:
        in_reach = {};
        goto done;
    case fallback::in_reach:
        goto done;
    }
    fm_assert(false);
done:
    if (search_tiles_left && (st == state::search || st == state::find_retreat || st == state::retreat))
    {
        restart_search(c);
        // skipped under the old options
        arrayClear(tile_skipped);
    }
    if (o.hold == old)
        return;
    const auto hiding = st == state::hidden || st == state::to_cover;
    const auto peeking = st == state::exposed || st == state::to_peek;
    if (hiding && o.hold == hold_phase::exposed || peeking && o.hold == hold_phase::hidden)
        wait_until = c.world().game_time();
}

// The threat, or empty when it's gone or on another floor than c, as rays and cover searches stay on one floor.
bptr<object> cover_script::find_threat(critter& c)
{
    return Cover::Script::find_object_at_z(c.world(), threat_id, c.position().chunk3().z);
}

// True when a ray from `from` reaches `to` before any collider a shot can't pass. Critters don't
// stop it, c included. Every ray the script casts goes through here, so num_rays counts them all.
bool cover_script::sees(critter& c, point from, point to)
{
    num_rays++;
    auto& pool = A.pool_registry().pass_pool(Pass::Params{options.div_size, pass_mode::shoot_through}, Grid::Kind::static_);
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
    Optional<u32> bbox_side;
    if (const auto obj = c.world().find_object(threat_id))
        bbox_side = Math::max((u32)obj->bbox_size.x(), (u32)obj->bbox_size.y());
    return threat_range.border(c.world(), A.pool_registry(), threat, bbox_side, options.threat_reach);
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
    const auto threat = find_threat(c);
    if (!threat)
    {
        retry_later(c);
        return false;
    }
    st = state::search;
    then = next;
    peekless = {};
    in_reach = {};
    search_retreated = false;
    unreachable.clear();
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

// Keeps unreachable, its failed Dijkstra calls included, so a spot with no path from where c stood stays skipped
// as c walks.
void cover_script::restart_search(critter& c)
{
    // More than a scan stays left after the restart's tile, so the count stays nonzero until the search ends.
    if (search_tiles_left > search_tiles().size() + 1)
    {
        const auto from = st == state::retreat ? walk.dest() : c.position();
        unreachable.origin_moved(point::distance(search_from, from));
        search_from = from;
        search_tile = 0;
        search_tiles_left--;
        arrayClear(tile_skipped);
        octant_search_tiles = {};
    }
    search_octant = Cover::Point::nearest_octant(Vector2(threat_pos - search_from));
    tile_listed = false;
}

// Back to the search after a retreat c didn't walk to its end, measured from where c stands rather than the
// walk's end, which restart_search set, while its budget allows a restart.
void cover_script::resume_search(critter& c)
{
    st = state::search;
    if (search_from != c.position())
        restart_search(c);
}

// Tries up to max_search_tiles_per_update tiles, each with its area and the threat's current position. A changed
// pass grid within a failed path search's reach restarts the search, and a new threat octant resumes its own scan.
void cover_script::search_step(critter& c)
{
    using fallback = Cover::Script::options::fallback;
    auto& w = c.world();
    const auto threat = find_threat(c);
    if (!threat)
    {
        retry_later(c);
        return;
    }
    const auto here = c.position();
    threat_pos = threat->position();
    // Listed spots hold only for the threat position they were listed for, and a Dijkstra search's result
    // only for the pass grids it ran in.
    const auto k = Cover::Point::nearest_octant(Vector2(threat_pos - search_from));
    const auto& tiles = search_tiles();
    if (unreachable.stale(w, search_from))
    {
        unreachable.clear();
        arrayClear(tile_skipped);
        restart_search(c);
    }
    else if (k != search_octant)
    {
        octant_search_tiles[search_octant] = (u16)search_tile;
        const auto from = st == state::retreat ? walk.dest() : c.position();
        if (from == search_from && search_tiles_left > tiles.size() - octant_search_tiles[k] + 1)
        {
            search_octant = k;
            search_tile = octant_search_tiles[k];
            search_tiles_left--;
            tile_listed = false;
        }
        else
            restart_search(c);
    }
    else if (threat_pos != tile_threat)
        tile_listed = false;

    // With a spot kept for last_resort, the search ends in the update after its last spot, since that spot may
    // have used the update's one Dijkstra call.
    if (search_tile == tiles.size() || unreachable.num_failed_dijkstra_calls() == max_failed_dijkstra_calls)
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
                                Range2Di::fromCenter(mid, Vector2i{half_tile<int>}), fills};
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
                skipped |= x.spot.pos == s.pos && x.spot.axis == s.axis && x.octant == search_octant;
            if (skipped)
                continue;
            const auto at_spot = s.pos == here;
            if (!at_spot && unreachable.joins(w, s.pos, Vector2i(c.bbox_size), pred))
            {
                unreachable.add(s.pos);
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
                arrayAppend(tile_skipped, skipped_spot{s, search_octant});
                continue;
            }
            peek_spot_searches_left--;
            // A path to s.pos could pass end and come back.
            const auto end = cover_end(c, s, threat_pos);
            if (options.needs_peek && !peek_spot(c, end, s.axis, threat_pos))
            {
                if (options.last_resort >= fallback::no_peek && !peekless)
                    peekless = s;
                arrayAppend(tile_skipped, skipped_spot{s, search_octant});
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
            const auto reach = max_search_path_length(here, end) + point::distance(here, search_from);
            unreachable.add_failed(s.pos, reach, w.pass_gen());
            if (unreachable.num_failed_dijkstra_calls() == max_failed_dijkstra_calls)
            {
                if (!peekless && !in_reach)
                    end_search(c, {});
                return;
            }
        }
        tile_listed = false;
        const auto kept = std::remove_if(tile_skipped.begin(), tile_skipped.end(),
                                         [&](const skipped_spot& x) { return x.octant == search_octant; });
        arrayRemoveSuffix(tile_skipped, (usize)(tile_skipped.end() - kept));
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
            retry_later(c);
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
        wait_until = c.world().game_time() + time_in(state::hidden);
        return;
    case search_then::follow:
        // Without a path to the peek spot, c hides at the cover it has a path to, and steps out from there.
        if (const auto p = find_peek_pos(c, threat_pos); p && walk_to(c, *p, state::exposed))
            return;
        has_peek = false;
        walk_to_cover(c, move(r->path));
        wait_until = c.world().game_time() + time_in(state::hidden);
        return;
    }
    fm_assert(false);
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
void cover_script::begin_retreat(critter& c, bool restart)
{
    // finish_search() in test/cover-point.cpp expects retreat_spot to go back only on a restart.
    fm_debug_assert(st != state::find_retreat);
    const auto octant = Cover::Point::nearest_octant(Vector2(threat_pos - c.position()));
    retreat.begin(Vector2i(c.bbox_size), octant, restart);
    st = state::find_retreat;
    // A search still running then ends without starting another retreat.
    then = search_then::seek;
    search_retreated = true;
    replans = 0;
}

void cover_script::retreat_step(critter& c)
{
    const auto threat = find_threat(c);
    if (!threat)
    {
        retry_later(c);
        return;
    }
    threat_pos = threat->position();
    const auto pred = blocks_all_but(c.id);
    auto r = retreat.step(c.world(), c.position(), threat_pos, Vector2i(c.bbox_size), pred,
                          max_failed_dijkstra_calls, [&](point to) { return sees(c, threat_pos, to); },
                          [&](point to) { return path_to(c, to); });
    if (r.path.is_found())
    {
        walk = critter_walk{move(r.path)};
        st = state::retreat;
        // The search goes on measured from the walk's end.
        if (search_tiles_left)
            restart_search(c);
        return;
    }
    if (!r.done)
        return;
    if (search_tiles_left)
        resume_search(c);
    else
        retry_later(c);
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

// Walks c along path to cover, or straight there without one.
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

void cover_script::retry_later(critter& c)
{
    st = state::seek;
    wait_until = c.world().game_time() + retry_time;
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
        walk_to_cover(c, {});
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
    const auto threat = find_threat(c);
    if (!threat || threat->position() == threat_pos)
        return;
    const auto pos = threat->position();
    threat_pos = pos;
    if (!hidden_at(c, pos, canonical, axis))
    {
        flee(c);
        return;
    }
    // A moved cover may lie off a straight walk's line, so c walks a path there, or walks straight without one.
    if (st == state::to_cover && refresh_cover(c) && walk.is_line() && !walk_to(c, cover, state::hidden))
        walk_to_cover(c, {});
    if (st == state::to_peek && !sees_gun(c, walk.dest(), pos))
        peek_again(c);
}

// True when, after a threat move, the end of c's retreat walk is no longer min_retreat_gain px farther than its
// start, or is seen when the first pass chose it. c then looks for a retreat again, once per walk, as on a block.
bool cover_script::check_retreat(critter& c)
{
    if (replans)
        return false;
    const auto threat = find_threat(c);
    if (!threat || threat->position() == threat_pos)
        return false;
    threat_pos = threat->position();
    if (retreat.keeps(walk.dest(), threat_pos, [&](point to) { return sees(c, threat_pos, to); }))
        return false;
    begin_retreat(c, true);
    replans = 1;
    retreat_step(c);
    return true;
}

// Walks c to find_peek_pos's spot for its cover with the threat at threat_pos, or to the cover
// itself to hide there for all of hidden_time when there is none. With no path, c seeks after
// retry_time.
void cover_script::peek_again(critter& c)
{
    if (const auto p = find_peek_pos(c, threat_pos); !p)
        take_cover(c);
    else if (!walk_to(c, *p, state::exposed))
        retry_later(c);
}

// With no peek spot, c hides for hidden_time more. Held exposed, a wait of time_in()'s none would
// call find_peek_pos in every update.
void cover_script::step_out(const bptr<critter>& c)
{
    if (const auto p = find_peek_pos(*c, threat_pos))
    {
        walk = critter_walk{*p};
        st = state::to_peek;
        wait_until = c->world().game_time() + time_in(state::exposed);
    }
    else
        wait_until = c->world().game_time() + hidden_time;
}

// Ends c's time out with a walk back to cover. A cover that refresh_cover moved may lie off the line that
// find_peek_pos checked clear, so c walks a path there, or straight when path_to finds none.
void cover_script::leave_peek(critter& c)
{
    wait_until = c.world().game_time() + time_in(state::hidden);
    if (!refresh_cover(c) || !walk_to(c, cover, state::hidden))
        walk_to_cover(c, {});
}

// Walks c to cover along a Dijkstra path and hides it there for hidden_time. It serves where c may
// be off the line from its cover that find_peek_pos checked clear, so a straight walk back could
// run into the blocker. With no path, c seeks after retry_time.
void cover_script::take_cover(critter& c)
{
    refresh_cover(c);
    if (walk_to(c, cover, state::hidden))
        wait_until = c.world().game_time() + time_in(state::hidden);
    else
        retry_later(c);
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
            retry_later(*c);
        else
        {
            st = state::exposed;
            face_threat(*c, i, threat_pos);
        }
        return;
    }
    if (st == state::to_peek)
    {
        take_cover(*c);
        return;
    }
    if (st == state::retreat && !replans)
    {
        // Once per retreat, so walks that keep getting blocked can't rerun find_retreat's Dijkstra searches.
        const auto threat = find_threat(*c);
        if (!threat)
        {
            retry_later(*c);
            return;
        }
        threat_pos = threat->position();
        begin_retreat(*c, true);
        replans = 1;
        return;
    }
    if (st == state::retreat && search_tiles_left)
    {
        resume_search(*c);
        return;
    }
    st = state::seek;
    wait_until = c->world().game_time() + (replans++ ? retry_time : Ns{});
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

Optional<Cover::Script::peek> cover_script::peek_spot(critter& c, point from, Vector2i axis, point threat)
{
    return floormat::peek_spot(c, from, axis, threat, gun_offset, blocks_all_but(c.id),
                               [&](point to) { return sees(c, threat, to); });
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
    const auto threat = find_threat(c);
    if (!threat)
        return {};
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
    const auto threat = find_threat(c);
    if (!threat)
        return false;
    const auto end = cover_end(c, s, threat->position());
    Optional<Cover::Script::peek> p;
    if (peek)
    {
        p = peek_spot(c, end, s.axis, threat->position());
        if (!p)
            return false;
    }
    if (!walk_to(c, p ? p->pos : end, p ? state::exposed : state::hidden))
        return false;
    canonical = s.pos;
    cover = end;
    axis = s.axis;
    has_cover = true;
    replans = 0;
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
    wait_until = c.world().game_time() + time_in(p ? state::exposed : state::hidden);
    if (st == state::exposed)
        face_threat(c, c.index(), threat_pos);
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

bptr<object> find_object_at_z(world& w, object_id id, i8 z)
{
    auto obj = w.find_object(id);
    if (obj && obj->position().chunk3().z != z)
        obj = nullptr;
    return obj;
}

} // namespace floormat::Cover::Script
