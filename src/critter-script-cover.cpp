#include "critter-script.hpp"
#include "critter-script-cover.hpp"
#include "critter-walk.hpp"
#include "cover-point.hpp"
#include "critter.hpp"
#include "grid-pool-registry.hpp"
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
constexpr uint32_t cover_radius = 5*(uint32_t)tile_size_xy;
constexpr int max_peek_dist = 2*tile_size_xy;
// A spot with no path costs a Dijkstra search that runs out its whole max_dist, so a search for
// cover or a retreat stops after this many of them.
constexpr uint32_t max_failed_paths = 8;

struct cover_script;
constexpr StringView script_name = name_of<cover_script>;
using ScriptPtr = Pointer<critter_script>;
using walk_result = critter::walk_result;

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

// The direction along the face of the blocker a cover spot with this axis is flush against.
Vector2i across_of(Vector2i axis) { return Math::abs(Vector2i{axis.y(), axis.x()}); }

// Takes cover from a critter and steps out of it and back, each phase lasting until its wait runs
// out. A search for cover starts only when rays from the threat, cast after it moves, show that it
// sees the critter or its cover. Only updates that start with the critter standing spend the wait.
struct cover_script final : critter_script
{
    using state = cover_script_status::state;

    StringView name() const override;
    const void* id() const override;
    void on_init(const bptr<critter>& c) override;
    void on_update(const bptr<critter>& c, size_t& i, const Ns& dt) override;
    void on_destroy(const bptr<critter>& c, script_destroy_reason reason) override;
    void delete_self() noexcept override;

    cover_script(astar& A, object_id threat, uint32_t div_size, Optional<int> gun_offset);

    cover_script_status status() const;
    void list_spots(critter& c, const cover_query& q, Array<cover_spot>& out);
    Optional<cover_peek> peek_for(critter& c, const cover_spot& s);
    bool send_to(critter& c, const cover_spot& s, bool peek);

    cover_script_options options;
    void set_options(const cover_script_options& o);

private:
    // What check_exposed's search found after the threat saw the cover of an exposed c: next_cover,
    // or the spot at retreat_pos. leave_peek walks c there when exposed_time runs out.
    enum class flank : uint8_t { none, cover, retreat, };
    struct retreat_spot { uint64_t threat_dist2; Vector2i offset; };
    // path is empty when c stands on spot.
    struct reachable_cover { cover_spot spot; path_search_result path; };

    void update(const bptr<critter>& c, size_t& i, const Ns& dt);
    void go_on(const bptr<critter>& c, size_t& i, const Ns& dt);
    bool wait_for(const Ns& dt);
    Ns time_in(state phase) const;
    bool holds() const;
    bool sees(critter& c, point from, point to);
    bool sees_gun(critter& c, point pos, point threat);
    Vector2i across() const;
    cover_point_result nearest_cover(critter& c);
    Optional<reachable_cover> nearest_reachable_cover(critter& c);
    bool joins_unreachable(critter& c, point to);
    Optional<path_search_result> find_cover(critter& c);
    path_search_result path_to(critter& c, point to);
    path_search_result find_retreat(critter& c);
    bool walk_to(critter& c, point to, state then);
    void walk_to_cover(path_search_result path);
    bool start_retreat(critter& c);
    bool relocate(critter& c);
    void retry_later();
    void seek(const bptr<critter>& c);
    void follow_threat(critter& c);
    void check_hidden(critter& c, point threat);
    void check_exposed(critter& c, point threat);
    void check_walk(critter& c);
    void peek_again(critter& c);
    void step_out(const bptr<critter>& c);
    void leave_peek(const bptr<critter>& c);
    void take_cover(const bptr<critter>& c);
    void walk_update(const bptr<critter>& c, size_t& i, const Ns& dt);
    void face_threat(critter& c, size_t i, point threat);
    Optional<cover_peek> peek_spot(critter& c, point from, Vector2i axis, point threat);
    Optional<point> find_peek_pos(critter& c, point threat);

