#include "app.hpp"
#include "run.hpp"
#include "compat/array-size.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/function2.hpp"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/critter.hpp"
#include "src/scenery-proto.hpp"
#include "src/anim-atlas.hpp"
#include "src/search.hpp"
#include "src/raycast-line.hpp"
#include "src/rotation.inl"
#include "src/point.inl"
#include "src/nanosecond.inl"
#include <cr/GrowableArray.h>
#include <cr/Optional.h>
#include <cmath>
#include <cstdio>

namespace floormat {
namespace {

constexpr auto origin = point{{0, 0, 0}, {0, 0}, {0, 0}};

constexpr Vector2i dir_vecs[] = { {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1} };

Vector2i vec_of(rotation r) { return dir_vecs[(uint8_t)r]; }
bool is_diagonal(rotation r) { return (uint8_t)r & 1; }
rotation turned(rotation r, int32_t eighths) { return rotation((uint8_t(r) + 8 + eighths) % 8); }

rotation rotation_of(Vector2i d)
{
    for (auto i = 0u; i < 8; i++)
        if (dir_vecs[i] == d)
            return rotation(i);
    fm_assert(false);
}

// rotation_to_similar(r)[k] in src/critter.cpp, for a diagonal r and k of 1 or 2
rotation alternative(rotation r, uint32_t k) { return turned(r, k == 1 ? -1 : 1); }

enum class entry : uint8_t { update_movement, move_toward_dest, move_toward_line, COUNT };
enum class end_reason : uint8_t { below_frame, turn_refused, time, arrived, blocked, at_end, n_zero, COUNT };

enum : uint8_t
{
    ev_turn = 1 << 0, ev_step = 1 << 1, ev_slide1 = 1 << 2, ev_slide2 = 1 << 3,
    ev_slide_failed = 1 << 4, ev_crossed = 1 << 5, ev_COUNT = 1 << 6,
};

constexpr const char* entry_names[] = { "update_movement", "move_toward_dest", "move_toward_line" };
constexpr const char* end_names[] = { "below_frame", "turn_refused", "time", "arrived", "blocked", "at_end", "n_zero" };
constexpr const char* event_names[] = { "ev_turn", "ev_step", "ev_slide1", "ev_slide2", "ev_slide_failed", "ev_crossed" };

struct walker_state
{
    point pos;
    rotation r;
    Vector2b bbox_offset;
    Vector2ub bbox_size;
    uint32_t delta;
    uint8_t moves;

    bool operator==(const walker_state&) const = default;
};

walker_state state_of(const critter& C)
{
    return { C.position(), C.r, C.bbox_offset, C.bbox_size, C.delta, C.moves_ };
}

walker_state facing(walker_state s, rotation new_r)
{
    s.bbox_offset = rotate_point(s.bbox_offset, s.r, new_r);
    s.bbox_size = rotate_size(s.bbox_size, s.r, new_r);
    s.r = new_r;
    return s;
}

struct walk_outcome
{
    int32_t stop_after = -1;
    uint32_t steps = 0, slides = 0;
    uint8_t first_slide = 0;
    bool resumed = false;

    void blocked()
    {
        if (stop_after < 0)
            stop_after = (int32_t)steps;
    }
};

struct expected_call
{
    walker_state s;
    Ns dt;
    critter::move_result res;
    uint8_t events;
    end_reason end;
    walk_outcome o;
};

struct skip_self
{
    object_id id;

    path_search_continue operator()(chunk&, collision_data x, Range2D) const
    {
        return x.id == id ? path_search_continue::pass : path_search_continue::blocked;
    }
};

// Moves one pixel at a time. A move from p to p + d is free when the box holding the boxes at p and
// p + d overlaps no blocker, searched from p's chunk.
struct model
{
    world& w;
    skip_self pred;
    Ns frame, diag;

    Ns cost(rotation r) const { return is_diagonal(r) ? diag : frame; }

    Range2D box_at(point p, chunk_coords_ ch, Vector2b offset, Vector2ub size) const
    {
        const auto min = p - point{ch, {}, {}} + Vector2i(offset) - Vector2i(size/2);
        return Range2D{Vector2(min), Vector2(min + Vector2i(size))};
    }

    bool step_free(const walker_state& s, Vector2i d) const
    {
        const auto ch = s.pos.chunk3();
        const auto a = box_at(s.pos, ch, s.bbox_offset, s.bbox_size);
        const auto b = Range2D{a.min() + Vector2(d), a.max() + Vector2(d)};
        return Search::is_passable(w, ch, Range2D{Math::min(a.min(), b.min()), Math::max(a.max(), b.max())}, pred);
    }

    bool can_turn(const walker_state& s, rotation new_r) const
    {
        if (new_r == s.r)
            return true;
        const auto t = facing(s, new_r);
        const auto ch = s.pos.chunk3();
        return Search::is_passable(w, ch, box_at(t.pos, ch, t.bbox_offset, t.bbox_size), pred);
    }

