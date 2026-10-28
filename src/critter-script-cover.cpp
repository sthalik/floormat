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

// Takes cover from a critter, then keeps stepping out of it and back.
struct cover_script final : critter_script
{
    enum class state : uint8_t { seek, to_cover, hidden, to_peek, exposed, };

    StringView name() const override;
    const void* id() const override;
    void on_init(const bptr<critter>& c) override;
    void on_update(const bptr<critter>& c, size_t& i, const Ns& dt) override;
    void on_destroy(const bptr<critter>& c, script_destroy_reason reason) override;
    void delete_self() noexcept override;

    cover_script(astar& A, object_id threat, uint32_t div_size);

private:
    bool wait_for(const Ns& dt);
    void seek(const bptr<critter>& c);
    void step_out(const bptr<critter>& c);
    void walk_update(const bptr<critter>& c, size_t& i, const Ns& dt);
    Optional<point> find_peek(critter& c, point threat);

    astar& A;
    critter_walk walk;
    point cover, threat_pos;
    Vector2i axis;
    Ns wait{};
    object_id threat_id;
    uint32_t div_size;
    state st = state::seek;
    uint8_t replans = 0;
};

StringView cover_script::name() const { return "cover"_s; }
const void* cover_script::id() const { return &script_name; }
void cover_script::delete_self() noexcept { delete this; }

// The world is destroyed after astar, so A is gone by now.
void cover_script::on_destroy(const bptr<critter>& c, script_destroy_reason) { c->clear_auto_movement(); }

cover_script::cover_script(astar& A, object_id threat, uint32_t div_size) :
    A{A}, threat_id{threat}, div_size{div_size}
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
        if (wait_for(dt))
            step_out(c);
        return;
    case state::exposed:
        if (wait_for(dt))
        {
            walk = critter_walk{cover};
            st = state::to_cover;
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

void cover_script::seek(const bptr<critter>& c)
{
    auto& w = c->world();
    const auto threat = w.find_object(threat_id);
    if (!threat)
    {
        wait = retry_time;
        return;
    }
    const auto body = Vector2ui(c->bbox_size);
    const auto from = c->position();
    const auto pred = blocks_all_but(c->id);
    const auto r = find_cover_point(w, A.pool_registry(), Cover::Params{div_size},
                                    {from, body, threat->position(), cover_radius}, pred);
    if (!r.found)
    {
        wait = retry_time;
        return;
    }
    cover = r.pos;
    axis = r.axis;
    threat_pos = threat->position();
    if (from == cover)
    {
        st = state::hidden;
        wait = hidden_time;
        return;
    }
    const auto max_dist = point::distance(from, cover)*2 + (uint32_t)tile_size_xy;
    auto path = A.Dijkstra<0, false>(w, from, cover, max_dist, body, pred);
    if (!path.is_found())
    {
        wait = retry_time;
        return;
    }
    (void)A.string_pull(w, path, body, pred);
    walk = critter_walk{move(path)};
    st = state::to_cover;
}

void cover_script::step_out(const bptr<critter>& c)
{
    const auto threat = c->world().find_object(threat_id);
    if (!threat || threat->position() != threat_pos)
    {
        st = state::seek;
        return;
    }
    if (const auto p = find_peek(*c, threat_pos))
    {
        walk = critter_walk{*p};
        st = state::to_peek;
    }
    else
        wait = hidden_time;
}

void cover_script::walk_update(const bptr<critter>& c, size_t& i, const Ns& dt)
{
    const auto ret = walk.update(c, i, dt);
    if (ret == walk_result::none)
        return;
    if (ret == walk_result::arrived)
    {
        replans = 0;
        st = st == state::to_cover ? state::hidden : state::exposed;
        wait = st == state::hidden ? hidden_time : exposed_time;
        return;
    }
    if (st == state::to_peek)
    {
        walk = critter_walk{cover};
        st = state::to_cover;
        return;
    }
    st = state::seek;
    wait = replans++ ? retry_time : Ns{};
}

// The spot nearest the cover, sliding along the blocker's face, where a ray from the threat reaches
// the body's center. Searched in steps of half the body, since a blocker the slide could pass
// is at least that wide, then bisected. Empty when neither side has one within max_peek px.
Optional<point> cover_script::find_peek(critter& c, point threat)
{
    auto& w = c.world();
    auto& ray_pool = A.pool_registry().pass_pool(Pass::Params{div_size, pass_mode::shoot_through});
    const auto body = Vector2i(c.bbox_size);
    const auto across = Math::abs(Vector2i{axis.y(), axis.x()});
    const auto step = Math::max(1, Math::dot(body, across)/2);
    const auto pred = blocks_all_but(c.id);
    const auto seen = [&](int t) {
        return raycast(w, threat, cover + across*t, c.id, ray_pool, Search::without_critters()).success;
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
                break;
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
    return ScriptPtr(new cover_script{A, threat, div_size});
}

} // namespace floormat
