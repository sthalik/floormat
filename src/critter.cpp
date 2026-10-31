#include "critter.hpp"
#include "critter-script.hpp"
#include "src/point.inl"
#include "src/anim-atlas.hpp"
#include "loader/loader.hpp"
#include "src/world.hpp"
#include "src/object.hpp"
#include "src/rotation.inl"
#include "src/sweep-aabb.hpp"
#include "src/search.hpp"
#include "src/raycast-line.hpp"
#include "src/nanosecond.inl"
#include "compat/map.hpp"
#include "compat/iota.hpp"
#include "compat/exception.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/function2.hpp"
#include <array>
#include <cr/ArrayView.h>
#include <mg/BatchFunctions.h>

namespace floormat {

template class basic_bptr<critter, non_atomic_refcount>;
template class basic_bptr<const critter, non_atomic_refcount>;

namespace {

constexpr auto m_auto_mask = critter::move_u { .bits {.AUTO = true} };

constexpr rotation arrows_to_dir_from_mask(unsigned mask)
{
    constexpr unsigned L = 1 << 3, R = 1 << 2, U = 1 << 1, D = 1 << 0;
    switch (mask)
    {
    using enum rotation;
    case L | U: return W;
    case L | D: return S;
    case R | U: return N;
    case R | D: return E;
    case L: return SW;
    case D: return SE;
    case R: return NE;
    case U: return NW;
    case L|(U|D): return SW;
    case R|(U|D): return NE;
    case U|(L|R): return NW;
    case D|(L|R): return SE;
    case 0:
    // degenerate case
    case L|R|D|U:
    case D|U:
    case L|R:
        return rotation{rotation_COUNT};
    }
    fm_assert(false);
}

constexpr auto arrows_to_dir(bool left, bool right, bool up, bool down)
{
    constexpr auto table = map(arrows_to_dir_from_mask, iota_array<uint8_t, 16>);
    constexpr uint8_t L = 1 << 3, R = 1 << 2, U = 1 << 1, D = 1 << 0;
    constexpr uint8_t mask = L|R|U|D;

    const uint8_t bits = left*L | right*R | up*U | down*D;
    CORRADE_ASSUME((bits & mask) == bits);
    return table.data()[bits];
}

#if 0
static_assert(arrows_to_dir(true, false, false, false) == rotation::SW);
static_assert(arrows_to_dir(true, false, true, true) == rotation::SW);
static_assert(arrows_to_dir(true, false, true, false) == rotation::W);
static_assert(arrows_to_dir(false, true, false, true) == rotation::E);
static_assert(arrows_to_dir(false, false, true, false) == rotation::NW);
static_assert(arrows_to_dir(false, false, false, false) == rotation_COUNT);
static_assert(arrows_to_dir(true, true, true, true) == rotation_COUNT);
static_assert(arrows_to_dir(true, true, false, false) == rotation_COUNT);
#endif

constexpr Vector2i rotation_to_vec(rotation r)
{
    constexpr Vector2i array[8] = {
        Vector2i{ 0, -1}, Vector2i{ 1, -1}, Vector2i{ 1,  0}, Vector2i{ 1,  1},
        Vector2i{ 0,  1}, Vector2i{-1,  1}, Vector2i{-1,  0}, Vector2i{-1, -1},
    };

    CORRADE_ASSUME(r < rotation_COUNT);
    return array[(uint8_t)r];
}

constexpr uint8_t is_diagonal(rotation r) { return (uint8_t)r & 1; }

// floor(sqrt(x)), digit by digit
constexpr uint64_t isqrt(uint64_t x)
{
    uint64_t res = 0, bit = uint64_t{1} << 62;
    while (bit > x)
        bit >>= 2;
    while (bit != 0)
    {
        if (x >= res + bit)
        {
            x -= res + bit;
            res = (res >> 1) + bit;
        }
        else
            res >>= 1;
        bit >>= 2;
    }
    return res;
}

// Returns the length of a step of a line with slope minor/major in 1/anim_speed_unit px, rounded down by under 2.
constexpr uint32_t step_length(uint32_t minor, uint32_t major)
{
    const auto q = uint64_t{minor} * anim_speed_unit / major;
    return (uint32_t)isqrt(uint64_t{anim_speed_unit} * anim_speed_unit + q * q);
}

static_assert(step_length(0, 1) == anim_speed_unit && step_length(1, 1) == 92681);

constexpr uint32_t step_length(rotation r)
{
    constexpr uint32_t lengths[2] = { step_length(0, 1), step_length(1, 1) };
    return lengths[is_diagonal(r)];
}

// Returns floor(frame * len / anim_speed_unit) without forming frame * len, which overflows for frame_duration(0).
constexpr Ns step_duration(Ns frame, uint32_t len)
{
    const auto f = frame.stamp;
    return Ns{f / anim_speed_unit * len + f % anim_speed_unit * len / anim_speed_unit};
}

struct step_costs
{
    Ns costs[2];