    expected_call update_movement(walker_state s, Ns dt, rotation new_r, walk_outcome o) const
    {
        const auto ch = s.pos.chunk3();
        auto ticks = dt + Ns{s.delta};
        uint8_t ev = 0;
        const auto ret = [&](end_reason end) {
            if (s.pos.chunk3() != ch)
                ev |= ev_crossed;
            return expected_call{s, Ns{}, {}, ev, end, o};
        };
        if (ticks < frame)
        {
            s.delta = object::ns_to_delta(ticks);
            return ret(end_reason::below_frame);
        }
        if (s.r != new_r)
        {
            if (!can_turn(s, new_r))
            {
                s.delta = 0;
                o.blocked();
                return ret(end_reason::turn_refused);
            }
            s = facing(s, new_r);
            ev |= ev_turn;
        }
        const auto step = cost(new_r);
        while (ticks >= step)
        {
            if (step_free(s, vec_of(new_r)))
            {
                s.pos = s.pos + vec_of(new_r);
                ticks -= step;
                ev |= ev_step;
                o.steps++;
                o.resumed |= o.slides > 0;
                continue;
            }
            o.blocked();
            uint32_t k = 1;
            while (is_diagonal(new_r) && k <= 2 && !step_free(s, vec_of(alternative(new_r, k))))
                k++;
            if (!is_diagonal(new_r) || k > 2)
            {
                s.delta = 0;
                if (is_diagonal(new_r))
                    ev |= ev_slide_failed;
                return ret(end_reason::blocked);
            }
            s.pos = s.pos + vec_of(alternative(new_r, k));
            ticks -= frame;
            ev |= k == 1 ? ev_slide1 : ev_slide2;
            o.slides++;
            if (!o.first_slide)
                o.first_slide = (uint8_t)k;
        }
        s.delta = object::ns_to_delta(ticks);
        return ret(end_reason::time);
    }

    expected_call move_toward(walker_state s, Ns dt, point dest, walk_outcome o) const
    {
        const auto ch = s.pos.chunk3();
        auto ticks = dt + Ns{s.delta};
        uint8_t ev = 0;
        bool moved = false;
        const auto ret = [&](end_reason end, Ns dt_out, bool blocked) {
            if (s.pos.chunk3() != ch)
                ev |= ev_crossed;
            return expected_call{s, dt_out, {.blocked = blocked, .moved = moved}, ev, end, o};
        };
        const auto blocked = [&](end_reason end) {
            o.blocked();
            s.moves = 0;
            s.delta = 0;
            return ret(end, ticks, true);
        };
        for (;;)
        {
            if (s.pos == dest)
            {
                s.delta = 0;
                return ret(end_reason::arrived, ticks, false);
            }
            // next_step() in src/critter.cpp: along the longer axis until both are equal, then diagonally
            const auto v = dest - s.pos, a = Math::abs(v);
            const auto d = Vector2i{a.x() >= a.y() ? Math::sign(v.x()) : 0, a.y() >= a.x() ? Math::sign(v.y()) : 0};
            const auto new_r = rotation_of(d);
            if (ticks < cost(new_r))
            {
                s.delta = object::ns_to_delta(ticks);
                return ret(end_reason::time, Ns{}, false);
            }
            if (s.r != new_r)
            {
                if (!can_turn(s, new_r))
                    return blocked(end_reason::turn_refused);
                s = facing(s, new_r);
                ev |= ev_turn;
            }
            if (!step_free(s, d))
                return blocked(end_reason::blocked);
            s.pos = s.pos + d;
            ticks -= cost(new_r);
            moved = true;
            ev |= ev_step;
            o.steps++;
        }
    }

