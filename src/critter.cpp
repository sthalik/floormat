#include "critter.hpp"
#include "critter-script.hpp"
#include "src/point.inl"
#include "src/anim-atlas.hpp"
#include "loader/loader.hpp"
#include "src/world.hpp"
#include "src/object.hpp"
#include "src/sweep-aabb.hpp"
#include "src/raycast-line.hpp"
#include "src/nanosecond.inl"
#include "shaders/shader.hpp"
#include "compat/map.hpp"
#include "compat/iota.hpp"
#include "compat/exception.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/function2.hpp"
#include <utility>
#include <array>
#include <numbers>
#include <mg/Functions.h>
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

// Diagonals are the odd rotations. Indexing by the low bit keeps these lookups branchless.
constexpr uint8_t is_diagonal(rotation r) { return (uint8_t)r & 1; }

constexpr float step_length(rotation r)
{
    constexpr float lengths[2] = { 1, std::numbers::sqrt2_v<float> };
    return lengths[is_diagonal(r)];
}

struct step_costs
{
    Ns costs[2];

    explicit step_costs(uint32_t hz) : costs{Second / hz, (Second / hz) * std::numbers::sqrt2} {}
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

Range2D critter_bbox_local(const critter& C)
{
    const auto center = Vector2(C.coord.local())*TILE_SIZE2 + Vector2(C.offset) + Vector2(C.bbox_offset);
    const auto min = center - Vector2(C.bbox_size/2);
    return Range2D{min, min + Vector2(C.bbox_size)};
}

sweep_result sweep_critter(critter& C, Vector2 displacement)
{
    const auto self_id = C.id;
    auto pred = [self_id](class chunk&, collision_data x, Range2D) {
        return x.id == self_id ? path_search_continue::pass : path_search_continue::blocked;
    };
    return find_swept_collider(C.chunk(), critter_bbox_local(C), displacement, pred);
}

uint32_t free_steps(critter& C, rotation r, uint32_t nsteps)
{
    const auto ch = C.coord.chunk3();
    const auto from = C.position() - point{ch, {}, {}} + Vector2i(C.bbox_offset);
    const auto line = rc::bresenham_line{from, from + rotation_to_vec(r) * int(nsteps)};
    const auto self_id = C.id;
    const auto pred = [self_id](class chunk&, collision_data x, Range2D) {
        return x.id == self_id ? path_search_continue::pass : path_search_continue::blocked;
    };
    const auto res = find_swept_collider(C.world(), ch, line, 0, nsteps, Vector2ui(C.bbox_size), pred);
    return res.step > 0 ? res.step - 1 : 0;
}

void warn_line_sweep_mismatch(const critter& C, rotation r)
{
    static bool once = false;
    if (once)
        return;
    once = true;
    WARN_nospace << "warning: line sweep blocked critter " << C.id << " at " << C.position()
                 << " moving " << (int)r << ", but the step is free. Walking in single steps.";
}

void advance_anim_frames(critter& C, float length, float anim_speed, uint32_t nframes_atlas)
{
    fm_debug_assert(anim_speed >= 0 && anim_speed <= 64);
    const auto adv = uint64_t(length * anim_speed * float(anim_speed_unit));
    const auto total = uint64_t(C.anim_progress) + adv;
    const auto whole = uint32_t(total / anim_speed_unit);
    C.anim_progress = uint32_t(total % anim_speed_unit);
    C.frame = uint16_t((uint32_t(C.frame) + whole) % nframes_atlas);
}

bool move_steps(size_t& i, critter& C, const anim_def& info, uint32_t nsteps, rotation new_r, rotation visible_r)
{
    // move_to() can refuse the implied rotation (non-square bbox against an obstacle)
    if (!C.move_to(i, rotation_to_vec(new_r) * int(nsteps), visible_r)) [[unlikely]]
        return false;

    advance_anim_frames(C, float(nsteps) * step_length(new_r), C.anim_speed, info.nframes);
    return true;
}

bool update_movement_body(size_t& i, critter& C, const anim_def& info, uint32_t nsteps, rotation new_r, rotation visible_r)
{
    if (sweep_critter(C, Vector2(rotation_to_vec(new_r) * int(nsteps))).has_collider)
        return false;
    return move_steps(i, C, info, nsteps, new_r, visible_r);
}

bool update_movement_alternatives(size_t& i, critter& C, const anim_def& info, Ns& ticks, const step_costs& cost, rotation new_r)
{
    // rotations[0] == new_r; the caller just tried that, skip it
    const auto rotations = rotation_to_similar(new_r);
    // both are diagonal for a cardinal new_r, both cardinal for a diagonal one
    const auto alt_cost = cost(rotations.data()[1]);
    // Only a cardinal new_r has dearer alternatives. A diagonal step can't get past a collider the
    // straight one hit, so the time isn't kept for them.
    if (ticks < alt_cost)
        return false;
    for (auto k : { 1u, 2u })
    {
        if (update_movement_body(i, C, info, 1, rotations.data()[k], new_r))
        {
            ticks -= alt_cost;
            return true;
        }
    }
    return false;
}

bool update_movement_1(critter& C, size_t& i, const anim_def& info, Ns& ticks, const step_costs& cost, rotation new_r)
{
    constexpr uint64_t max_run = chunk_size_xy / 2;
    const auto step = cost(new_r);
    uint64_t max = max_run;
    bool slid = false, check = false;
    while (ticks >= step)
    {
        const auto n = (uint32_t)Math::min(max, ticks / step);
        if (n > 1)
        {
            const auto k = free_steps(C, new_r, n);
            if (k > 0 && !move_steps(i, C, info, k, new_r, new_r)) [[unlikely]]
            {
                max = 1;
                continue;
            }
            ticks -= step * k;
            if (k > 0 && slid)
            {
                max = max_run;
                slid = false;
            }
            if (k < n)
            {
                // Step k + 1 is blocked. The single step below sweeps it again, so a wrong line
                // sweep costs one warning and single steps for the rest of the update.
                max = 1;
                check = true;
            }
            continue;
        }
        if (update_movement_body(i, C, info, 1, new_r, new_r))
        {
            if (check) [[unlikely]]
                warn_line_sweep_mismatch(C, new_r);
            check = false;
            ticks -= step;
            if (slid)
            {
                max = max_run;
                slid = false;
            }
            continue;
        }
        check = false;
        if (!update_movement_alternatives(i, C, info, ticks, cost, new_r))
            return false;
        slid = true;
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

// Diagonal when minor/major > tan 22.5° = √2 - 1, that is (minor + major)² > 2·major².
// Integers never tie.
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

// The step whose pixel is px, or -1u. Along the major axis one step has px's coordinate, along
// the minor axis a range of them.
uint32_t step_at(const rc::bresenham_line& line, Vector2i px)
{
    const auto x = line.steps_within(0, px.x(), px.x()), y = line.steps_within(1, px.y(), px.y());
    const auto k = Math::max(x.first, y.first);
    return k <= Math::min(x.last, y.last) ? k : -1u;
}

Ns return_unspent_dt(const Ns& ticks, float speed)
{
    // A tiny speed would overflow the cast. 2^53 ns is the bound alloc_ticks() asserts on.
    constexpr auto max_ns = double(uint64_t{1} << 53);
    return Ns{(uint64_t)Math::min((double)ticks.stamp / (double)speed, max_ns)};
}

} // namespace

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
    const auto cost = step_costs{info.fps};
    auto ticks = alloc_ticks(dt, delta, info.fps, speed);
    if (ticks < cost.frame())
    {
        delta = ns_to_delta(ticks);
        return;
    }

    fm_assert(new_r < rotation_COUNT);
    fm_assert(is_dynamic());

    if (r != new_r)
    {
        if (!can_rotate(new_r)) [[unlikely]]
        {
            delta = {};
            return;
        }
        rotate(i, new_r);
    }
    c->ensure_passability();

    fm_debug2_assert(new_r < rotation_COUNT);
    if (update_movement_1(*this, i, info, ticks, cost, new_r)) [[likely]]
        delta = ns_to_delta(ticks);
    else
        delta = {};
}

auto critter::move_toward(size_t& index, Ns& dt, const point& dest) -> move_result
{
    fm_assert(is_dynamic());
    fm_assert(bbox_size.x() && bbox_size.y());

    if (speed == 0) [[unlikely]]
    {
        dt = Ns{};
        return {.blocked = position() != dest, .moved = false};
    }

    const auto& info = atlas->info();
    const auto cost = step_costs{info.fps};
    auto ticks = alloc_ticks(dt, delta, info.fps, speed);
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
        const auto step = next_step(from, dest);
        //Debug{} << "step" << step.direction << step.count << "|" << C.position();
        fm_assert(step.direction != Vector2b{} && step.count > 0);
        const auto new_r = dir_from_step(step);
        const auto step_cost = cost(new_r);
        // Max steps for the operation to avoid overshooting
        constexpr uint64_t len_limit = tile_size_xy;
        const auto nsteps = (uint32_t)Math::min({ticks / step_cost, uint64_t{step.count}, len_limit});
        if (nsteps == 0)
            break;

        if (r != new_r)
        {
            if (!can_rotate(new_r))
                return blocked();
            rotate(index, new_r);
        }
        const auto off_i = rotation_to_vec(new_r) * int(nsteps);
        if (sweep_critter(*this, Vector2(off_i)).has_collider || !move_to(index, off_i, new_r))
            return blocked();
        ticks -= step_cost * nsteps;
        moved = true;
        advance_anim_frames(*this, float(nsteps) * step_length(new_r), anim_speed, info.nframes);
    }

    delta = ns_to_delta(ticks);
    return { .blocked = false, .moved = moved };
}

auto critter::move_toward(size_t& index, Ns& dt, const point& from, const point& dest) -> move_result
{
    fm_assert(is_dynamic());
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

    const auto& info = atlas->info();
    auto ticks = alloc_ticks(dt, delta, info.fps, speed);
    dt = Ns{};
    const auto step_len = Vector2(dest - from).length() / float(steps);
    const auto step_cost = (Second / info.fps) * double(step_len);
    const auto n = (uint32_t)Math::min(uint64_t{steps - k}, ticks / step_cost);
    if (n == 0)
    {
        delta = ns_to_delta(ticks);
        return { .blocked = false, .moved = false };
    }

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
        teleport_to(index, origin + line.pixel_at(reached), new_r);
        advance_anim_frames(*this, float(reached - k) * step_len, anim_speed, info.nframes);
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
