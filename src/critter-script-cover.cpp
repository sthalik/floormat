#include "critter-script.hpp"
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
#include "world.hpp"
#include "entity/name-of.hpp"
#include "compat/function2.hpp"
#include <cr/Optional.h>
#include <mg/Functions.h>
#include <mg/Range.h>

namespace floormat {

namespace {

constexpr Ns hidden_time = 2*Second, exposed_time = 2*Second;
// before looking for cover again when there was none, or the walk there was blocked twice
constexpr Ns retry_time = Second;
constexpr uint32_t cover_radius = 3*(uint32_t)tile_size_xy;
constexpr int max_peek = 2*tile_size_xy;

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
// When the critter stands with some wait left and the threat moved or is gone, it looks for cover
// again from where it stands. A hidden critter then walks to the new cover, and an exposed one to
// find_peek's spot for it. An exposed critter with no such spot walks to the new cover and hides
// there for all of hidden_time. When that search finds no cover or no path, or the threat is gone,
// the critter stands where it is and looks for cover again after retry_time.
// Only an exposed critter turns toward the threat. face_threat runs when a walk to a peek spot
// arrives, and when the threat moves and find_peek's new spot is where the critter stands. A
// critter in seek keeps its rotation, even in the open.
struct cover_script final : critter_script
{
    enum class state : uint8_t { seek, to_cover, hidden, to_peek, exposed, };

    StringView name() const override;
    const void* id() const override;
    void on_init(const bptr<critter>& c) override;
    void on_update(const bptr<critter>& c, size_t& i, const Ns& dt) override;
    void on_destroy(const bptr<critter>& c, script_destroy_reason reason) override;
    void delete_self() noexcept override;

    cover_script(astar& A, object_id threat, uint32_t div_size, Optional<int> gun_offset);

private:
    bool wait_for(const Ns& dt);
    bool find_cover(critter& c);
    bool walk_to(critter& c, point to, state then);
    void seek(const bptr<critter>& c);
    void follow_threat(const bptr<critter>& c);
    void step_out(const bptr<critter>& c);
    void take_cover(const bptr<critter>& c);
    void walk_update(const bptr<critter>& c, size_t& i, const Ns& dt);
    void face_threat(critter& c, size_t i);
    Optional<point> find_peek(critter& c, point threat);

