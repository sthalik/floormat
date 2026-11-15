#include "critter-script-cover.hpp"
#include "critter-script.hpp"
#include "critter-walk.hpp"
#include "critter-script-cover-movement-range.hpp"
#include "critter-script-cover-peek-spot.hpp"
#include "critter-script-cover-point.hpp"
#include "critter-script-cover-retreat-finder.hpp"
#include "critter-script-cover-search.hpp"
#include "critter-script-cover-spot-tests.hpp"
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
#include "compat/function-view.hpp"
#include "compat/nanosecond.inl"
#include "compat/timer.hpp"
#include <cr/GrowableArray.h>
#include <cr/Optional.h>
#include <mg/Functions.h>
#include <mg/Range.h>

namespace floormat {

namespace {

constexpr Ns hidden_time = 2*Second, exposed_time = 2*Second;
// before looking for cover again when there was none, or the walk there was blocked twice
constexpr Ns retry_time = Second;

struct cover_script;
constexpr StringView script_name = name_of<cover_script>;
using ScriptPtr = Pointer<critter_script>;
using walk_result = critter::walk_result;
using Search::blocks_all_but;

// Takes cover from a critter and steps out of it and back, each phase lasting until its wait runs out.
// Only updates that start with the critter standing spend the wait.
struct cover_script final : critter_script
{
    using state = Cover::Script::status::state;

    StringView name() const override;
    const void* id() const override;
    void on_init(const bptr<critter>& C) override;
    void on_update(const bptr<critter>& C, usize& i, const Ns& dt) override;
    void on_destroy(const bptr<critter>& C, script_destroy_reason reason) override;
    void delete_self() noexcept override;

    cover_script(astar& A, object_id threat, u32 div_size, Optional<int> gun_offset);

    Cover::Script::status status() const;
    Cover::Point::result list_spots(critter& C, const Cover::Point::query& q, Array<Cover::Point::spot>& out);
    Optional<Cover::Script::peek> peek_for(critter& C, const Cover::Point::spot& s);
    bool send_to(critter& C, const Cover::Point::spot& s, bool peek);

    Cover::Script::options options;
    void set_options(critter& C, const Cover::Script::options& o);

private:
    // What end_search does with cover: hide there for hidden_time, for the wait left with relocate, or peek
    // from it with follow. Without cover, seek waits and the others retreat.
    enum class search_then : u8 { seek, relocate, follow, };

    void update(const bptr<critter>& C, usize& i, const Ns& dt);
    void go_on(const bptr<critter>& C, usize& i, const Ns& dt);
    bool wait_for(critter& C) const;
    Ns time_in(state phase) const;
    bool holds() const;
    bptr<object> find_threat(critter& C);
    bool find_threat_or_retry(critter& C);
    bool sees_gun(critter& C, point pos, point threat);
    bool refresh_cover(critter& C);
    Vector2i across() const;
    bool start_search(critter& C, search_then next);
    void begin_search(critter& C, search_then next);
    void flee(critter& C);
    void restart_search(critter& C);
    void resume_search(critter& C);
    void search_step(critter& C);
    void end_search(critter& C, Optional<Cover::cover_search::reachable_cover> r);
    void begin_retreat(critter& C, bool restart = false);
    void retreat_step(critter& C);
    bool walk_to(critter& C, point to, state then);
    void walk_to_cover(critter& C, path_search_result path);
    void retry_later(critter& C);
    void seek(const bptr<critter>& C);
    void follow_threat(critter& C);
    void check_hidden(critter& C, point threat);
    void check_exposed(critter& C, point threat);
    void check_walk(critter& C);
    bool check_retreat(critter& C);
    void peek_again(critter& C);
    void step_out(const bptr<critter>& C);
    void leave_peek(critter& C);
    void take_cover(critter& C);
    void walk_update(const bptr<critter>& C, usize& i, const Ns& dt);
    void face_threat(critter& C, usize i, point threat);
    Optional<point> find_peek_pos(critter& C, point threat);