    expected_call move_toward(walker_state s, Ns dt, point from, point dest, rotation line_r, Ns step_cost,
                              walk_outcome o) const
    {
        const auto ch = s.pos.chunk3();
        const auto line_origin = point{from.chunk3(), {}, {}};
        const auto line = rc::bresenham_line{from - line_origin, dest - line_origin};
        const auto steps = line.steps();
        const auto k = line.step_of(s.pos - line_origin);
        fm_assert(line_origin + line.pixel_at(k) == s.pos);
        uint8_t ev = 0;
        const auto ret = [&](end_reason end, Ns dt_out, critter::move_result res) {
            if (s.pos.chunk3() != ch)
                ev |= ev_crossed;
            return expected_call{s, dt_out, res, ev, end, o};
        };
        if (k == steps)
            return ret(end_reason::at_end, dt, {false, false});
        auto ticks = dt + Ns{s.delta};
        const auto n = (uint32_t)Math::min(uint64_t{steps - k}, ticks / step_cost);
        if (n == 0)
        {
            s.delta = object::ns_to_delta(ticks);
            return ret(end_reason::n_zero, Ns{}, {false, false});
        }
        if (s.r != line_r)
        {
            if (!can_turn(s, line_r))
            {
                o.blocked();
                s.moves = 0;
                s.delta = 0;
                return ret(end_reason::turn_refused, dt, {true, false});
            }
            s = facing(s, line_r);
            ev |= ev_turn;
        }
        auto t = k;
        while (t < k + n && step_free(s, line.pixel_at(t + 1) - line.pixel_at(t)))
        {
            s.pos = s.pos + (line.pixel_at(t + 1) - line.pixel_at(t));
            t++;
        }
        const bool hit = t < k + n;
        ticks -= step_cost * (t - k);
        o.steps += t - k;
        if (t > k)
            ev |= ev_step;
        if (!hit && t < steps)
        {
            s.delta = object::ns_to_delta(ticks);
            return ret(end_reason::time, Ns{}, {false, true});
        }
        s.delta = 0;
        if (!hit)
            return ret(end_reason::arrived, ticks, {false, true});
        o.blocked();
        s.moves = 0;
        return ret(end_reason::blocked, ticks, {true, t > k});
    }
};

struct span { int32_t lo, hi; };

// On an axis the walk moves along with sign s, the rect's near face meets the leading face of the
// box [lo, hi) after k0 steps. It overlaps the box swept from step k to k + 1 when k >= k0.
span ahead(int32_t lo, int32_t hi, int32_t s, int32_t k0, int32_t len)
{
    return s > 0 ? span{hi + k0, hi + k0 + len} : span{lo - k0 - len, lo - k0};
}

// The rect's far face meets the box's trailing face after k0 steps. It overlaps the swept box when
// k < k0, given len >= k0.
span behind(int32_t lo, int32_t hi, int32_t s, int32_t k0, int32_t len)
{
    return s > 0 ? span{lo + k0 - len, lo + k0} : span{hi - k0, hi - k0 + len};
}

// A blocker covering x and y in the frame of chunk (0, 0, 0). bbox_size is 8 bits, so a longer one
// is placed as adjoining pieces, which block exactly where one rect would.
void add_rect(world& w, span x, span y)
{
    fm_assert(x.lo < x.hi && y.lo < y.hi);
    constexpr int32_t piece = 255;
    for (auto y0 = y.lo; y0 < y.hi; y0 += piece)
        for (auto x0 = x.lo; x0 < x.hi; x0 += piece)
        {
            const auto size = Vector2i{Math::min(x.hi - x0, piece), Math::min(y.hi - y0, piece)};
            const auto center = origin + (Vector2i{x0, y0} + size/2);
            scenery_proto p;
            p.atlas       = loader.invalid_scenery_atlas().proto->atlas;
            p.subtype     = generic_scenery_proto{};
            p.bbox_offset = center.offset();
            p.bbox_size   = Vector2ub(size);
            p.pass        = pass_mode::blocked;
            w.make_scenery(w.make_id(), center.coord(), move(p));
        }
}

struct box_desc
{
    Vector2ub size;
    Vector2b offset;
};

// The first three are square without an offset, so turning never moves them.
constexpr box_desc boxes[] = {
    { Vector2ub{1}, {} }, { Vector2ub{2}, {} }, { Vector2ub{24}, {} }, { {23, 25}, {} }, { Vector2ub{7}, {5, -3} },
};
constexpr uint32_t square_boxes = 3;

struct walk_box { Vector2i lo, hi; };

// The proto's box is the one facing N.
walk_box box_facing(Vector2i px, const box_desc& b, rotation r)
{
    const auto lo = px + Vector2i(rotate_point(b.offset, rotation::N, r)) - Vector2i(rotate_size(b.size, rotation::N, r)/2);
    return { lo, lo + Vector2i(rotate_size(b.size, rotation::N, r)) };
}

enum class level : uint8_t
{
    none, start_overlap, head_on, beside_flush, lane, turn_refused, leading_corner, side_corner,
    second_leg, second_turn_refused,
};

constexpr const char* level_names[] = {
    "none", "start_overlap", "head_on", "beside_flush", "lane", "turn_refused", "leading_corner", "side_corner",
    "second_leg", "second_turn_refused",
};

struct layout
{
    level lv;
    int32_t gap = 0;
    // beside_flush and lane: 0 is the lane's low side, 1 the high one. side_corner: the axis the
    // rect is ahead on; it is behind on the other.
    uint8_t side = 0;
    int32_t overlap = 0;
};

struct direction
{
    // The step for unit directions, the whole walk for the others.
    Vector2i vec;
    rotation leg_r;
    rotation line_r;
    bool unit;
};

constexpr direction off_octant[] = {
    { {80, -26}, rotation::E, rotation::E,  false },
    { {54,  80}, rotation::S, rotation::SE, false },
    { {-80, 54}, rotation::W, rotation::SW, false },
    { {-26, -80}, rotation::N, rotation::N, false },
};

uint32_t major_of(Vector2i v) { return Math::abs(v.x()) >= Math::abs(v.y()) ? 0 : 1; }

// move_toward(dest)'s first leg, along the major axis
int32_t first_leg(Vector2i v)
{
    const auto a = Math::abs(v);
    return Math::max(a.x(), a.y()) - Math::min(a.x(), a.y());
}

// b_diag: the box facing the diagonal leg where it starts.
void place_obstacles(world& w, const layout& l, const direction& d, walk_box b, walk_box b_turned, walk_box b_diag,
                     int32_t len)
{
    const auto s = Math::sign(d.vec);
    const auto a = major_of(d.vec), c = 1 - a;
    const auto g = l.gap;
    const auto on = [&](uint32_t i, span p, span q) {
        if (i == 0)
            add_rect(w, p, q);
        else
            add_rect(w, q, p);
    };
    const auto ahead_ = [&](uint32_t i, int32_t k0, int32_t n) { return ahead(b.lo[i], b.hi[i], s[i], k0, n); };
    const auto behind_ = [&](uint32_t i, int32_t k0, int32_t n) { return behind(b.lo[i], b.hi[i], s[i], k0, n); };
    const auto around = [&](uint32_t i, int32_t k, int32_t m) {
        return span{b.lo[i] + k*s[i] - m, b.hi[i] + k*s[i] + m};
    };

    switch (l.lv)
    {
    case level::none:
        break;
    case level::start_overlap:
        add_rect(w, {b.lo.x() - 8, b.lo.x() + 1}, {b.lo.y() - 8, b.lo.y() + 1});
        break;
    case level::head_on:
        if (d.unit && s.x() && s.y())
        {
            // Both cardinal moves at the corner are blocked too.
            add_rect(w, ahead_(0, g, 8), around(1, g, 8));
            add_rect(w, around(0, g, 8), ahead_(1, g, 8));
        }
        else
            on(a, ahead_(a, g, 8), around(c, 0, 16));
        break;
    case level::beside_flush:
        on(a, span{Math::min(b.lo[a], b.lo[a] + len*s[a]) - 8, Math::max(b.hi[a], b.hi[a] + len*s[a]) + 8},
           l.side ? span{b.hi[c], b.hi[c] + 8} : span{b.lo[c] - 8, b.lo[c]});
        break;
    case level::lane:
        on(a, ahead_(a, g, 40), l.side ? span{b.hi[c] - 1, b.hi[c] + 8} : span{b.lo[c] - 8, b.lo[c] + 1});
        break;
    case level::turn_refused:
    {
        // Blocks step 1 too. Made first, so a search in its chunk meets it before the rect refusing
        // the turn, which must still be found.
        on(a, ahead_(a, 0, 8), around(c, 0, 16));
        const auto& b0 = b_turned;
        const auto i = b.hi[0] - b.lo[0] > b0.hi[0] - b0.lo[0] ? 0u : 1u;
        fm_assert(b.hi[i] - b.lo[i] > b0.hi[i] - b0.lo[i]);
        const auto lo = Math::min(b.lo, b0.lo) - Vector2i{8}, hi = Math::max(b.hi, b0.hi) + Vector2i{8};
        on(i, span{b0.hi[i], b0.hi[i] + 8}, span{lo[1 - i], hi[1 - i]});
        break;
    }
    case level::leading_corner:
        add_rect(w, ahead_(0, g, 8), ahead_(1, g, 8));
        break;
    case level::side_corner:
        on(l.side, ahead_(l.side, g, 8), behind_(1 - l.side, g + l.overlap, g + l.overlap + 8));
        break;
    case level::second_leg:
        add_rect(w, ahead(b_diag.lo.x(), b_diag.hi.x(), s.x(), g, 8), ahead(b_diag.lo.y(), b_diag.hi.y(), s.y(), g, 8));
        break;
    case level::second_turn_refused:
    {
        // Where the turned box reaches past the first leg's box across the first leg, which the first
        // leg never sweeps.
        const auto shift = vec_of(d.leg_r) * first_leg(d.vec);
        const auto b1 = walk_box{b.lo + shift, b.hi + shift};
        const auto m = b_diag.lo[c] < b1.lo[c] ? span{b_diag.lo[c], b1.lo[c]} : span{b1.hi[c], b_diag.hi[c]};
        on(c, m, span{b_diag.lo[a], b_diag.hi[a]});
        break;
    }
    }
}

struct expected_outcome
{
    int32_t stop_after;
    uint8_t slide;
    int32_t slides;
};

expected_outcome outcome_of(const layout& l, const direction& d)
{
    const auto leg_r = d.leg_r;
    switch (l.lv)
    {
    case level::none:
    case level::beside_flush:
        return { -1, 0, 0 };
    case level::start_overlap:
    case level::turn_refused:
        return { 0, 0, 0 };
    case level::head_on:
    case level::lane:
        return { l.gap, 0, 0 };
    case level::leading_corner:
        return { l.gap, 1, -1 };
    case level::side_corner:
        if (l.overlap == 0)
            return { -1, 0, 0 };
        // The free cardinal moves along the axis the rect is behind on.
        return { l.gap, uint8_t(vec_of(alternative(leg_r, 1))[l.side] == 0 ? 1 : 2), l.overlap };
    case level::second_leg:
        return { first_leg(d.vec) + l.gap, 0, 0 };
    case level::second_turn_refused:
        return { first_leg(d.vec), 0, 0 };
    }
    fm_assert(false);
}

enum class place : uint8_t { inside, edge, corner, missing };
constexpr const char* place_names[] = { "inside", "edge", "corner", "missing" };

Vector2i start_px(place pl, Vector2i s, uint32_t major)
{
    const auto edge = [](int32_t si) { return si > 0 ? chunk_size<int32_t> - half_tile<int32_t> - 1 : -half_tile<int32_t>; };
    auto px = chunk_size<Vector2i>/2;
    if (pl == place::edge || pl == place::missing)
        px[major] = edge(s[major]);
    else if (pl == place::corner)
        px = {edge(s.x()), edge(s.y())};
    return px;
}

enum class time_mode : uint8_t { half_frame, one_frame, three_frames, whole };
constexpr const char* time_names[] = { "half_frame", "one_frame", "three_frames", "whole" };

struct walk
{
    entry e;
    rotation walk_r;
    point start, dest;
    uint32_t budget;
    Ns line_cost;
};

struct case_desc
{
    const walk& wk;
    rotation start_r;
    uint32_t box;
    layout l;
    place pl;
    time_mode t;
    int32_t random = -1;
};

void print_case(const case_desc& cd, uint32_t call)
{
    const auto& b = boxes[cd.box];
    DBG_nospace << "critter-walk: " << entry_names[(uint32_t)cd.wk.e] << " from " << cd.wk.start << " to "
                << cd.wk.dest << " walk_r " << (int)cd.wk.walk_r << " start_r " << (int)cd.start_r
                << " box " << b.size << " " << b.offset << " level " << level_names[(uint32_t)cd.l.lv]
                << " gap " << cd.l.gap << " side " << (int)cd.l.side << " overlap " << cd.l.overlap
                << " place " << place_names[(uint32_t)cd.pl] << " time " << time_names[(uint32_t)cd.t]
                << " random " << cd.random << " call " << call;
}

void print_state(const char* name, const walker_state& s, Ns dt, critter::move_result res)
{
    DBG_nospace << name << ": pos " << s.pos << " r " << (int)s.r << " bbox " << s.bbox_offset << " "
                << s.bbox_size << " delta " << s.delta << " moves " << (int)s.moves << " dt " << dt.stamp
                << " blocked " << res.blocked << " moved " << res.moved << " displaced " << res.displaced;
}

void check_call(const critter& C, const expected_call& x, critter::move_result res, Ns dt, const case_desc& cd,
                uint32_t call)
{
    const auto s = state_of(C);
    bool ok = s == x.s;
    if (cd.wk.e != entry::update_movement)
        ok = ok && res.blocked == x.res.blocked && res.moved == x.res.moved && res.displaced == x.res.displaced
                && dt == x.dt;
    if (ok)
        return;
    print_case(cd, call);
    print_state("code ", s, dt, res);
    print_state("model", x.s, x.dt, x.res);
    DBG_nospace << "model end " << end_names[(uint32_t)x.end] << " events " << x.events;
    fm_abort("critter-walk: the critter differs from the model");
}

struct combos
{
    bool seen[(size_t)entry::COUNT][ev_COUNT][(size_t)end_reason::COUNT] = {};
};

Ns remaining_cost(const model& m, const walk& wk, const walker_state& s, const walk_outcome& o)
{
    switch (wk.e)
    {
    case entry::update_movement:
        return m.cost(wk.walk_r) * (wk.budget - Math::min(wk.budget, o.steps + o.slides));
    case entry::move_toward_dest:
    {
        const auto a = Math::abs(wk.dest - s.pos);
        const auto lo = (uint32_t)Math::min(a.x(), a.y()), hi = (uint32_t)Math::max(a.x(), a.y());
        return m.frame * (hi - lo) + m.diag * lo;
    }
    case entry::move_toward_line:
    {
        const auto line_origin = point{wk.start.chunk3(), {}, {}};
        const auto line = rc::bresenham_line{wk.start - line_origin, wk.dest - line_origin};
        return wk.line_cost * (line.steps() - line.step_of(s.pos - line_origin));
    }
    case entry::COUNT:
        break;
    }
    fm_assert(false);
}

template<typename DtFn>
walk_outcome run_walk(const model& m, critter& C, const case_desc& cd, combos* seen, const DtFn& next_dt)
{
    const auto& wk = cd.wk;
    auto idx = C.index();
    C.teleport_to(idx, wk.start, cd.start_r);
    C.delta = 0;
    C.set_keys(true, false, true, false);
    walk_outcome o;
    bool last = false;
    for (uint32_t call = 0; ; call++)
    {
        fm_assert(call < 10'000);
        const auto s = state_of(C);
        const auto dt = next_dt(s, o);
        auto dt_io = dt;
        critter::move_result res{};
        expected_call x;
        switch (wk.e)
        {
        case entry::update_movement:
            x = m.update_movement(s, dt, wk.walk_r, o);
            C.update_movement(idx, dt, wk.walk_r);
            break;
        case entry::move_toward_dest:
            x = m.move_toward(s, dt, wk.dest, o);
            res = C.move_toward(idx, dt_io, wk.dest);
            break;
        case entry::move_toward_line:
            x = m.move_toward(s, dt, wk.start, wk.dest, wk.walk_r, wk.line_cost, o);
            res = C.move_toward(idx, dt_io, wk.start, wk.dest);
            break;
        case entry::COUNT:
            fm_assert(false);
        }
        check_call(C, x, res, dt_io, cd, call);
        o = x.o;
        if (seen)
            seen->seen[(size_t)wk.e][x.events][(size_t)x.end] = true;
        const bool ended = x.end == end_reason::blocked || x.end == end_reason::turn_refused ||
                           x.end == end_reason::arrived || x.end == end_reason::at_end;
        if (last || wk.e == entry::update_movement && (ended || o.steps + o.slides >= wk.budget))
            break;
        last = ended;
    }
    return o;
}

Ns measure_line_cost(Vector2i vec)
{
    auto w = world();
    object_id id = 0;
    auto& C = *w.ensure_player_character(id, Run::make_proto(1.f));
    auto idx = C.index();
    const auto from = origin + chunk_size<Vector2i>/2;
    C.teleport_to(idx, from, rotation_COUNT);
    C.delta = 0;
    const auto steps = rc::bresenham_line{{}, vec}.steps();
    const auto dt_in = object::frame_duration(C.atlas->info().fps) * (2 * steps + 1);
    auto dt = dt_in;
    const auto res = C.move_toward(idx, dt, from, from + vec);
    fm_assert(!res.blocked && res.moved && C.position() == from + vec);
    const auto spent = dt_in - dt;
    fm_assert(spent.stamp % steps == 0);
    const auto cost = double(spent.stamp / steps);
    // The code computes frame * step_length() / anim_speed_unit in integers. step_length() is under
    // two units short and the product is rounded down, so the cost is at most
    // 2 * frame / anim_speed_unit + 1 ns below frame * |vec| / steps, and never above it.
    const auto frame = double(object::frame_duration(C.atlas->info().fps).stamp);
    const auto exact = frame * std::sqrt(double(vec.dot())) / double(steps);
    fm_assert(cost <= exact && cost >= exact - 2 * frame / double(anim_speed_unit) - 1);
    return Ns{spent.stamp / steps};
}

struct line_cost_entry
{
    Vector2i vec;
    Ns cost;
};

struct product
{
    combos seen;
    Array<line_cost_entry> line_costs;

    Ns line_cost(Vector2i vec)
    {
        for (const auto& x : line_costs)
            if (x.vec == vec)
                return x.cost;
        const auto cost = measure_line_cost(vec);
        arrayAppend(line_costs, line_cost_entry{vec, cost});
        return cost;
    }
};

critter& populate(world& w, const layout& l, const direction& d, uint32_t box, Vector2i px, int32_t len,
                  bool make_chunks)
{
    if (make_chunks)
        for (int16_t y = -1; y <= 1; y++)
            for (int16_t x = -1; x <= 1; x++)
                (void)w[chunk_coords_{x, y, 0}];
    const auto& b = boxes[box];
    const auto diag_px = px + vec_of(d.leg_r) * first_leg(d.vec);
    place_obstacles(w, l, d, box_facing(px, b, d.leg_r), box_facing(px, b, turned(d.leg_r, 2)),
                    box_facing(diag_px, b, rotation_of(Math::sign(d.vec))), len);
    object_id id = 0;
    auto proto = Run::make_proto(1.f);
    proto.bbox_offset = b.offset;
    proto.bbox_size = b.size;
    auto& C = *w.ensure_player_character(id, move(proto));
    Run::mark_all_modified(w);
    return C;
}

int32_t walk_length(const layout& l)
{
    switch (l.lv)
    {
    case level::none:
    case level::beside_flush:
        return 70;
    case level::start_overlap:
    case level::turn_refused:
        return 3;
    case level::head_on:
    case level::lane:
    case level::leading_corner:
        return l.gap + 2;
    case level::side_corner:
        return l.overlap == 0 ? 70 : l.gap + 2;
    case level::second_leg:
    case level::second_turn_refused:
        // off-octant only, which walk d.vec whatever the length
        return 0;
    }
    fm_assert(false);
}

// only: the one entry point a layout is walked with, or COUNT for all.
void walk_layout(product& P, const direction& d, uint32_t box, const layout& l, place pl, entry only)
{
    const auto s = Math::sign(d.vec);
    const auto px = start_px(pl, s, major_of(d.vec));
    const auto start = origin + px;
    const auto len = walk_length(l);
    const auto vec = d.unit ? d.vec * len : d.vec;
    const auto x = outcome_of(l, d);
    const auto budget = uint32_t(l.lv == level::side_corner && l.overlap > 0 ? l.gap + l.overlap + 4 : len);
    constexpr time_mode all_times[] = { time_mode::half_frame, time_mode::one_frame, time_mode::three_frames, time_mode::whole };
    constexpr time_mode whole_only[] = { time_mode::whole };
    const bool long_walk = only != entry::COUNT && l.lv == level::head_on;
    const auto times = long_walk ? ArrayView<const time_mode>{whole_only} : ArrayView<const time_mode>{all_times};

    const auto run = [&](world& w, critter& C, entry e, rotation start_r, time_mode t) {
        const auto& info = C.atlas->info();
        const auto m = model{w, skip_self{C.id}, object::frame_duration(info.fps), P.line_cost({1, 1})};
        const auto walk_r = e == entry::move_toward_line ? d.line_r : d.leg_r;
        const auto wk = walk{e, walk_r, start, start + vec, budget,
                             e == entry::move_toward_line ? P.line_cost(vec) : Ns{}};
        const auto cd = case_desc{wk, start_r, box, l, pl, t};
        const auto next_dt = [&](const walker_state& st, const walk_outcome& o) {
            switch (t)
            {
            case time_mode::half_frame: return m.frame / 2u;
            case time_mode::one_frame: return m.frame;
            case time_mode::three_frames: return m.frame * 3u;
            case time_mode::whole: return remaining_cost(m, wk, st, o) + m.frame / 2u;
            }
            fm_assert(false);
        };
        const auto o = run_walk(m, C, cd, &P.seen, next_dt);

        bool ok = o.stop_after == x.stop_after;
        if (e == entry::update_movement)
            ok = ok && o.first_slide == x.slide && (x.slides < 0 || o.slides == uint32_t(x.slides) && (x.slides == 0 || o.resumed));
        else if (x.stop_after < 0)
            ok = ok && C.position() == wk.dest;
        if (!ok)
        {
            print_case(cd, 0);
            DBG_nospace << "outcome: stop_after " << o.stop_after << " steps " << o.steps << " slides " << o.slides
                        << " first_slide " << o.first_slide << " resumed " << o.resumed << ", expected stop_after "
                        << x.stop_after << " slide " << x.slide << " slides " << x.slides;
            fm_abort("critter-walk: the walk doesn't end as its obstacle level says");
        }
    };

    Optional<world> shared;
    critter* shared_C = nullptr;
    if (pl != place::missing)
    {
        shared.emplace(world());
        shared_C = &populate(*shared, l, d, box, px, len, true);
    }
    for (auto e : { entry::update_movement, entry::move_toward_dest, entry::move_toward_line })
    {
        if (only != entry::COUNT && e != only || !d.unit && e == entry::update_movement)
            continue;
        const auto walk_r = e == entry::move_toward_line ? d.line_r : d.leg_r;
        for (auto start_r : { walk_r, turned(walk_r, 2) })
        {
            if (l.lv == level::turn_refused && start_r == walk_r)
                continue;
            for (auto t : times)
            {
                if (shared)
                    run(*shared, *shared_C, e, start_r, t);
                else
                {
                    auto w = world();
                    run(w, populate(w, l, d, box, px, len, false), e, start_r, t);
                }
            }
        }
    }
}

template<typename F>
void each_layout(const direction& d, uint32_t box, const F& f)
{
    f(layout{level::none}, entry::COUNT);
    if (!d.unit)
    {
        // The line walk takes another path.
        for (int32_t g : { 0, 1, 2 })
            f(layout{level::second_leg, g}, entry::move_toward_dest);
        if (box >= square_boxes)
            f(layout{level::second_turn_refused}, entry::move_toward_dest);
        else
        {
            // These are placed for the box facing leg_r, and the line walk may face another way,
            // which only a square box without an offset ignores.
            f(layout{level::start_overlap}, entry::COUNT);
            for (int32_t g : { 0, 1, 2 })
                f(layout{level::head_on, g}, entry::COUNT);
        }
        return;
    }
    f(layout{level::start_overlap}, entry::COUNT);
    for (int32_t g : { 0, 1, 2 })
        f(layout{level::head_on, g}, entry::COUNT);
    // max_run in update_movement() and move_toward(dest)
    for (int32_t g : { 511, 512, 513 })
        for (auto e : { entry::update_movement, entry::move_toward_dest })
            f(layout{level::head_on, g}, e);
    if (!is_diagonal(d.leg_r))
    {
        for (auto side : { uint8_t{0}, uint8_t{1} })
        {
            f(layout{level::beside_flush, 0, side}, entry::COUNT);
            for (int32_t g : { 0, 1, 2 })
                f(layout{level::lane, g, side}, entry::COUNT);
        }
        if (boxes[box].size.x() != boxes[box].size.y())
            f(layout{level::turn_refused}, entry::COUNT);
        return;
    }
    for (int32_t g : { 0, 1, 2 })
    {
        f(layout{level::leading_corner, g}, entry::COUNT);
        for (auto side : { uint8_t{0}, uint8_t{1} })
            for (int32_t n : { 0, 1, 2, 10 })
                f(layout{level::side_corner, g, side, n}, entry::COUNT);
    }
}

void run_product(product& P)
{
    const auto walk_direction = [&](const direction& d, uint32_t nboxes) {
        for (auto box = 0u; box < nboxes; box++)
            each_layout(d, box, [&](const layout& l, entry only) {
                const bool diagonal = d.unit && is_diagonal(d.leg_r);
                for (auto pl : { place::inside, place::edge, place::corner, place::missing })
                {
                    if (pl == place::corner && !diagonal || pl == place::missing && l.lv != level::none)
                        continue;
                    walk_layout(P, d, box, l, pl, only);
                }
            });
    };
    for (auto i = 0u; i < 8; i++)
        walk_direction(direction{dir_vecs[i], rotation(i), rotation(i), true}, (uint32_t)array_size(boxes));
    for (const auto& d : off_octant)
        walk_direction(d, (uint32_t)array_size(boxes));
}

struct combo
{
    entry e;
    uint8_t events;
    end_reason end;
};

constexpr combo expected_combos[] = {
    { entry::update_movement, 0, end_reason::below_frame },
    { entry::update_movement, 0, end_reason::turn_refused },
    { entry::update_movement, 0, end_reason::time },
    { entry::update_movement, 0, end_reason::blocked },
    { entry::update_movement, ev_turn, end_reason::time },
    { entry::update_movement, ev_turn, end_reason::blocked },
    { entry::update_movement, ev_step, end_reason::time },
    { entry::update_movement, ev_step, end_reason::blocked },
    { entry::update_movement, ev_turn|ev_step, end_reason::time },
    { entry::update_movement, ev_turn|ev_step, end_reason::blocked },
    { entry::update_movement, ev_slide1, end_reason::time },
    { entry::update_movement, ev_turn|ev_slide1, end_reason::time },
    { entry::update_movement, ev_step|ev_slide1, end_reason::time },
    { entry::update_movement, ev_turn|ev_step|ev_slide1, end_reason::time },
    { entry::update_movement, ev_slide2, end_reason::time },
    { entry::update_movement, ev_turn|ev_slide2, end_reason::time },
    { entry::update_movement, ev_step|ev_slide2, end_reason::time },
    { entry::update_movement, ev_turn|ev_step|ev_slide2, end_reason::time },
    { entry::update_movement, ev_slide_failed, end_reason::blocked },
    { entry::update_movement, ev_turn|ev_slide_failed, end_reason::blocked },
    { entry::update_movement, ev_step|ev_slide_failed, end_reason::blocked },
    { entry::update_movement, ev_turn|ev_step|ev_slide_failed, end_reason::blocked },
    { entry::update_movement, ev_step|ev_crossed, end_reason::time },
    { entry::update_movement, ev_step|ev_crossed, end_reason::blocked },
    { entry::update_movement, ev_turn|ev_step|ev_crossed, end_reason::time },
    { entry::update_movement, ev_turn|ev_step|ev_crossed, end_reason::blocked },
    { entry::update_movement, ev_slide1|ev_crossed, end_reason::time },
    { entry::update_movement, ev_turn|ev_slide1|ev_crossed, end_reason::time },
    { entry::update_movement, ev_step|ev_slide1|ev_crossed, end_reason::time },
    { entry::update_movement, ev_turn|ev_step|ev_slide1|ev_crossed, end_reason::time },
    { entry::update_movement, ev_slide2|ev_crossed, end_reason::time },
    { entry::update_movement, ev_turn|ev_slide2|ev_crossed, end_reason::time },
    { entry::update_movement, ev_step|ev_slide2|ev_crossed, end_reason::time },
    { entry::update_movement, ev_turn|ev_step|ev_slide2|ev_crossed, end_reason::time },
    { entry::update_movement, ev_step|ev_slide_failed|ev_crossed, end_reason::blocked },
    { entry::update_movement, ev_turn|ev_step|ev_slide_failed|ev_crossed, end_reason::blocked },
    { entry::move_toward_dest, 0, end_reason::turn_refused },
    { entry::move_toward_dest, 0, end_reason::time },
    { entry::move_toward_dest, 0, end_reason::arrived },
    { entry::move_toward_dest, 0, end_reason::blocked },
    { entry::move_toward_dest, ev_turn, end_reason::blocked },
    { entry::move_toward_dest, ev_step, end_reason::turn_refused },
    { entry::move_toward_dest, ev_step, end_reason::time },
    { entry::move_toward_dest, ev_step, end_reason::arrived },
    { entry::move_toward_dest, ev_step, end_reason::blocked },
    { entry::move_toward_dest, ev_turn|ev_step, end_reason::turn_refused },
    { entry::move_toward_dest, ev_turn|ev_step, end_reason::time },
    { entry::move_toward_dest, ev_turn|ev_step, end_reason::arrived },
    { entry::move_toward_dest, ev_turn|ev_step, end_reason::blocked },
    { entry::move_toward_dest, ev_step|ev_crossed, end_reason::turn_refused },
    { entry::move_toward_dest, ev_step|ev_crossed, end_reason::time },
    { entry::move_toward_dest, ev_step|ev_crossed, end_reason::arrived },
    { entry::move_toward_dest, ev_step|ev_crossed, end_reason::blocked },
    { entry::move_toward_dest, ev_turn|ev_step|ev_crossed, end_reason::turn_refused },
    { entry::move_toward_dest, ev_turn|ev_step|ev_crossed, end_reason::time },
    { entry::move_toward_dest, ev_turn|ev_step|ev_crossed, end_reason::arrived },
    { entry::move_toward_dest, ev_turn|ev_step|ev_crossed, end_reason::blocked },
    { entry::move_toward_line, 0, end_reason::turn_refused },
    { entry::move_toward_line, 0, end_reason::blocked },
    { entry::move_toward_line, 0, end_reason::at_end },
    { entry::move_toward_line, 0, end_reason::n_zero },
    { entry::move_toward_line, ev_turn, end_reason::blocked },
    { entry::move_toward_line, ev_step, end_reason::time },
    { entry::move_toward_line, ev_step, end_reason::arrived },
    { entry::move_toward_line, ev_step, end_reason::blocked },
    { entry::move_toward_line, ev_turn|ev_step, end_reason::time },
    { entry::move_toward_line, ev_turn|ev_step, end_reason::arrived },
    { entry::move_toward_line, ev_turn|ev_step, end_reason::blocked },
    { entry::move_toward_line, ev_step|ev_crossed, end_reason::time },
    { entry::move_toward_line, ev_step|ev_crossed, end_reason::arrived },
    { entry::move_toward_line, ev_step|ev_crossed, end_reason::blocked },
    { entry::move_toward_line, ev_turn|ev_step|ev_crossed, end_reason::time },
    { entry::move_toward_line, ev_turn|ev_step|ev_crossed, end_reason::arrived },
    { entry::move_toward_line, ev_turn|ev_step|ev_crossed, end_reason::blocked },
};

void print_combo(entry e, uint32_t ev, end_reason end, const char* note)
{
    std::fprintf(stderr, "    { entry::%s, ", entry_names[(uint32_t)e]);
    if (!ev)
        std::fputs("0", stderr);
    for (auto i = 0u, n = 0u; i < array_size(event_names); i++)
        if (ev & 1u << i)
            std::fprintf(stderr, "%s%s", n++ ? "|" : "", event_names[i]);
    std::fprintf(stderr, ", end_reason::%s },%s\n", end_names[(uint32_t)end], note);
}

void check_combos(const combos& c)
{
    combos expected;
    for (const auto& x : expected_combos)
        expected.seen[(size_t)x.e][x.events][(size_t)x.end] = true;
    bool ok = true;
    for (auto e = 0u; e < (uint32_t)entry::COUNT; e++)
        for (auto ev = 0u; ev < ev_COUNT; ev++)
            for (auto end = 0u; end < (uint32_t)end_reason::COUNT; end++)
                ok &= c.seen[e][ev][end] == expected.seen[e][ev][end];
    if (ok)
        return;
    std::fputs("critter-walk: combinations seen (+ unlisted, - listed but not seen):\n", stderr);
    for (auto e = 0u; e < (uint32_t)entry::COUNT; e++)
        for (auto ev = 0u; ev < ev_COUNT; ev++)
            for (auto end = 0u; end < (uint32_t)end_reason::COUNT; end++)
                if (c.seen[e][ev][end] || expected.seen[e][ev][end])
                    print_combo(entry(e), ev, end_reason(end),
                                c.seen[e][ev][end] == expected.seen[e][ev][end] ? "" : c.seen[e][ev][end] ? " +" : " -");
    fm_abort("critter-walk: the product's combinations differ from the table");
}

void run_random(product& P, uint32_t count)
{
    uint64_t seed = 0x9e3779b97f4a7c15;
    const auto next = [&](uint32_t n) {
        seed = seed * 6364136223846793005u + 1442695040888963407u;
        return uint32_t(seed >> 33) % n;
    };
    for (auto i = 0u; i < count; i++)
    {
        const auto e = entry(next(3));
        const bool off = e != entry::update_movement && next(3) == 0;
        const auto r = rotation(next(8));
        const auto d = off ? off_octant[next(4)] : direction{vec_of(r), r, r, true};
        const auto box = next((uint32_t)array_size(boxes));
        const auto s = Math::sign(d.vec);
        const auto pl = place(next(d.unit && is_diagonal(d.leg_r) ? 3 : 2));
        const auto px = start_px(pl, s, major_of(d.vec));
        const auto start = origin + px;
        const auto vec = d.unit ? d.vec * 40 : d.vec;

        auto w = world();
        for (int16_t y = -1; y <= 1; y++)
            for (int16_t x = -1; x <= 1; x++)
                (void)w[chunk_coords_{x, y, 0}];
        for (auto k = 0u, n = 1 + next(3); k < n; k++)
        {
            const auto lo = px + vec * (int32_t)next(41) / 40 + Vector2i{(int32_t)next(61), (int32_t)next(61)} - Vector2i{30};
            add_rect(w, {lo.x(), lo.x() + 1 + (int32_t)next(40)}, {lo.y(), lo.y() + 1 + (int32_t)next(40)});
        }
        object_id id = 0;
        auto proto = Run::make_proto(1.f);
        proto.bbox_offset = boxes[box].offset;
        proto.bbox_size = boxes[box].size;
        auto& C = *w.ensure_player_character(id, move(proto));
        Run::mark_all_modified(w);

        const auto& info = C.atlas->info();
        const auto m = model{w, skip_self{C.id}, object::frame_duration(info.fps), P.line_cost({1, 1})};
        const auto wk = walk{e, e == entry::move_toward_line ? d.line_r : d.leg_r, start, start + vec, 40,
                             e == entry::move_toward_line ? P.line_cost(vec) : Ns{}};
        const auto cd = case_desc{wk, rotation(next(8)), box, layout{level::none}, pl, time_mode::whole, (int32_t)i};
        (void)run_walk(m, C, cd, nullptr, [&](const walker_state& st, const walk_outcome& o) {
            switch (next(4))
            {
            case 0: return m.frame;
            case 1: return m.frame * 3u;
            case 2: return remaining_cost(m, wk, st, o) + m.frame / 2u;
            default: return Ns{1 + next(uint32_t(m.frame.stamp * 3))};
            }
        });
    }
}

} // namespace

// Walks a critter through update_movement() and both move_toward() overloads and compares every call
// with a model that moves one pixel at a time. Each obstacle level states how the walk ends, so a
// fixture mistake fails instead of repeating another level. The product must reach exactly the
// combinations of events in expected_combos.
void Test::test_critter_walk()
{
    product P;
    run_product(P);
    check_combos(P.seen);
    run_random(P, 200);
}

} // namespace floormat
