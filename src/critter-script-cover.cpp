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
constexpr int max_peek = 2*tile_size_xy;
// A spot where the body fits but no path leads, past a wall or among other critters, costs a
// Dijkstra search that runs out its whole max_dist. A retreat stops after this many of them.
constexpr uint32_t max_retreat_paths = 8;

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

// Where the gun is when c's body is centered on pos and c faces threat: offset px to the right of
// pos, square to the exact direction from pos to threat, rounded to whole px. A negative offset puts
// the gun on the left. Without offset, it is half of c's bbox_size along across, the direction
// find_peek slides in. rotation_to_vec in critter.cpp maps N to (0, -1) and E, a right turn from N,
// to (1, 0), so the right of a facing (x, y) is (-y, x). With pos at threat there is no facing, and
// the gun is at pos. face_threat turns the exposed c to within 22.5° of this facing, unless c's box
// turned that way would overlap a collider.
point gun_point(const critter& c, point pos, point threat, Vector2i across, Optional<int> offset)
{
    if (pos == threat)
        return pos;
    const auto dist = offset ? *offset : Math::dot(Vector2i(c.bbox_size), across)/2;
    const auto f = Vector2(threat - pos);
    return pos + Vector2i(Math::round(Vector2{-f.y(), f.x()} * ((float)dist / f.length())));
}

// Takes cover from a critter, then keeps stepping out of it and back. Each phase, hidden or
// exposed, lasts until its wait runs out, unless a walk is blocked or a search for cover fails.
// Only updates that start with the critter standing in its phase spend the wait. The update in
// which it starts walking after a threat that moved is one of them, so a threat that keeps moving
// can't keep the phase from ending. The phase lasts longer by the walk's later updates, and by
// none when the walk arrives in the update that starts it.
// Hidden, exposed or walking to either, the critter looks for cover again only when rays from the
// threat show that the threat sees it or its cover. check_hidden, check_exposed and check_walk
// cast them, and only in an update in which the threat stands elsewhere than at the last check or
// search, so a threat that stands still costs no checks. relocate's search takes the nearest cover
// within cover_radius, else find_retreat's spot away from the threat. With
// options.search_every_move, a standing critter instead searches on every move of the threat, in
// follow_threat, and walks cast no rays. A threat that is gone sends a standing critter to seek,
// which looks for cover again after retry_time.
// Only an exposed critter turns toward the threat. face_threat runs when a walk to a peek spot
// arrives, and in each update in which the threat moved and the critter stays exposed. A critter
// in seek keeps its rotation, even in the open.
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
    void list_spots(critter& c, Array<point>& out);

    cover_script_options options;

private:
    // What check_exposed's search found after the threat saw the cover of an exposed c: next_cover,
    // or the spot at retreat_pos. leave_peek walks c there when exposed_time runs out.
    enum class flank : uint8_t { none, cover, retreat, };
    struct retreat_spot { uint64_t threat_dist2; Vector2i offset; };

    void update(const bptr<critter>& c, size_t& i, const Ns& dt);
    void go_on(const bptr<critter>& c, size_t& i, const Ns& dt);
    bool wait_for(const Ns& dt);
    bool sees(critter& c, point from, point to);
    bool sees_gun(critter& c, point pos, point threat);
    Vector2i across() const;
    cover_point_result nearest_cover(critter& c);
    bool find_cover(critter& c);
    path_search_result path_to(critter& c, point to);
    path_search_result find_retreat(critter& c);
    bool walk_to(critter& c, point to, state then);
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
    void face_threat(critter& c, size_t i);
    Optional<point> find_peek(critter& c, point threat);

    astar& A;
    critter_walk walk;
    cover_point finder;
    cover_query query = {};
    Array<retreat_spot> retreat_spots;
    // threat_pos is where the threat stood at the last check or search. next_cover and next_axis
    // become cover and axis only when leave_peek walks c there, since until then c peeks from
    // cover, and find_peek and the ray to its gun go along that cover's face.
    point cover, next_cover, threat_pos, retreat_pos, peek_pos, gun_pos, gun_seen_from;
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

// The world is destroyed after astar, so A is gone by now.
void cover_script::on_destroy(const bptr<critter>& c, script_destroy_reason) { c->clear_auto_movement(); }