    astar& A;
    critter_walk walk;
    cover_point finder;
    cover_query query = {};
    Array<retreat_spot> retreat_spots;
    Array<point> unreachable;
    // threat_pos is where the threat stood at the last check or search. next_cover and next_axis
    // replace cover and axis only in leave_peek, as c peeks from cover until then.
    point cover, next_cover, threat_pos, retreat_pos, peek_pos, gun_pos, gun_ray_from;
    Vector2i axis, next_axis;
    Ns wait{}, last_update{}, max_update{}, total_update{};
    object_id threat_id;
    uint32_t div_size;
    uint32_t searches = 0, rays = 0, paths = 0;
    Optional<int> gun_offset;
    state st = state::seek;
    flank flanked = flank::none;
    uint8_t replans = 0;
    bool has_cover = false, has_peek = false, has_query = false;
};

StringView cover_script::name() const { return "cover"_s; }
const void* cover_script::id() const { return &script_name; }
void cover_script::delete_self() noexcept { delete this; }

// A may be destroyed already, at world teardown.
void cover_script::on_destroy(const bptr<critter>& c, script_destroy_reason) { c->clear_auto_movement(); }

cover_script::cover_script(astar& A, object_id threat, uint32_t div_size, Optional<int> gun_offset) :
    A{A}, threat_id{threat}, div_size{div_size}, gun_offset{gun_offset}
{}

cover_script_status cover_script::status() const
{
    return {
        .cover = flanked == flank::cover ? next_cover : cover, .peek = peek_pos, .gun = gun_pos,
        .gun_ray_from = gun_ray_from, .query = query, .searches = searches, .rays = rays, .paths = paths,
        .last_update = last_update, .max_update = max_update, .total_update = total_update,
        .st = st, .has_cover = has_cover, .has_peek = has_peek, .has_query = has_query,
    };
}

void cover_script::list_spots(critter& c, const cover_query& q, Array<cover_spot>& out)
{
    arrayClear(out);
    const auto r = finder.find_cover_point(c.world(), A.pool_registry(), Cover::Params{div_size}, q,
                                           blocks_all_but(c.id), true);
    const auto n = r.spots.size();
    if (arrayCapacity(out) < n)
        arrayReserve(out, n + n/2);
    arrayAppend(out, r.spots);
}

void cover_script::on_init(const bptr<critter>& c)
{
    // find_cover_point and Dijkstra take the body's center as the critter's position.
    fm_assert(c->bbox_offset == Vector2b{});
    c->moves.AUTO = true;
}

void cover_script::on_update(const bptr<critter>& c, size_t& i, const Ns& dt)
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
        if (const auto threat = c->world().find_object(threat_id))
            face_threat(*c, i, threat->position());
    }
    last_update = t.update();
    max_update = std::max(max_update, last_update);
    total_update += last_update;
}