    Cover::spot_tests tests;
    critter_walk walk;
    Optional<Cover::Point::query> query;
    Cover::cover_search search;
    Cover::retreat_finder retreat;
    search_then then = search_then::seek;
    // A retreat started since start_search, so a kept last_resort spot starts no other.
    bool search_retreated = false;
    Optional<Cover::Script::cover> cover;
    // Where the threat stood at the last check or search.
    point threat_pos;
    Ns wait_until{}, prev_game_time{}, last_update{}, max_update{}, total_update{};
    state st = state::seek;
    u8 num_blocked_walks = 0;
    bool retreat_replanned = false;
};

StringView cover_script::name() const { return "cover"_s; }
const void* cover_script::id() const { return &script_name; }
void cover_script::delete_self() noexcept { delete this; }

// The astar that tests holds may be destroyed already, at world teardown.
void cover_script::on_destroy(const bptr<critter>& C, script_destroy_reason) { C->clear_auto_movement(); }

cover_script::cover_script(astar& A, object_id threat, u32 div_size, Optional<int> gun_offset) :
    tests{A, threat, gun_offset}
{
    options.div_size = div_size;
}

Cover::Script::status cover_script::status() const
{
    return {
        .cover = cover, .query = query, .counters = tests.counters, .search = search.progress(),
        .retreat = retreat.progress(), .last_update = last_update, .max_update = max_update,
        .total_update = total_update, .st = st,
    };
}

Cover::Point::result cover_script::list_spots(critter& C, const Cover::Point::query& q, Array<Cover::Point::spot>& out)
{
    arrayClear(out);
    const auto r = tests.finder().find(C.world(), tests.pool_registry(), Grid::Cover::Params{options.div_size}, q,
                                       blocks_all_but(C.id), true);
    const auto n = r.spots.size();
    if (arrayCapacity(out) < n)
        arrayReserve(out, n + n/2);
    arrayAppend(out, r.spots);
    return r;
}

void cover_script::on_init(const bptr<critter>& C)
{
    // find and Dijkstra take the body's center as the critter's position.
    fm_assert(C->bbox_offset == Vector2b{});
    C->moves.AUTO = true;
    prev_game_time = C->world().game_time();
}

void cover_script::on_update(const bptr<critter>& C, usize& i, const Ns& dt)
{
    if (C->maybe_stop_auto_movement())
    {
        C->clear_auto_movement();
        C->script.do_clear(C);
        return;
    }
    // do_clear deletes the script, so the clock starts after it.
    auto t = Time::now();
    update(C, i, dt);
    // threat_pos is where the threat stood at the last check or search, and in seek C checks
    // nothing. A retreat ends in seek with C facing away from a threat that may not have moved.
    if (st == state::seek)
    {
        if (const auto threat = find_threat(*C))
            face_threat(*C, i, threat->position());
    }
    last_update = t.update();
    max_update = Math::max(max_update, last_update);
    total_update += last_update;
}

void cover_script::update(const bptr<critter>& C, usize& i, const Ns& dt)
{
    // Game time that passes while C gets no updates, as outside the chunks the editor updates, doesn't count
    // toward a wait, so wait_until moves forward by the game time since C's last update, less this update's dt.
    const auto now = C->world().game_time();
    wait_until += Math::max(now - prev_game_time, dt) - dt;
    prev_game_time = now;
    if (st != state::seek && (st != state::hidden && st != state::exposed || holds()))
        wait_until += dt;
    switch (st)
    {
    case state::seek:
        if (wait_for(*C))
            seek(C);
        return;
    case state::search:
        search_step(*C);
        // This runs after search_step, so cover found in this update needs no retreat.
        if (st == state::search && search.has_last_resort() && !search_retreated &&
            !tests.hidden_at(*C, threat_pos, C->position(), Cover::Point::axis_toward(threat_pos - C->position())))
            begin_retreat(*C);
        go_on(C, i, dt);
        return;
    case state::to_cover:
    case state::to_peek:
        if (!options.search_every_move)
            check_walk(*C);
        go_on(C, i, dt);
        return;
    case state::find_retreat:
        retreat_step(*C);
        go_on(C, i, dt);
        return;
    case state::retreat:
        if ((options.search_every_move || !check_retreat(*C)) && search.running())
            search_step(*C);
        go_on(C, i, dt);
        return;
    case state::hidden:
    case state::exposed:
        if (!holds() && wait_for(*C))
        {
            if (st == state::hidden)
                step_out(C);
            else
                leave_peek(*C);
            return;
        }
        if (const auto threat = find_threat(*C); !threat)
            retry_later(*C);
        else if (const auto pos = threat->position(); pos != threat_pos)
        {
            // This update started with C standing, so its dt was spent on the wait above, and a
            // walk the checks start moves by the same dt.
            if (options.search_every_move)
                follow_threat(*C);
            else if (st == state::hidden)
                check_hidden(*C, pos);
            else
                check_exposed(*C, pos);
            go_on(C, i, dt);
        }
        return;
    }
    fm_assert(false);
}

// The rest of an update in which the checks or a search may have changed st: a walk moves by dt,
// and a C that stays exposed or arrived on its peek spot at once turns toward the threat.
void cover_script::go_on(const bptr<critter>& C, usize& i, const Ns& dt)
{
    if (st == state::to_cover || st == state::to_peek || st == state::retreat)
        walk_update(C, i, dt);
    else if (st == state::exposed)
        face_threat(*C, i, threat_pos);
}

bool cover_script::wait_for(critter& C) const
{
    return C.world().game_time() >= wait_until;
}

// The wait C starts phase with, hidden or exposed: hidden_time or exposed_time, or none when
// options.hold keeps C in the other phase.
Ns cover_script::time_in(state phase) const
{
    using hold_phase = Cover::Script::options::hold_phase;
    if (phase == state::hidden)
        return options.hold == hold_phase::exposed ? Ns{} : hidden_time;
    return options.hold == hold_phase::hidden ? Ns{} : exposed_time;
}

// True when options.hold keeps C in the phase it stands in, so its wait isn't spent.
bool cover_script::holds() const
{
    using hold_phase = Cover::Script::options::hold_phase;
    return options.hold == (st == state::hidden ? hold_phase::hidden : hold_phase::exposed);
}

void cover_script::set_options(critter& C, const Cover::Script::options& o)
{
    using hold_phase = Cover::Script::options::hold_phase;
    const auto old = options.hold;
    options = o;
    search.options_changed(o.last_resort);
    if (search.running() && (st == state::search || st == state::find_retreat || st == state::retreat))
        restart_search(C);
    if (o.hold == old)
        return;
    const auto hiding = st == state::hidden || st == state::to_cover;
    const auto peeking = st == state::exposed || st == state::to_peek;
    if (hiding && o.hold == hold_phase::exposed || peeking && o.hold == hold_phase::hidden)
        wait_until = C.world().game_time();
}

// The threat, or empty when it's gone or on another floor than C, as rays and cover searches stay on one floor.
bptr<object> cover_script::find_threat(critter& C)
{
    return Cover::Script::find_object_at_z(C.world(), tests.threat_id(), C.position().chunk3().z);
}

// True with threat_pos set to where the threat stands. False, with C seeking after retry_time, when it's gone.
bool cover_script::find_threat_or_retry(critter& C)
{
    const auto threat = find_threat(C);
    if (!threat)
    {
        retry_later(C);
        return false;
    }
    threat_pos = threat->position();
    return true;
}

// True when a ray from threat reaches gun_point() of C's body centered on pos. The peek of cover then holds the
// ray's ends, for the status.
bool cover_script::sees_gun(critter& C, point pos, point threat)
{
    const auto gun = Cover::gun_point(C, pos, threat, across(), tests.gun_offset());
    if (!tests.sees(C, options, threat, gun))
        return false;
    auto& c = *cover;
    auto& p = *c.peek;
    p.gun = gun;
    p.gun_ray_from = threat;
    return true;
}

// Replaces a cover the threat at threat_pos sees with cover_end() of canonical, and returns whether it did.
bool cover_script::refresh_cover(critter& C)
{
    auto& c = *cover;
    if (c.pos == c.canonical || tests.hidden_at(C, threat_pos, c.pos, c.axis))
        return false;
    c.pos = tests.cover_end(C, options, {c.canonical, c.axis}, threat_pos);
    return true;
}

Vector2i cover_script::across() const
{
    const auto& c = *cover;
    return Cover::Point::across_of(c.axis);
}

// Puts C in state::search, which stands while it looks for cover within cover_search::radius. False, with C
// seeking after retry_time, when the threat is gone.
bool cover_script::start_search(critter& C, search_then next)
{
    if (!find_threat_or_retry(C))
        return false;
    st = state::search;
    then = next;
    search_retreated = false;
    search.begin(C.position(), threat_pos);
    return true;
}

// start_search, then the search's first step in the same update.
void cover_script::begin_search(critter& C, search_then next)
{
    if (start_search(C, next))
        search_step(C);
}

// C looks for a retreat at once, and the search for cover runs during the walk, measured from its end.
void cover_script::flee(critter& C)
{
    if (!start_search(C, search_then::seek))
        return;
    begin_retreat(C);
    retreat_step(C);
}

void cover_script::restart_search(critter& C)
{
    search.restart(st == state::retreat ? walk.dest() : C.position(), threat_pos);
}

// Back to the search after a retreat C didn't walk to its end, measured from where C stands rather than the
// walk's end, which restart_search set, while its budget allows a restart.
void cover_script::resume_search(critter& C)
{
    st = state::search;
    if (search.from() != C.position())
        restart_search(C);
}

void cover_script::search_step(critter& C)
{
    if (!find_threat_or_retry(C))
        return;
    auto r = search.step(C, tests, options, st == state::retreat ? walk.dest() : C.position(), threat_pos);
    if (r.done)
        end_search(C, move(r.cover));
}

// Takes r's cover, or continues the retreat, retreats or waits as `then` says when r is empty.
void cover_script::end_search(critter& C, Optional<Cover::cover_search::reachable_cover> r)
{
    search.end();
    const auto from = search.from();
    auto radius = Cover::cover_search::radius;
    if (r)
    {
        const auto& reachable = *r;
        radius = Math::min(radius, point::distance(from, reachable.spot.pos) + (u32)tile_size_xy);
    }
    query = Cover::Point::query{from, Vector2ui(C.bbox_size), threat_pos, radius, options.octants,
                                options.listed_cover_end_dist()};
    tests.counters.num_searches++;
    if (!r)
    {
        if (st == state::retreat)
            return;
        if (then == search_then::seek)
            retry_later(C);
        else
            begin_retreat(C);
        return;
    }
    auto& reachable = *r;
    cover = Cover::Script::cover{.pos = reachable.end, .canonical = reachable.spot.pos,
                                 .axis = reachable.spot.axis, .peek = {}};
    auto& c = *cover;
    switch (then)
    {
    case search_then::relocate:
        walk_to_cover(C, move(reachable.path));
        return;
    case search_then::seek:
        walk_to_cover(C, move(reachable.path));
        wait_until = C.world().game_time() + time_in(state::hidden);
        return;
    case search_then::follow:
        // Without a path to the peek spot, C hides at the cover it has a path to, and steps out from there.
        if (const auto p = find_peek_pos(C, threat_pos); p && walk_to(C, *p, state::exposed))
            return;
        c.peek = {};
        walk_to_cover(C, move(reachable.path));
        wait_until = C.world().game_time() + time_in(state::hidden);
        return;
    }
    fm_assert(false);
}

// Puts C in state::find_retreat, standing while it looks for a path to the nearest spot within retreat_radius
// over min_retreat_gain px farther from threat_pos than C. Spots a ray from threat_pos doesn't reach go first.
void cover_script::begin_retreat(critter& C, bool restart)
{
    // finish_search() in test/cover-point.cpp expects retreat.spot to go back only on a restart.
    fm_debug_assert(st != state::find_retreat);
    const auto octant = Cover::Point::nearest_octant(Vector2(threat_pos - C.position()));
    retreat.begin(Vector2i(C.bbox_size), octant);
    tests.counters.num_retreat_restarts += restart;
    st = state::find_retreat;
    // A search still running then ends without starting another retreat.
    then = search_then::seek;
    search_retreated = true;
    retreat_replanned = false;
    num_blocked_walks = 0;
}

void cover_script::retreat_step(critter& C)
{
    if (!find_threat_or_retry(C))
        return;
    auto r = retreat.step(C, tests, options, threat_pos);
    if (r.path.is_found())
    {
        walk = critter_walk{move(r.path)};
        st = state::retreat;
        // The search goes on measured from the walk's end.
        if (search.running())
            restart_search(C);
        return;
    }
    if (!r.done)
        return;
    if (search.running())
        resume_search(C);
    else
        retry_later(C);
}

// Sets st to `then` when C stands at `to`, else starts the walk there along path_to's path. False
// when there is no path.
bool cover_script::walk_to(critter& C, point to, state then)
{
    if (C.position() == to)
    {
        st = then;
        return true;
    }
    auto path = tests.path_to(C, to);
    if (!path.is_found())
        return false;
    walk = critter_walk{move(path)};
    st = then == state::hidden ? state::to_cover : then == state::exposed ? state::to_peek : state::retreat;
    return true;
}

// Walks C along path to cover, or straight there without one.
void cover_script::walk_to_cover(critter& C, path_search_result path)
{
    if (path.is_found())
        walk = critter_walk{move(path)};
    else if (const auto& c = *cover; C.position() != c.pos)
        walk = critter_walk{c.pos};
    else
    {
        st = state::hidden;
        return;
    }
    st = state::to_cover;
}

void cover_script::retry_later(critter& C)
{
    st = state::seek;
    wait_until = C.world().game_time() + retry_time;
}

void cover_script::seek(const bptr<critter>& C) { begin_search(*C, search_then::seek); }

// options.search_every_move's search on each move of the threat. A hidden C relocates. An exposed C
// walks to find_peek_pos's spot for the new cover, or hides at it for hidden_time without one.
void cover_script::follow_threat(critter& C)
{
    begin_search(C, st == state::hidden ? search_then::relocate : search_then::follow);
}

// C flees when `threat`, which stands elsewhere than at the last check, sees a pixel of C at canonical. If it
// sees one only at cover, C walks to a new cover_end().
void cover_script::check_hidden(critter& C, point threat)
{
    threat_pos = threat;
    const auto& c = *cover;
    if (!tests.hidden_at(C, threat, c.canonical, c.axis))
        flee(C);
    else if (refresh_cover(C))
        walk_to_cover(C, {});
}

// A ray from `threat`, which stands elsewhere than at the last check, to C's gun. A pixel of C at canonical
// seen makes C stop peeking and flee. A blocked gun ray sends C to a new peek spot.
void cover_script::check_exposed(critter& C, point threat)
{
    threat_pos = threat;
    const auto& c = *cover;
    if (!tests.hidden_at(C, threat, c.canonical, c.axis))
    {
        flee(C);
        return;
    }
    if (!sees_gun(C, C.position(), threat))
        peek_again(C);
}

// The checks of a walk update in which the threat stands elsewhere than at the last check, as in check_hidden.
// A blocked ray to the gun at the peek spot sends C to a new one.
void cover_script::check_walk(critter& C)
{
    const auto threat = find_threat(C);
    if (!threat || threat->position() == threat_pos)
        return;
    const auto pos = threat->position();
    threat_pos = pos;
    const auto& c = *cover;
    if (!tests.hidden_at(C, pos, c.canonical, c.axis))
    {
        flee(C);
        return;
    }
    // A moved cover may lie off a straight walk's line, so C walks a path there, or walks straight without one.
    if (st == state::to_cover && refresh_cover(C) && walk.is_line() && !walk_to(C, c.pos, state::hidden))
        walk_to_cover(C, {});
    if (st == state::to_peek && !sees_gun(C, walk.dest(), pos))
        peek_again(C);
}

// True when, after a threat move, the end of C's retreat walk is no longer min_retreat_gain px farther than its
// start, or is seen when the first pass chose it. C then looks for a retreat again, once per walk, as on a block.
bool cover_script::check_retreat(critter& C)
{
    if (retreat_replanned)
        return false;
    const auto threat = find_threat(C);
    if (!threat || threat->position() == threat_pos)
        return false;
    threat_pos = threat->position();
    if (retreat.keeps(C, tests, options, walk.dest(), threat_pos))
        return false;
    begin_retreat(C, true);
    retreat_replanned = true;
    retreat_step(C);
    return true;
}

// Walks C to find_peek_pos's spot for its cover with the threat at threat_pos, or to the cover
// itself to hide there for all of hidden_time when there is none. With no path, C seeks after
// retry_time.
void cover_script::peek_again(critter& C)
{
    if (const auto p = find_peek_pos(C, threat_pos); !p)
        take_cover(C);
    else if (!walk_to(C, *p, state::exposed))
        retry_later(C);
}

// With no peek spot, C hides for hidden_time more. Held exposed, a wait of time_in()'s none would
// call find_peek_pos in every update.
void cover_script::step_out(const bptr<critter>& C)
{
    if (const auto p = find_peek_pos(*C, threat_pos))
    {
        walk = critter_walk{*p};
        st = state::to_peek;
        wait_until = C->world().game_time() + time_in(state::exposed);
    }
    else
        wait_until = C->world().game_time() + hidden_time;
}

// Ends C's time out with a walk back to cover. A cover that refresh_cover moved may lie off the line that
// find_peek_pos checked clear, so C walks a path there, or straight when path_to finds none.
void cover_script::leave_peek(critter& C)
{
    wait_until = C.world().game_time() + time_in(state::hidden);
    const auto& c = *cover;
    if (!refresh_cover(C) || !walk_to(C, c.pos, state::hidden))
        walk_to_cover(C, {});
}

// Walks C to cover along a Dijkstra path and hides it there for hidden_time. It serves where C may
// be off the line from its cover that find_peek_pos checked clear, so a straight walk back could
// run into the blocker. With no path, C seeks after retry_time.
void cover_script::take_cover(critter& C)
{
    refresh_cover(C);
    const auto& c = *cover;
    if (walk_to(C, c.pos, state::hidden))
        wait_until = C.world().game_time() + time_in(state::hidden);
    else
        retry_later(C);
}

void cover_script::walk_update(const bptr<critter>& C, usize& i, const Ns& dt)
{
    const auto ret = walk.update(C, i, dt);
    if (ret == walk_result::none)
        return;
    if (ret == walk_result::arrived)
    {
        retreat_replanned = false;
        num_blocked_walks = 0;
        if (st == state::to_cover)
        {
            const auto& c = *cover;
            if (C->position() != c.pos)
                walk = critter_walk{c.pos};
            else
                st = state::hidden;
        }
        else if (st == state::retreat && search.running())
        {
            st = state::search;
            face_threat(*C, i, threat_pos);
        }
        else if (st == state::retreat)
            retry_later(*C);
        else
        {
            st = state::exposed;
            face_threat(*C, i, threat_pos);
        }
        return;
    }
    if (st == state::to_peek)
    {
        take_cover(*C);
        return;
    }
    if (st == state::retreat && !retreat_replanned)
    {
        // Once per retreat, so walks that keep getting blocked can't rerun find_retreat's Dijkstra searches.
        if (!find_threat_or_retry(*C))
            return;
        begin_retreat(*C, true);
        retreat_replanned = true;
        return;
    }
    if (st == state::retreat && search.running())
    {
        resume_search(*C);
        return;
    }
    st = state::seek;
    wait_until = C->world().game_time() + (num_blocked_walks++ || retreat_replanned ? retry_time : Ns{});
}

// Turns C to the one of the 8 rotations nearest the direction from its center to threat, unless
// threat is at its center or can_rotate finds its box overlapping a collider. rotate() swaps
// bbox_size's x and y between {N, NE, S, SW} and {E, SE, W, NW}, which later searches then read.
void cover_script::face_threat(critter& C, usize i, point threat)
{
    const auto v = threat - C.position();
    if (v.isZero())
        return;
    const auto r = nearest_rotation(v);
    if (r != C.r && C.can_rotate(r))
        C.rotate(i, r);
}

// peek_spot for C's cover. The peek of cover holds the spot, its gun point and threat when found, and is empty
// otherwise.
Optional<point> cover_script::find_peek_pos(critter& C, point threat)
{
    auto& c = *cover;
    const auto p = tests.peek_spot(C, options, c.pos, c.axis, threat);
    if (!p)
    {
        c.peek = {};
        return {};
    }
    const auto& peek = *p;
    c.peek = Cover::Script::cover_peek{peek.pos, peek.gun, threat};
    return peek.pos;
}

Optional<Cover::Script::peek> cover_script::peek_for(critter& C, const Cover::Point::spot& s)
{
    const auto threat = find_threat(C);
    if (!threat)
        return {};
    // sees() counts every ray, and the counters leave this out.
    const auto pos = threat->position();
    const auto n = tests.counters.num_rays;
    const auto p = tests.peek_spot(C, options, tests.cover_end(C, options, s, pos), s.axis, pos);
    tests.counters.num_rays = n;
    return p;
}

// send_to_cover, and send_to_peek with peek. The path is found before anything changes, so a spot
// with no path leaves the script as it was.
bool cover_script::send_to(critter& C, const Cover::Point::spot& s, bool peek)
{
    const auto threat = find_threat(C);
    if (!threat)
        return false;
    const auto end = tests.cover_end(C, options, s, threat->position());
    Optional<Cover::Script::peek> pʹ;
    auto to = end;
    if (peek)
    {
        pʹ = tests.peek_spot(C, options, end, s.axis, threat->position());
        if (!pʹ)
            return false;
        const auto& p = *pʹ;
        to = p.pos;
    }
    if (!walk_to(C, to, pʹ ? state::exposed : state::hidden))
        return false;
    cover = Cover::Script::cover{.pos = end, .canonical = s.pos, .axis = s.axis, .peek = {}};
    retreat_replanned = false;
    num_blocked_walks = 0;
    threat_pos = threat->position();
    if (pʹ)
    {
        const auto& [pos, gun] = *pʹ;
        auto& c = *cover;
        c.peek = Cover::Script::cover_peek{pos, gun, threat_pos};
    }
    using hold_phase = Cover::Script::options::hold_phase;
    if (options.hold != hold_phase::none)
        options.hold = pʹ ? hold_phase::exposed : hold_phase::hidden;
    wait_until = C.world().game_time() + time_in(pʹ ? state::exposed : state::hidden);
    if (st == state::exposed)
        face_threat(C, C.index(), threat_pos);
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
    const auto s = get_status(c);
    if (!s)
    {
        arrayClear(out);
        return;
    }
    const auto& status = *s;
    if (status.query)
        list_spots(c, *status.query, out);
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

rc::raycast_result_s line_of_sight(world& w, Grid::GridRegistry& reg, u32 div_size, point from, point to,
                                   object_id self)
{
    const auto params = Pass::Params{div_size, pass_mode::shoot_through};
    return raycast(w, from, to, self, reg.object_pool(params), Search::without_critters);
}

} // namespace floormat::Cover::Script