cover_script::cover_script(astar& A, object_id threat, uint32_t div_size, Optional<int> gun_offset) :
    A{A}, threat_id{threat}, div_size{div_size}, gun_offset{gun_offset}
{}

cover_script_status cover_script::status() const
{
    return {
        .cover = flanked == flank::cover ? next_cover : cover, .peek = peek_pos, .gun = gun_pos,
        .gun_seen_from = gun_seen_from, .query = query, .searches = searches, .rays = rays, .paths = paths,
        .last_update = last_update, .max_update = max_update, .total_update = total_update,
        .st = st, .has_cover = has_cover, .has_peek = has_peek, .has_query = has_query,
    };
}

void cover_script::list_spots(critter& c, Array<point>& out)
{
    arrayClear(out);
    if (!has_query)
        return;
    const auto r = finder.find_cover_point(c.world(), A.pool_registry(), Cover::Params{div_size}, query,
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
        if (wait_for(dt))
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
        face_threat(*c, i);
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

// True when a ray from `from` reaches `to` before any collider a shot can't pass. Critters don't
// stop it, c included. Every ray the script casts goes through here, so rays counts them all.
bool cover_script::sees(critter& c, point from, point to)
{
    rays++;
    auto& pool = A.pool_registry().pass_pool(Pass::Params{div_size, pass_mode::shoot_through});
    return raycast(c.world(), from, to, c.id, pool, Search::without_critters()).success;
}

// True when a ray from threat reaches gun_point() of c's body centered on pos. gun_pos and
// gun_seen_from then hold the ray's ends, for the status.
bool cover_script::sees_gun(critter& c, point pos, point threat)
{
    const auto gun = gun_point(c, pos, threat, across(), gun_offset);
    if (!sees(c, threat, gun))
        return false;
    gun_pos = gun;
    gun_seen_from = threat;
    return true;
}

// The direction along the blocker's face, the one find_peek slides in.
Vector2i cover_script::across() const { return Math::abs(Vector2i{axis.y(), axis.x()}); }

// find_cover_point's cover from the threat within cover_radius of c, with options.octants. Not
// found when the threat is gone or nothing hides c from it. Unless the threat is gone, threat_pos
// is then where it stands, and query is this search's, found or not, for list_cover_spots.
cover_point_result cover_script::nearest_cover(critter& c)
{
    auto& w = c.world();
    const auto threat = w.find_object(threat_id);
    if (!threat)
        return {};
    threat_pos = threat->position();
    query = {c.position(), Vector2ui(c.bbox_size), threat_pos, cover_radius, options.octants};
    has_query = true;
    searches++;
    return finder.find_cover_point(w, A.pool_registry(), Cover::Params{div_size}, query, blocks_all_but(c.id));
}

// True when nearest_cover finds cover, and then cover and axis hold it.
bool cover_script::find_cover(critter& c)
{
    const auto r = nearest_cover(c);
    if (!r.found)
        return false;
    cover = r.pos;
    axis = r.axis;
    has_cover = true;
    has_peek = false;
    return true;
}

// A Dijkstra path from c to `to`, smoothed when string_pull can. Not found when no path within
// twice their distance plus cover_radius leads there. A cover spot on the far side of the blocker
// c stands beside is near in a straight line, but the way around the blocker can be many times
// longer.
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

// path_to's path to the spot farthest from threat_pos among those within cover_radius of c that
// lie farther from threat_pos than c, fit c's body and have a path. The spots lie on a grid with a
// step of half the body, starting at c, and are tried farthest first. Not found when the spots run
// out, or after max_retreat_paths of them have no path.
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
        if (++failed == max_retreat_paths)
            break;
    }
    return {};
}

// True when c stands at `to`, and then st is `then`, or starts walking there along path_to's path,
// and then st is the walk to `then`: hidden for cover, exposed for a peek spot, seek for a retreat.
// False when there is no path. Either way c is no longer flanked, since it leaves the spot it was
// flanked on, or seeks when it can't.
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

// Searches from where c stands and starts the walk there: to the nearest cover within
// cover_radius when a path leads to it, else to find_retreat's spot. False when there is neither,
// and then c seeks after retry_time. The wait stays as it was, since walks don't spend it.
bool cover_script::relocate(critter& c)
{
    if (find_cover(c) && walk_to(c, cover, state::hidden))
        return true;
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
    wait = find_cover(*c) && walk_to(*c, cover, state::hidden) ? hidden_time : retry_time;
}