    explicit step_costs(Ns frame) : costs{frame, step_duration(frame, step_length(1, 1))} {}
    Ns frame() const { return costs[0]; }
    Ns operator()(rotation r) const { return costs[is_diagonal(r)]; }
};

constexpr std::array<rotation, 3> rotation_to_similar_(rotation r)
{
    switch (r)
    {
    using enum rotation;
    case N:  return {  N, NW, NE };
    case NE: return { NE,  N,  E };
    case E:  return {  E, NE, SE };
    case SE: return { SE,  E,  S };
    case S:  return {  S, SE, SW };
    case SW: return { SW,  S,  W };
    case W:  return {  W, SW, NW };
    case NW: return { NW,  W,  N };
    }
    fm_assert(false);
}

constexpr std::array<rotation, 3> rotation_to_similar(rotation r)
{
    constexpr auto array = map(rotation_to_similar_, iota_array<rotation, (size_t)rotation_COUNT>);
    return array.data()[(uint8_t)r];
}

// Keeps the box within the 3x3 chunks that Search::is_passable_() searches.
constexpr uint64_t max_run = chunk_size_xy / 2;

struct run_s
{
    point pos;
    rotation r;
    uint32_t nsteps;
    // whether C turns to r at pos before moving
    bool turn;
};

struct free_steps_result
{
    uint32_t steps;
    // Set when the run turns and C's box, turned to r at pos, overlaps a collider other than C's own.
    bool turn_blocked;
    // For a diagonal run cut short, whether a 1 px move along x, and along y, from its last free step is blocked.
    bool slide_blocked[2];
};

// For each run, returns the steps C's box takes before its first blocked step, as find_swept_collider() would.
// C's own collider is skipped, so the answer for runs[1] still holds after C moves along runs[0].
std::array<free_steps_result, 2> free_steps(critter& C, ArrayView<const run_s> runs)
{
    fm_debug_assert(!runs.isEmpty() && runs.size() <= 2);
    const auto ch = C.coord.chunk3();
    const auto self_id = C.id;

    struct state
    {
        Vector2i from, d;
        // a box spans [center - lo, center + hi), as scenery_tile()
        Vector2i lo, hi;
        int32_t nsteps;
        // the first blocked step, or nsteps + 1
        int32_t step;
        bool diagonal, turn, turn_blocked, slide_blocked[2];
    };
    state st[2];
    auto min = Vector2i{limits<int32_t>::max}, max = Vector2i{limits<int32_t>::min};
    for (auto i = 0u; i < runs.size(); i++)
    {
        const auto& run = runs[i];
        const auto [offset, size] = run.r == C.r ? Pair<Vector2b, Vector2ub>{C.bbox_offset, C.bbox_size}
                                                 : rotate_bbox(C.bbox_offset, C.bbox_size, C.r, run.r);
        const auto from = run.pos - point{ch, {}, {}} + Vector2i(offset);
        const auto d = rotation_to_vec(run.r), to = from + d * int32_t(run.nsteps);
        const auto lo = Vector2i(size/2), hi = Vector2i(size) - lo;
        // can_move_to() lets an object that passes through others turn anywhere
        const bool turn = run.turn && C.pass != pass_mode::pass;
        st[i] = { from, d, lo, hi, int32_t(run.nsteps), int32_t(run.nsteps) + 1, bool(is_diagonal(run.r)), turn, false, {} };
        min = Math::min(min, Math::min(from, to) - lo);
        max = Math::max(max, Math::max(from, to) + hi);
    }

    // Returns the first and last step whose box overlaps [r0, r1) on axis i, or first > last for none.
    const auto steps = [](const state& s, uint32_t i, int32_t r0, int32_t r1) {
        const auto c = s.from[i];
        struct { int32_t first, last; } k;
        if (s.d[i] > 0)
            k = { r0 - c - s.hi[i] + 1, r1 - c + s.lo[i] };
        else if (s.d[i] < 0)
            k = { c - s.lo[i] - r1 + 1, c + s.hi[i] - r0 };
        else if (c - s.lo[i] < r1 && r0 < c + s.hi[i])
            k = { 1, s.nsteps };
        else
            k = { 1, 0 };
        return k;
    };

    const auto add = [&](state& s, Vector2i r0, Vector2i r1) {
        if (s.turn && (s.from - s.lo < r1).all() && (r0 < s.from + s.hi).all())
            s.turn_blocked = true;
        const auto kx = steps(s, 0, r0.x(), r1.x()), ky = steps(s, 1, r0.y(), r1.y());
        const auto k0 = Math::max(Math::max(kx.first, ky.first), 1);
        // A rect first blocking at s.step itself can still block a slide that earlier rects left free.
        const auto k1 = Math::min(Math::min(kx.last, ky.last), s.step);
        if (k0 <= k1)
        {
            // The 1 px move along x from step k0 - 1 is blocked when the box there already overlaps the rect on y.
            const bool bx = ky.first < k0, by = kx.first < k0;
            if (k0 < s.step)
            {
                s.step = k0;
                s.slide_blocked[0] = bx;
                s.slide_blocked[1] = by;
            }
            else
            {
                s.slide_blocked[0] |= bx;
                s.slide_blocked[1] |= by;
            }
        }
    };

    const auto pred = [&](class chunk& c, collision_data x, Range2D rect) {
        if (x.id == self_id)
            return path_search_continue::pass;
        // is_passable_() passes rect in c's frame
        const auto off = Vector2i(c.coord().x - ch.x, c.coord().y - ch.y) * chunk_size<int32_t>;
        const auto r0 = Vector2i(rect.min()) + off, r1 = Vector2i(rect.max()) + off;
        for (auto i = 0u; i < runs.size(); i++)
            add(st[i], r0, r1);
        // Later runs matter only when run 0 is free, so the search stops once no rect can change run 0's answer.
        const auto& s = st[0];
        const bool done = s.turn_blocked ||
                          s.step == 1 && !s.turn && (!s.diagonal || s.slide_blocked[0] && s.slide_blocked[1]);
        return done ? path_search_continue::blocked : path_search_continue::pass;
    };
    Search::is_passable_(&C.chunk(), C.world().neighbors(ch), Vector2(min), Vector2(max), pred);

    std::array<free_steps_result, 2> ret{};
    for (auto i = 0u; i < runs.size(); i++)
        ret[i] = { uint32_t(st[i].step - 1), st[i].turn_blocked, { st[i].slide_blocked[0], st[i].slide_blocked[1] } };
    return ret;
}

// step_len is in 1/anim_speed_unit px. Each step rounds down alone, so one call equals nsteps calls for 1 step.
void advance_anim_frames(critter& C, uint32_t nsteps, uint32_t step_len, uint32_t nframes_atlas)
{
    fm_debug_assert(C.anim_speed >= 0 && C.anim_speed <= 64);
    // anim_speed_unit is a power of two, so the float product is exact and only the conversion rounds.
    const auto speed = uint64_t(C.anim_speed * float(anim_speed_unit));
    const auto adv = nsteps * (speed * step_len / anim_speed_unit);
    const auto total = uint64_t(C.anim_progress) + adv;
    const auto whole = uint32_t(total / anim_speed_unit);
    C.anim_progress = uint32_t(total % anim_speed_unit);
    C.frame = uint16_t((uint32_t(C.frame) + whole) % nframes_atlas);
}

void move_steps(size_t& i, critter& C, const anim_def& info, uint32_t nsteps, rotation new_r, rotation visible_r)
{
    [[maybe_unused]] const bool moved = C.move_to(i, rotation_to_vec(new_r) * int(nsteps), visible_r);
    fm_debug_assert(moved);
    advance_anim_frames(C, nsteps, step_length(new_r), info.nframes);
}

// Moves C along new_r while ticks last, sliding 1 px along x or y when a diagonal step is blocked.
// Returns false when C is blocked or can't turn to new_r.
bool update_movement_1(critter& C, size_t& i, const anim_def& info, Ns& ticks, const step_costs& cost, rotation new_r)
{
    const auto step = cost(new_r);
    for (bool turn = C.r != new_r; turn || ticks >= step; turn = false)
    {
        const auto n = (uint32_t)Math::min(max_run, ticks / step);
        const run_s run{C.position(), new_r, n, turn};
        const auto res = free_steps(C, {&run, 1})[0];
        if (turn)
        {
            if (res.turn_blocked)
                return false;
            C.rotate(i, new_r);
        }
        const auto k = res.steps;
        if (k > 0)
        {
            move_steps(i, C, info, k, new_r, new_r);
            ticks -= step * k;
        }
        if (k == n)
            continue;
        // Coords are whole pixels, so whatever blocks a cardinal step also blocks both diagonal steps next to it.
        if (!is_diagonal(new_r))
            return false;
        // Since k < n, ticks still pay for a diagonal step, and a slide costs less.
        const auto alt = rotation_to_similar(new_r);
        const auto a1 = uint32_t(rotation_to_vec(alt.data()[1]).x() == 0);
        rotation e;
        if (!res.slide_blocked[a1])
            e = alt.data()[1];
        else if (!res.slide_blocked[a1 ^ 1])
            e = alt.data()[2];
        else
            return false;
        move_steps(i, C, info, 1, e, new_r);
        ticks -= cost(e);
    }
    return true;
}

struct step_s
{
    uint32_t count;
    Vector2b direction;
};

constexpr rotation dir_from_step_mask(uint8_t val)
{
    switch (val)
    {
    using enum rotation;
    case 0 << 2 | 0: /* -1 -1 */ return NW;
    case 0 << 2 | 1: /* -1  0 */ return W;
    case 0 << 2 | 2: /* -1  1 */ return SW;
    case 1 << 2 | 0: /*  0 -1 */ return N;
    case 1 << 2 | 1: /*  0  0 */ return rotation_COUNT;
    case 1 << 2 | 2: /*  0  1 */ return S;
    case 2 << 2 | 0: /*  1 -1 */ return NE;
    case 2 << 2 | 1: /*  1  0 */ return E;
    case 2 << 2 | 2: /*  1  1 */ return SE;
    default: return rotation_COUNT;
    }
}

constexpr rotation dir_from_step(step_s step)
{
    constexpr auto table = map(dir_from_step_mask, iota_array<uint8_t, 1 << 4>);
    auto x = uint8_t(step.direction.x() + 1);
    auto y = uint8_t(step.direction.y() + 1);
    //fm_debug_assert((x & 3) == x && (y & 3) == y);
    auto val = uint8_t(x << 2 | y);
    return table.data()[val];
}

constexpr step_s next_step(point from, point to)
{
    // Returns one segment of an octant-style walk from `from` to `to`. The walk is
    // decomposed "axis-aligned first, diagonal second": when |x| != |y| the call
    // returns the axis-aligned step count needed to equalise the two magnitudes;
    // the next call (on the now-diagonal remainder) returns the diagonal segment.
    //
    //   x-dominant   |x| >  |y|    step=(sx,  0)    count = |x| - |y|
    //   y-dominant   |y| >  |x|    step=( 0, sy)    count = |y| - |x|
    //   xy-equal     |x| == |y|    step=(sx, sy)    count = |x|         (== |y|)
    //
    // sx, sy are per-axis signs of vec_in (each in {-1, 0, +1}). Pure-axis walks
    // (vec.x()==0 or vec.y()==0) are the dominant case with min_xy=0, so the
    // subtraction is a no-op.

    fm_debug_assert(from.chunk3().z == to.chunk3().z);
    const auto vec_in = to - from;
    fm_debug_assert(!vec_in.isZero());

    const auto vec = Vector2ui(Math::abs(vec_in));
    const auto signs = Vector2b(Math::sign(vec_in));

    const bool x_ge_y = vec.x() >= vec.y();
    const bool y_ge_x = vec.y() >= vec.x();
    const auto step = Vector2b{(signed char)x_ge_y, (signed char)y_ge_x} * signs;

    const auto max_xy = Math::max(vec.x(), vec.y());
    const auto min_xy = Math::min(vec.x(), vec.y());
    // xor is 1 iff exactly one ge holds (single-axis dominant); 0 if both (x==y diagonal)
    const uint32_t count = max_xy - min_xy * uint32_t(x_ge_y ^ y_ge_x);
    return { count, step };
}

// Returns the step whose pixel is px, or -1u when the line misses px.
uint32_t step_at(const rc::bresenham_line& line, Vector2i px)
{
    const auto x = line.interval_bounds_to_steps(0, px.x(), px.x()), y = line.interval_bounds_to_steps(1, px.y(), px.y());
    const auto k = Math::max(x.first, y.first);
    return k <= Math::min(x.last, y.last) ? k : -1u;
}

Ns return_unspent_dt(const Ns& ticks, float speed)
{
    constexpr auto max_ns = double(uint64_t{1} << 53);
    return Ns{(uint64_t)Math::min((double)ticks.stamp / (double)speed, max_ns)};
}

} // namespace

// Diagonal when minor/major > tan 22.5° = √2 - 1, that is (minor + major)² > 2·major².
rotation nearest_rotation(Vector2i vec)
{
    fm_debug_assert(!vec.isZero());
    const auto a = Vector2ui(Math::abs(vec));
    const uint64_t major = Math::max(a.x(), a.y()), minor = Math::min(a.x(), a.y());
    const bool diagonal = (minor + major) * (minor + major) > 2 * major * major;
    const bool x_major = a.x() >= a.y();
    const auto step = Vector2b{(signed char)(x_major || diagonal), (signed char)(!x_major || diagonal)};
    return dir_from_step({1, step * Vector2b(Math::sign(vec))});
}

extern template class Script<critter, critter_script>;

critter_proto::critter_proto(const critter_proto&) = default;
critter_proto::~critter_proto() noexcept = default;
critter_proto& critter_proto::operator=(const critter_proto&) = default;

critter_proto::critter_proto()
{
    type = object_type::critter;
    atlas = loader.anim_atlas("npc-walk", loader.ANIM_PATH);
}

critter_proto::critter_proto(object_proto base) : object_proto{move(base)} {}

bool critter_proto::operator==(const object_proto& e0) const
{
    if (type != e0.type)
        return false;

    if (!object_proto::operator==(e0))
        return false;

    const auto& s0 = static_cast<const critter_proto&>(e0);
    return name == s0.name && Math::abs(speed - s0.speed) < 1e-8f
           && Math::abs(anim_speed - s0.anim_speed) < 1e-8f && playable == s0.playable;
}

void critter::set_keys(bool L, bool R, bool U, bool D) { moves = { L, R, U, D, moves.AUTO, }; }
void critter::set_keys_auto() { moves_ = m_auto_mask.val; }
void critter::clear_auto_movement() { moves_ &= ~m_auto_mask.val; }
bool critter::maybe_stop_auto_movement()
{
    bool b1 = moves_ == m_auto_mask.val;
    bool b2 = moves.AUTO &= b1;
    return !b2;
}

Vector2ui critter::circumscribed_bbox_size() const
{
    return floormat::circumscribed_bbox_size(bbox_offset, bbox_size, r);
}

int32_t critter::depth_offset() const
{
    //return tile_shader::character_depth_offset;
    return 0;
}

void critter::update(const bptr<object>& ptrʹ, size_t& i, const Ns& dt)
{
    fm_debug_assert(&*ptrʹ == this);

    check_script_update_1(script.state());
    script->on_update(static_pointer_cast<critter>(ptrʹ), i, dt);
#if 0 // for now, objects can't delete themselves
    if (check_script_update_2(script.state())) [[unlikely]]
        return;
#endif
    if (playable) [[unlikely]]
    {
        if (!moves.AUTO)
        {
            const auto new_r = arrows_to_dir(moves.L, moves.R, moves.U, moves.D);
            if (new_r == rotation_COUNT)
                delta = 0;
            else
                update_movement(i, dt, new_r);
        }
    }
}

void critter::update_movement(size_t& i, const Ns& dt, rotation new_r)
{
    const auto& info = atlas->info();
    const auto cost = step_costs{atlas->frame_duration()};
    auto ticks = alloc_ticks(dt, delta, cost.frame(), speed);
    if (ticks < cost.frame())
    {
        delta = ns_to_delta(ticks);
        return;
    }

    fm_assert(new_r < rotation_COUNT);
    fm_assert(is_dynamic());
    c->ensure_passability();

    if (update_movement_1(*this, i, info, ticks, cost, new_r)) [[likely]]
        delta = ns_to_delta(ticks);
    else
        delta = {};
}

auto critter::move_toward(size_t& index, Ns& dt, const point& dest) -> move_result
{
    fm_assert(bbox_size.x() && bbox_size.y());

    if (speed == 0) [[unlikely]]
    {
        dt = Ns{};
        return {.blocked = position() != dest, .moved = false};
    }

    const auto& info = atlas->info();
    const auto cost = step_costs{atlas->frame_duration()};
    auto ticks = alloc_ticks(dt, delta, cost.frame(), speed);
    dt = Ns{};
    bool moved = false;

    const auto blocked = [&] {
        set_keys(false, false, false, false);
        dt = return_unspent_dt(ticks, speed);
        delta = {};
        return move_result{ .blocked = true, .moved = moved };
    };

    for (;;)
    {
        chunk().ensure_passability();

        const auto from = position();
        if (from == dest)
        {
            // all of it goes back as dt, so delta mustn't count it again
            dt = return_unspent_dt(ticks, speed);
            delta = {};
            return { .blocked = false, .moved = moved, };
        }
        // Up to two legs share one free_steps() query, so together they take at most max_run steps.
        run_s legs[2];
        uint32_t nlegs = 0;
        auto pos = from;
        auto left = ticks;
        auto budget = max_run;
        for (auto facing = r; nlegs < 2 && pos != dest; )
        {
            const auto step = next_step(pos, dest);
            fm_assert(step.direction != Vector2b{} && step.count > 0);
            const auto leg_r = dir_from_step(step);
            const auto nsteps = (uint32_t)Math::min({left / cost(leg_r), uint64_t{step.count}, budget});
            if (nsteps == 0)
                break;
            legs[nlegs++] = { pos, leg_r, nsteps, leg_r != facing };
            if (nsteps < step.count)
                break;
            pos = pos + Vector2i(step.direction) * int32_t(nsteps);
            left -= cost(leg_r) * nsteps;
            budget -= nsteps;
            facing = leg_r;
        }
        if (nlegs == 0)
            break;
        fm_assert(is_dynamic());

        const auto res = free_steps(*this, {legs, nlegs});
        for (auto j = 0u; j < nlegs; j++)
        {
            const auto& leg = legs[j];
            if (leg.turn)
            {
                if (res[j].turn_blocked)
                    return blocked();
                rotate(index, leg.r);
            }
            const auto k = res[j].steps;
            if (k > 0)
            {
                move_steps(index, *this, info, k, leg.r, leg.r);
                ticks -= cost(leg.r) * k;
                moved = true;
            }
            if (k < leg.nsteps)
                return blocked();
        }
    }

    delta = ns_to_delta(ticks);
    return { .blocked = false, .moved = moved };
}

auto critter::move_toward(size_t& index, Ns& dt, const point& from, const point& dest) -> move_result
{
    fm_assert(bbox_size.x() && bbox_size.y());
    fm_assert(from.chunk3().z == dest.chunk3().z);

    const auto ch = from.chunk3();
    const auto origin = point{ch, {}, {}};
    const auto line = rc::bresenham_line{from - origin, dest - origin};
    const auto steps = line.steps();
    const auto pos = position();
    const auto k = pos.chunk3().z == ch.z ? step_at(line, pos - origin) : -1u;

    const auto stop = [&] {
        set_keys(false, false, false, false);
        delta = {};
    };

    if (k == -1u) [[unlikely]]
    {
        stop();
        return { .blocked = true, .moved = false, .displaced = true };
    }
    if (k == steps)
        return { .blocked = false, .moved = false };
    if (speed == 0) [[unlikely]]
    {
        dt = Ns{};
        return { .blocked = true, .moved = false };
    }

    const auto& info = atlas->info();
    const auto frame_ns = atlas->frame_duration();
    auto ticks = alloc_ticks(dt, delta, frame_ns, speed);
    const auto step_len = step_length(line.minor_len(), steps);
    const auto step_cost = step_duration(frame_ns, step_len);
    const auto n = (uint32_t)Math::min(uint64_t{steps - k}, ticks / step_cost);
    if (n == 0)
    {
        dt = Ns{};
        delta = ns_to_delta(ticks);
        return { .blocked = false, .moved = false };
    }
    fm_assert(is_dynamic());

    // Before dt is taken, so a failed turn leaves it to the caller.
    const auto new_r = nearest_rotation(dest - from);
    if (r != new_r)
    {
        if (!can_rotate(new_r))
        {
            stop();
            return { .blocked = true, .moved = false };
        }
        rotate(index, new_r);
    }
    dt = Ns{};

    // after rotate(), so the box is the one for new_r
    const auto bb_off = Vector2i(bbox_offset);
    const auto box_line = rc::bresenham_line{from - origin + bb_off, dest - origin + bb_off};
    const auto self_id = id;
    const auto pred = [self_id](class chunk&, collision_data x, Range2D) {
        return x.id == self_id ? path_search_continue::pass : path_search_continue::blocked;
    };
    const auto res = find_swept_collider(world(), ch, box_line, k, k + n, Vector2ui(bbox_size), pred);

    const auto reached = res.step > k ? res.step - 1 : k;
    ticks -= step_cost * (reached - k);
    const bool done = res.has_collider || reached == steps;
    if (reached > k)
    {
        teleport_to(index, origin + line.point_at(reached), new_r);
        advance_anim_frames(*this, reached - k, step_len, info.nframes);
    }

    if (!done)
    {
        delta = ns_to_delta(ticks);
        return { .blocked = false, .moved = true };
    }
    dt = return_unspent_dt(ticks, speed);
    delta = {};
    if (res.has_collider)
    {
        stop();
        return { .blocked = true, .moved = reached > k };
    }
    return { .blocked = false, .moved = true };
}

object_type critter::type() const noexcept { return object_type::critter; }

critter::operator critter_proto() const
{
    critter_proto ret{object::operator object_proto()};
    ret.name = name;
    ret.speed = speed;
    ret.anim_speed = anim_speed;
    ret.playable = playable;
    return ret;
}

critter::critter(object_id id, class chunk& c, critter_proto proto) :
    object{id, c, proto},
    name{move(proto.name)},
    speed{proto.speed},
    anim_speed{proto.anim_speed},
    playable{proto.playable}
{
    if (!name)
        name = "(Unnamed)"_s;
    fm_soft_assert(atlas->check_rotation(r));
    fm_soft_assert(speed >= 0);
    fm_assert(bbox_size.x() && bbox_size.y());
}

critter::~critter() noexcept
{
}

void critter::init_script(const bptr<object>& ptrʹ)
{
    script.do_initialize(static_pointer_cast<critter>(ptrʹ));
}

void critter::destroy_script_pre(const bptr<object>& ptrʹ, script_destroy_reason r)
{
    script.do_destroy_pre(static_pointer_cast<critter>(ptrʹ), r);
}

void critter::destroy_script_post()
{
    script.do_finish_destroy();
}

} // namespace floormat