    astar& A;
    critter_walk walk;
    cover_point finder;
    point cover, threat_pos;
    Vector2i axis;
    Ns wait{};
    object_id threat_id;
    uint32_t div_size;
    Optional<int> gun_offset;
    state st = state::seek;
    uint8_t replans = 0;
};

StringView cover_script::name() const { return "cover"_s; }
const void* cover_script::id() const { return &script_name; }
void cover_script::delete_self() noexcept { delete this; }

// The world is destroyed after astar, so A is gone by now.
void cover_script::on_destroy(const bptr<critter>& c, script_destroy_reason) { c->clear_auto_movement(); }

cover_script::cover_script(astar& A, object_id threat, uint32_t div_size, Optional<int> gun_offset) :
    A{A}, threat_id{threat}, div_size{div_size}, gun_offset{gun_offset}
{}

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
    switch (st)
    {
    case state::seek:
        if (wait_for(dt))
            seek(c);
        return;
    case state::to_cover:
    case state::to_peek:
        walk_update(c, i, dt);
        return;
    case state::hidden:
    case state::exposed:
        if (wait_for(dt))
        {
            if (st == state::hidden)
                step_out(c);
            else
            {
                walk = critter_walk{cover};
                st = state::to_cover;
                wait = hidden_time;
            }
        }
        else if (const auto threat = c->world().find_object(threat_id); !threat || threat->position() != threat_pos)
        {
            // A threat that moved during a walk is followed here. This update started with c
            // standing, so its dt was spent on the wait above, and the walk follow_threat starts
            // moves by the same dt. follow_threat leaves st at exposed only when c already stands
            // on the new peek spot. walk_update calls face_threat when a walk to one arrives.
            follow_threat(c);
            if (st == state::to_cover || st == state::to_peek)
                walk_update(c, i, dt);
            else if (st == state::exposed)
                face_threat(*c, i);
        }
        return;
    }
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

// True when there is cover from the threat within cover_radius of c, and then cover, axis and
// threat_pos hold it. False when the threat is gone or nothing hides c from it.
bool cover_script::find_cover(critter& c)
{
    auto& w = c.world();
    const auto threat = w.find_object(threat_id);
    if (!threat)
        return false;
    const auto r = finder.find_cover_point(w, A.pool_registry(), Cover::Params{div_size},
                                           {c.position(), Vector2ui(c.bbox_size), threat->position(), cover_radius},
                                           blocks_all_but(c.id));
    if (!r.found)
        return false;
    cover = r.pos;
    axis = r.axis;
    threat_pos = threat->position();
    return true;
}

// True when c stands at `to`, and then st is `then`, or starts walking there along a Dijkstra path,
// smoothed when string_pull can, and then st is the walk to `then`. False when there is no path.
bool cover_script::walk_to(critter& c, point to, state then)
{
    auto& w = c.world();
    const auto from = c.position();
    if (from == to)
    {
        st = then;
        return true;
    }
    const auto body = Vector2ui(c.bbox_size);
    const auto pred = blocks_all_but(c.id);
    const auto max_dist = point::distance(from, to)*2 + (uint32_t)tile_size_xy;
    auto path = A.Dijkstra<0, false>(w, from, to, max_dist, body, pred);
    if (!path.is_found())
        return false;
    (void)A.string_pull(w, path, body, pred);
    walk = critter_walk{move(path)};
    st = then == state::hidden ? state::to_cover : state::to_peek;
    return true;
}

void cover_script::seek(const bptr<critter>& c)
{
    wait = find_cover(*c) && walk_to(*c, cover, state::hidden) ? hidden_time : retry_time;
}

// Looks for cover again from where c stands, then walks to the spot for c's phase: the new cover,
// or find_peek's spot for it when c is exposed. c keeps what is left of the wait, which the walk
// doesn't spend, and stays put when it already stands there. An exposed c with no such spot walks
// to the new cover and hides there for all of hidden_time. With the threat gone, no cover or no
// path, st is seek and wait is retry_time.
void cover_script::follow_threat(const bptr<critter>& c)
{
    const auto exposed = st == state::exposed;
    if (find_cover(*c))
    {
        const auto peek = exposed ? find_peek(*c, threat_pos) : Optional<point>{};
        if (peek ? walk_to(*c, *peek, state::exposed) : walk_to(*c, cover, state::hidden))
        {
            if (exposed && !peek)
                wait = hidden_time;
            return;
        }
    }
    st = state::seek;
    wait = retry_time;
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

// Walks c to cover along a Dijkstra path and hides it there for hidden_time. It serves where c may
// be off the slide from cover that find_peek checked, so a straight walk back could run into the
// blocker. With no path, st is seek and wait is retry_time.
void cover_script::take_cover(const bptr<critter>& c)
{
    if (walk_to(*c, cover, state::hidden))
        wait = hidden_time;
    else
    {
        st = state::seek;
        wait = retry_time;
    }
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
// has one within max_peek px.
Optional<point> cover_script::find_peek(critter& c, point threat)
{
    auto& w = c.world();
    auto& ray_pool = A.pool_registry().pass_pool(Pass::Params{div_size, pass_mode::shoot_through});
    const auto body = Vector2i(c.bbox_size);
    const auto across = Math::abs(Vector2i{axis.y(), axis.x()});
    const auto step = Math::max(1, Math::dot(body, across)/2);
    const auto pred = blocks_all_but(c.id);
    const auto seen = [&](int t) {
        const auto gun = gun_point(c, cover + across*t, threat, across, gun_offset);
        return raycast(w, threat, gun, c.id, ray_pool, Search::without_critters()).success;
    };
    // The slide sweeps the box spanning both ends.
    const auto clear = [&](int t) {
        const auto lo = cover.intra().center_shifted() - body/2 + Math::min(across*t, Vector2i{});
        const auto size = body + Math::abs(across*t);
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
    if (!best)
        return {};
    return cover + across * *best;
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

} // namespace floormat