// options.search_every_move's search on each move of the threat. A hidden c relocates. An exposed
// c looks for cover again from where it stands and walks to find_peek's spot for it, or to the new
// cover to hide there for all of hidden_time when there is no such spot. It keeps what is left of
// the wait, which the walk doesn't spend, and stays put when it already stands there. With no
// cover or no path to it, an exposed c retreats as relocate does.
void cover_script::follow_threat(critter& c)
{
    if (st == state::hidden)
    {
        (void)relocate(c);
        return;
    }
    if (find_cover(c))
    {
        if (const auto p = find_peek(c, threat_pos))
        {
            if (walk_to(c, *p, state::exposed))
                return;
        }
        else if (walk_to(c, cover, state::hidden))
        {
            wait = hidden_time;
            return;
        }
    }
    if (!start_retreat(c))
        retry_later();
}

// The threat stands at `threat`, elsewhere than at the last check. c relocates when a ray from
// there reaches its body's center, and otherwise stays hidden. One ray keeps the cost per move
// low, and a threat that sees only an edge of the body leaves c where it is.
void cover_script::check_hidden(critter& c, point threat)
{
    threat_pos = threat;
    if (sees(c, threat, c.position()))
        (void)relocate(c);
}

// The threat stands at `threat`, elsewhere than at the last check. The first ray goes from there
// to the center of the cover c is to hide at when exposed_time runs out: next_cover when flanked
// is flank::cover, else the cover c peeks from. When the ray reaches it, c searches from where it
// stands for the nearest cover within cover_radius that a path leads to, else for find_retreat's
// spot. c then stays exposed until exposed_time runs out, and leave_peek walks it to what the
// search found. When the search finds neither, c seeks after retry_time and is no longer exposed.
// When the ray is blocked and flanked is flank::retreat, the threat no longer sees the cover c
// peeks from, so flanked becomes flank::none, and c walks back to that cover when exposed_time
// runs out. With flanked flank::none, a second ray goes to c's gun, and when that ray is blocked,
// c walks to find_peek's new spot for its cover without searching. With flank::cover, c casts no
// ray to its gun, and stays where it is until exposed_time runs out.
void cover_script::check_exposed(critter& c, point threat)
{
    threat_pos = threat;
    if (sees(c, threat, flanked == flank::cover ? next_cover : cover))
    {
        // walk_to needs no path to the spot c stands on.
        if (const auto r = nearest_cover(c); r.found && (r.pos == c.position() || path_to(c, r.pos).is_found()))
        {
            next_cover = r.pos;
            next_axis = r.axis;
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

// The checks of a walk update in which the threat stands elsewhere than at the last check. A walk
// to cover or to a peek spot stops when a ray from the threat reaches the cover's center, and c
// relocates at once. When that walk was to a peek spot and the search finds cover, c hides there
// for all of hidden_time. Otherwise, when the ray to the gun at the peek spot is blocked, c walks
// to find_peek's new spot, as an exposed c would. A threat that is gone leaves the walk as it is.
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
            wait = hidden_time;
        return;
    }
    if (st == state::to_peek && !sees_gun(c, walk.dest(), pos))
        peek_again(c);
}

// Walks c to find_peek's spot for its cover with the threat at threat_pos, or to the cover itself
// to hide there for all of hidden_time when there is none. With no path, c seeks after retry_time.
void cover_script::peek_again(critter& c)
{
    if (const auto p = find_peek(c, threat_pos))
    {
        if (!walk_to(c, *p, state::exposed))
            retry_later();
    }
    else if (walk_to(c, cover, state::hidden))
        wait = hidden_time;
    else
        retry_later();
}

void cover_script::step_out(const bptr<critter>& c)
{
    if (const auto p = find_peek(*c, threat_pos))
    {
        walk = critter_walk{*p};
        st = state::to_peek;
        wait = exposed_time;
    }
    else
        wait = hidden_time;
}

// Ends c's time out. Unflanked, c stands on the slide from cover that find_peek checked, and walks
// straight back. Flanked, c walks along a Dijkstra path to what check_exposed's search found.
void cover_script::leave_peek(const bptr<critter>& c)
{
    switch (flanked)
    {
    case flank::none:
        walk = critter_walk{cover};
        st = state::to_cover;
        wait = hidden_time;
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
// be off the slide from cover that find_peek checked, so a straight walk back could run into the
// blocker. With no path, c seeks after retry_time.
void cover_script::take_cover(const bptr<critter>& c)
{
    if (walk_to(*c, cover, state::hidden))
        wait = hidden_time;
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
            face_threat(*c, i);
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

// Turns c to the one of the 8 rotations nearest the direction from c's center to threat_pos. c
// keeps its rotation when threat_pos is its center, which gives no direction, or when can_rotate
// finds its box turned that way overlapping a collider. rotate() keeps bbox_offset at zero, so c's
// position stays its body's center, which find_cover_point and Dijkstra take it to be. It swaps
// bbox_size's x and y when c turns between N, NE, S or SW and E, SE, W or NW. find_cover, walk_to,
// find_peek and gun_point read bbox_size when they run, so after this turn they take a non-square
// body as it faces the threat, as after a walk they take it as the walk left it.
void cover_script::face_threat(critter& c, size_t i)
{
    const auto v = threat_pos - c.position();
    if (v.isZero())
        return;
    const auto r = nearest_rotation(v);
    if (r != c.r && c.can_rotate(r))
        c.rotate(i, r);
}

// The spot nearest the cover, sliding along the blocker's face, where a ray from the threat reaches
// gun_point() of the body centered there. Searched in steps of half the body, since a blocker the
// slide could pass is at least that wide, then bisected. The bisection takes the cover itself as
// unseen, so the slide is at least 1 px. When a step's slide runs into a collider, the last slide
// tried on that side is the longest clear one, also found by bisection. Empty when neither side
// has one within max_peek px. peek_pos and gun_pos hold the spot and its gun point when found, and
// gun_seen_from holds threat.
Optional<point> cover_script::find_peek(critter& c, point threat)
{
    auto& w = c.world();
    const auto body = Vector2i(c.bbox_size);
    const auto dir = across();
    const auto step = Math::max(1, Math::dot(body, dir)/2);
    const auto pred = blocks_all_but(c.id);
    const auto seen = [&](int t) {
        return sees(c, threat, gun_point(c, cover + dir*t, threat, dir, gun_offset));
    };
    // The slide sweeps the box spanning both ends.
    const auto clear = [&](int t) {
        const auto lo = cover.intra().center_shifted() - body/2 + Math::min(dir*t, Vector2i{});
        const auto size = body + Math::abs(dir*t);
        return Search::is_passable(w, cover.chunk3(), Range2D{Vector2(lo), Vector2(lo + size)}, pred);
    };

    Optional<int> best;
    for (const auto side : { 1, -1 })
    {
        int lo = 0, hi = 0;
        for (auto t = step; t <= max_peek && !hi; t += step)
        {
            if (!clear(side*t))
            {
                // A longer slide's box holds a shorter one's, so the clear slides end before t.
                auto clear_t = lo, blocked_t = t;
                while (blocked_t - clear_t > 1)
                {
                    const auto mid = (clear_t + blocked_t)/2;
                    if (clear(side*mid))
                        clear_t = mid;
                    else
                        blocked_t = mid;
                }
                if (clear_t > lo && seen(side*clear_t))
                    hi = clear_t;
                break;
            }
            if (seen(side*t))
                hi = t;
            else
                lo = t;
        }
        if (!hi)
            continue;
        while (hi - lo > 1)
        {
            const auto mid = (lo + hi)/2;
            if (seen(side*mid))
                hi = mid;
            else
                lo = mid;
        }
        if (!best || hi < Math::abs(*best))
            best = side*hi;
    }
    has_peek = !!best;
    if (!best)
        return {};
    peek_pos = cover + dir * *best;
    gun_pos = gun_point(c, peek_pos, threat, dir, gun_offset);
    gun_seen_from = threat;
    return peek_pos;
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
        s->options = options;
}

void list_cover_spots(critter& c, Array<point>& out)
{
    if (auto* s = cover_script_of(c))
        s->list_spots(c, out);
    else
        arrayClear(out);
}

} // namespace floormat