void cover_script::update(const bptr<critter>& c, size_t& i, const Ns& dt)
{
    switch (st)
    {
    case state::seek:
        if (wait_for(dt))
            seek(c);
        return;
    case state::to_cover:
    case state::to_peek:
        if (!options.search_every_move)
            check_walk(*c);
        go_on(c, i, dt);
        return;
    case state::retreat:
        walk_update(c, i, dt);
        return;
    case state::hidden:
    case state::exposed:
        if (!holds() && wait_for(dt))
        {
            if (st == state::hidden)
                step_out(c);
            else
                leave_peek(c);
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
void cover_script::go_on(const bptr<critter>& c, size_t& i, const Ns& dt)
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
    using hold_phase = cover_script_options::hold_phase;
    if (phase == state::hidden)
        return options.hold == hold_phase::exposed ? Ns{} : hidden_time;
    return options.hold == hold_phase::hidden ? Ns{} : exposed_time;
}

// True when options.hold keeps c in the phase it stands in, so its wait isn't spent. A flanked c
// spends it, since the threat sees the cover c peeks from.
bool cover_script::holds() const
{
    using hold_phase = cover_script_options::hold_phase;
    if (flanked != flank::none)
        return false;
    return options.hold == (st == state::hidden ? hold_phase::hidden : hold_phase::exposed);
}

void cover_script::set_options(const cover_script_options& o)
{
    using hold_phase = cover_script_options::hold_phase;
    const auto old = options.hold;
    options = o;
    if (o.hold == old)
        return;
    const auto hiding = st == state::hidden || st == state::to_cover;
    const auto peeking = st == state::exposed || st == state::to_peek;
    if (hiding && o.hold == hold_phase::exposed || peeking && o.hold == hold_phase::hidden)
        wait = Ns{};
}

// True when a ray from `from` reaches `to` before any collider a shot can't pass. Critters don't
// stop it, c included. Every ray the script casts goes through here, so rays counts them all.
bool cover_script::sees(critter& c, point from, point to)
{
    rays++;
    auto& pool = A.pool_registry().pass_pool(Pass::Params{div_size, pass_mode::shoot_through});
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

Vector2i cover_script::across() const { return across_of(axis); }

// find_cover_point's cover from the threat within cover_radius of c, with options.octants and
// options.max_cover_end_dist. Unless the threat is gone, sets threat_pos and query, found or not.
cover_point_result cover_script::nearest_cover(critter& c)
{
    auto& w = c.world();
    const auto threat = w.find_object(threat_id);
    if (!threat)
        return {};
    threat_pos = threat->position();
    query = {c.position(), Vector2ui(c.bbox_size), threat_pos, cover_radius, options.octants, options.max_cover_end_dist};
    has_query = true;
    searches++;
    return finder.find_cover_point(w, A.pool_registry(), Cover::Params{div_size}, query, blocks_all_but(c.id));
}

// The first of nearest_cover's spots, in the order tried, that c stands on or that path_to finds a
// path to, with that path. The search that lists all spots runs only when the first has no path,
// as it can't stop at the first. Spots that joins_unreachable are skipped.
Optional<cover_script::reachable_cover> cover_script::nearest_reachable_cover(critter& c)
{
    const auto r = nearest_cover(c);
    if (!r.found)
        return {};
    if (r.pos == c.position())
        return reachable_cover{{r.pos, r.axis}, {}};
    if (auto path = path_to(c, r.pos); path.is_found())
        return reachable_cover{{r.pos, r.axis}, move(path)};

    searches++;
    const auto list = finder.find_cover_point(c.world(), A.pool_registry(), Cover::Params{div_size}, query,
                                              blocks_all_but(c.id), true);
    if (arrayCapacity(unreachable) < list.spots.size())
        arrayReserve(unreachable, list.spots.size());
    arrayClear(unreachable);
    arrayAppend(unreachable, r.pos);
    auto failed = 1u;
    for (auto i = 1u; i < list.spots.size() && failed < max_failed_paths; i++)
    {
        const auto& s = list.spots[i];
        if (s.pos == c.position())
            return reachable_cover{s, {}};
        if (!joins_unreachable(c, s.pos))
        {
            if (auto path = path_to(c, s.pos); path.is_found())
                return reachable_cover{s, move(path)};
            failed++;
        }
        arrayAppend(unreachable, s.pos);
    }
    return {};
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

// nearest_reachable_cover's path, when it finds cover, and then cover and axis hold it.
Optional<path_search_result> cover_script::find_cover(critter& c)
{
    auto r = nearest_reachable_cover(c);
    if (!r)
        return {};
    cover = r->spot.pos;
    axis = r->spot.axis;
    has_cover = true;
    has_peek = false;
    return move(r->path);
}

// A Dijkstra path from c to `to`, smoothed when string_pull can. Not found when no path within
// twice their distance plus cover_radius leads there, as the way around a blocker can be many
// times longer than the straight line.
path_search_result cover_script::path_to(critter& c, point to)
{
    auto& w = c.world();
    const auto from = c.position();
    const auto body = Vector2ui(c.bbox_size);
    const auto pred = blocks_all_but(c.id);
    const auto max_dist = point::distance(from, to)*2 + cover_radius;
    paths++;
    auto path = A.Dijkstra<0, false>(w, from, to, max_dist, body, pred);
    if (path.is_found())
        (void)A.string_pull(w, path, body, pred);
    return path;
}

// path_to's path to the spot farthest from threat_pos among those within cover_radius of c that lie
// farther from it than c, fit c's body and have a path. Spots on a half-body grid are tried
// farthest first, and max_failed_paths of them with no path end the search.
path_search_result cover_script::find_retreat(critter& c)
{
    auto& w = c.world();
    const auto from = c.position();
    const auto body = Vector2i(c.bbox_size);
    const auto step = Math::max(body/2, Vector2i{1});
    const auto n = Vector2i{(int)cover_radius} / step;
    const auto here = point::distance_squared(from, threat_pos);
    const auto count = (uint32_t)((2*n.x() + 1)*(2*n.y() + 1));
    if (arrayCapacity(retreat_spots) < count)
        arrayReserve(retreat_spots, count);
    arrayClear(retreat_spots);
    for (auto y = -n.y(); y <= n.y(); y++)
        for (auto x = -n.x(); x <= n.x(); x++)
        {
            const auto off = Vector2i{x, y} * step;
            if (off.dot() > (int)(cover_radius*cover_radius))
                continue;
            if (const auto d = point::distance_squared(from + off, threat_pos); d > here)
                arrayAppend(retreat_spots, retreat_spot{d, off});
        }
    // std::sort leaves the order of equal elements to the standard library, so the offset breaks
    // the tie and every library picks the same spot.
    std::sort(retreat_spots.begin(), retreat_spots.end(), [](const retreat_spot& a, const retreat_spot& b) {
        if (a.threat_dist2 != b.threat_dist2)
            return a.threat_dist2 > b.threat_dist2;
        return a.offset.y() != b.offset.y() ? a.offset.y() < b.offset.y() : a.offset.x() < b.offset.x();
    });

    const auto pred = blocks_all_but(c.id);
    auto failed = 0u;
    for (const auto& s : retreat_spots)
    {
        const auto to = from + s.offset;
        // Dijkstra finds no path into a chunk that doesn't exist.
        if (!w.at(to.chunk3()))
            continue;
        const auto lo = Vector2(to.intra().center_shifted() - body/2);
        if (!Search::is_passable(w, to.chunk3(), Range2D{lo, lo + Vector2(body)}, pred))
            continue;
        if (auto path = path_to(c, to); path.is_found())
            return path;
        if (++failed == max_failed_paths)
            break;
    }
    return {};
}

// Sets st to `then` when c stands at `to`, else starts the walk there along path_to's path. False
// when there is no path. Either way c is no longer flanked.
bool cover_script::walk_to(critter& c, point to, state then)
{
    flanked = flank::none;
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

// walk_to(c, cover, state::hidden) along find_cover's path, which is empty when c stands on cover.
void cover_script::walk_to_cover(path_search_result path)
{
    flanked = flank::none;
    if (!path.is_found())
    {
        st = state::hidden;
        return;
    }
    walk = critter_walk{move(path)};
    st = state::to_cover;
}

bool cover_script::start_retreat(critter& c)
{
    auto path = find_retreat(c);
    if (!path.is_found())
        return false;
    walk = critter_walk{move(path)};
    st = state::retreat;
    flanked = flank::none;
    return true;
}

// Starts a walk from where c stands to find_cover's spot, else to find_retreat's, keeping the wait.
// False when there is neither, and then c seeks after retry_time.
bool cover_script::relocate(critter& c)
{
    if (auto path = find_cover(c))
    {
        walk_to_cover(move(*path));
        return true;
    }
    if (start_retreat(c))
        return true;
    retry_later();
    return false;
}

void cover_script::retry_later()
{
    st = state::seek;
    wait = retry_time;
    flanked = flank::none;
}

void cover_script::seek(const bptr<critter>& c)
{
    if (auto path = find_cover(*c))
    {
        walk_to_cover(move(*path));
        wait = time_in(state::hidden);
    }
    else
        wait = retry_time;
}

// options.search_every_move's search on each move of the threat. A hidden c relocates. An exposed c
// walks to find_peek_pos's spot for the new cover, or hides at it for hidden_time without one, and
// retreats as relocate does when no cover or peek path is found.
void cover_script::follow_threat(critter& c)
{
    if (st == state::hidden)
    {
        (void)relocate(c);
        return;
    }
    if (auto path = find_cover(c))
    {
        if (const auto p = find_peek_pos(c, threat_pos))
        {
            if (walk_to(c, *p, state::exposed))
                return;
        }
        else
        {
            walk_to_cover(move(*path));
            wait = time_in(state::hidden);
            return;
        }
    }
    if (!start_retreat(c))
        retry_later();
}

// c relocates when a ray from `threat`, which stands elsewhere than at the last check, reaches its
// body's center. One ray keeps the cost per move low, so a threat that sees only an edge is ignored.
void cover_script::check_hidden(critter& c, point threat)
{
    threat_pos = threat;
    if (sees(c, threat, c.position()))
        (void)relocate(c);
}

// Rays from `threat`, which stands elsewhere than at the last check: to c's cover, or next_cover
// when flanked by cover, and, unless flanked, to its gun. A seen cover starts a search, and c leaves
// for the result when exposed_time runs out. A blocked gun ray sends c to a new peek spot.
void cover_script::check_exposed(critter& c, point threat)
{
    threat_pos = threat;
    if (sees(c, threat, flanked == flank::cover ? next_cover : cover))
    {
        if (const auto r = nearest_reachable_cover(c))
        {
            next_cover = r->spot.pos;
            next_axis = r->spot.axis;
            flanked = flank::cover;
        }
        else if (auto path = find_retreat(c); path.is_found())
        {
            retreat_pos = path.path().back();
            flanked = flank::retreat;
        }
        else
            retry_later();
        return;
    }
    if (flanked == flank::retreat)
        flanked = flank::none;
    if (flanked == flank::none && !sees_gun(c, c.position(), threat))
        peek_again(c);
}

// The checks of a walk update in which the threat stands elsewhere than at the last check. A seen
// cover stops the walk and c relocates. A blocked ray to the gun at the peek spot sends c to a new one.
void cover_script::check_walk(critter& c)
{
    const auto threat = c.world().find_object(threat_id);
    if (!threat || threat->position() == threat_pos)
        return;
    const auto pos = threat->position();
    threat_pos = pos;
    if (sees(c, pos, cover))
    {
        const auto peeking = st == state::to_peek;
        if (relocate(c) && peeking && (st == state::to_cover || st == state::hidden))
            wait = time_in(state::hidden);
        return;
    }
    if (st == state::to_peek && !sees_gun(c, walk.dest(), pos))
        peek_again(c);
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
    }
    else if (walk_to(c, cover, state::hidden))
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

// Ends c's time out. Unflanked, c stands on the line from its cover that find_peek_pos checked
// clear, and walks straight back. Flanked, c walks along a Dijkstra path to what check_exposed's
// search found.
void cover_script::leave_peek(const bptr<critter>& c)
{
    switch (flanked)
    {
    case flank::none:
        walk = critter_walk{cover};
        st = state::to_cover;
        wait = time_in(state::hidden);
        return;
    case flank::cover:
        cover = next_cover;
        axis = next_axis;
        has_peek = false;
        take_cover(c);
        return;
    case flank::retreat:
        if (walk_to(*c, retreat_pos, state::seek))
            wait = retry_time;
        else
            retry_later();
        return;
    }
}

// Walks c to cover along a Dijkstra path and hides it there for hidden_time. It serves where c may
// be off the line from its cover that find_peek_pos checked clear, so a straight walk back could
// run into the blocker. With no path, c seeks after retry_time.
void cover_script::take_cover(const bptr<critter>& c)
{
    if (walk_to(*c, cover, state::hidden))
        wait = time_in(state::hidden);
    else
        retry_later();
}

void cover_script::walk_update(const bptr<critter>& c, size_t& i, const Ns& dt)
{
    const auto ret = walk.update(c, i, dt);
    if (ret == walk_result::none)
        return;
    if (ret == walk_result::arrived)
    {
        replans = 0;
        if (st == state::to_cover)
            st = state::hidden;
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
    st = state::seek;
    wait = replans++ ? retry_time : Ns{};
}

// Turns c to the one of the 8 rotations nearest the direction from its center to threat, unless
// threat is at its center or can_rotate finds its box overlapping a collider. rotate() swaps
// bbox_size's x and y between {N, NE, S, SW} and {E, SE, W, NW}, which later searches then read.
void cover_script::face_threat(critter& c, size_t i, point threat)
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
Optional<cover_peek> cover_script::peek_spot(critter& c, point from, Vector2i axis, point threat)
{
    auto& w = c.world();
    const auto body = Vector2i(c.bbox_size);
    const auto across = across_of(axis);
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
    return cover_peek{pos, gun_point(c, pos, threat, across, gun_offset)};
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

Optional<cover_peek> cover_script::peek_for(critter& c, const cover_spot& s)
{
    const auto threat = c.world().find_object(threat_id);
    if (!threat)
        return {};
    // sees() counts every ray, and the counters leave this out.
    const auto n = rays;
    const auto p = peek_spot(c, s.pos, s.axis, threat->position());
    rays = n;
    return p;
}

// send_to_cover, and send_to_peek with peek. The path is found before anything changes, so a spot
// with no path leaves the script as it was.
bool cover_script::send_to(critter& c, const cover_spot& s, bool peek)
{
    const auto threat = c.world().find_object(threat_id);
    Optional<cover_peek> p;
    if (peek)
    {
        if (!threat)
            return false;
        p = peek_spot(c, s.pos, s.axis, threat->position());
        if (!p)
            return false;
    }
    const auto to = p ? p->pos : s.pos;
    const auto here = c.position() == to;
    path_search_result path;
    if (!here)
    {
        path = path_to(c, to);
        if (!path.is_found())
            return false;
    }
    cover = s.pos;
    axis = s.axis;
    has_cover = true;
    flanked = flank::none;
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
    using hold_phase = cover_script_options::hold_phase;
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

ScriptPtr critter_script::make_cover_script(astar& A, object_id threat, uint32_t div_size)
{
    return ScriptPtr(new cover_script{A, threat, div_size, {}});
}

ScriptPtr critter_script::make_cover_script(astar& A, object_id threat, uint32_t div_size, int gun_offset)
{
    return ScriptPtr(new cover_script{A, threat, div_size, gun_offset});
}

Optional<cover_script_status> get_cover_script_status(critter& c)
{
    if (auto* s = cover_script_of(c))
        return s->status();
    return {};
}

void set_cover_script_options(critter& c, const cover_script_options& options)
{
    if (auto* s = cover_script_of(c))
        s->set_options(options);
}

void list_cover_spots(critter& c, Array<cover_spot>& out)
{
    if (const auto s = get_cover_script_status(c); s && s->has_query)
        list_cover_spots(c, s->query, out);
    else
        arrayClear(out);
}

void list_cover_spots(critter& c, const cover_query& q, Array<cover_spot>& out)
{
    if (auto* s = cover_script_of(c))
        s->list_spots(c, q, out);
    else
        arrayClear(out);
}

Optional<cover_peek> find_cover_peek(critter& c, const cover_spot& s)
{
    if (auto* x = cover_script_of(c))
        return x->peek_for(c, s);
    return {};
}

bool send_to_cover(critter& c, const cover_spot& s)
{
    auto* x = cover_script_of(c);
    return x && x->send_to(c, s, false);
}

bool send_to_peek(critter& c, const cover_spot& s)
{
    auto* x = cover_script_of(c);
    return x && x->send_to(c, s, true);
}

} // namespace floormat
