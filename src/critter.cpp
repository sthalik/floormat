#include "critter.hpp"
#include "critter-script.hpp"
#include "src/point.inl"
#include "src/anim-atlas.hpp"
#include "loader/loader.hpp"
#include "src/world.hpp"
#include "src/object.hpp"
#include "src/sweep-aabb.hpp"
#include "src/nanosecond.inl"
#include "shaders/shader.hpp"
#include "compat/limits.hpp"
#include "compat/map.hpp"
#include "compat/iota.hpp"
#include "compat/exception.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/function2.hpp"
#include <utility>
#include <array>
#include <mg/Functions.h>

namespace floormat {

template class bptr<critter>;
template class bptr<const critter>;

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

constexpr Vector2 rotation_to_vec(rotation r)
{
    constexpr double c = 1.0;
    constexpr double d = c / Vector2d{1,  1}.length();

    constexpr Vector2 array[8] = {
        Vector2(Vector2d{ 0, -1} * c),
        Vector2(Vector2d{ 1, -1} * d),
        Vector2(Vector2d{ 1,  0} * c),
        Vector2(Vector2d{ 1,  1} * d),
        Vector2(Vector2d{ 0,  1} * c),
        Vector2(Vector2d{-1,  1} * d),
        Vector2(Vector2d{-1,  0} * c),
        Vector2(Vector2d{-1, -1} * d),
    };

    CORRADE_ASSUME(r < rotation_COUNT);
    return array[(uint8_t)r];
}

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
    const auto half_bbox = Vector2(C.bbox_size)*0.5f;
    return Range2D{center - half_bbox, center + half_bbox};
}

sweep_result sweep_critter(critter& C, Vector2 displacement)
{
    const auto self_id = C.id;
    auto pred = [self_id](class chunk&, collision_data x, Range2D) {
        return x.id == self_id ? path_search_continue::pass : path_search_continue::blocked;
    };
    return find_swept_collider(C.chunk(), critter_bbox_local(C), displacement, pred);
}

enum class step_result : uint8_t { blocked, moved, accumulated };

void advance_anim_frames(critter& C, uint32_t nsteps, float anim_speed, uint32_t nframes_atlas)
{
    fm_debug_assert(anim_speed >= 0 && anim_speed <= 64);
    const auto adv = uint64_t(float(nsteps) * anim_speed * float(anim_speed_unit));
    const auto total = uint64_t(C.anim_progress) + adv;
    const auto whole = uint32_t(total / anim_speed_unit);
    C.anim_progress = uint32_t(total % anim_speed_unit);
    C.frame = uint16_t((uint32_t(C.frame) + whole) % nframes_atlas);
}

// offset_frac: scalar magnitude of pending sub-pixel motion.
// update_movement clears on stuck so alternatives()' alt-direction isZero writes don't leak.
step_result update_movement_body(size_t& i, critter& C, const anim_def& info, uint32_t nsteps, rotation new_r, rotation visible_r)
{
    const auto vec = rotation_to_vec(new_r);
    using Frac = decltype(critter::offset_frac);
    constexpr auto frac = (float{limits<Frac>::max}+1)/2;
    constexpr auto inv_frac = 1 / frac;
    const auto from_accum = C.offset_frac * inv_frac * vec;
    const auto offset_ = vec * float(nsteps) + from_accum;
    const auto off_i = Vector2i(offset_);

    if (off_i.isZero())
    {
        const auto rem = offset_.length();
        C.offset_frac = Frac(rem * frac);
        return step_result::accumulated;
    }

    // blocked: leave offset_frac untouched so the caller can try an alternative direction
    if (sweep_critter(C, Vector2(off_i)).has_collider)
        return step_result::blocked;

    const auto rem = Math::fmod(offset_, 1.f).length();
    C.offset_frac = Frac(rem * frac);
    C.move_to(i, off_i, visible_r);
    advance_anim_frames(C, nsteps, C.anim_speed, info.nframes);
    return step_result::moved;
}

// Only `moved` counts: accepting `accumulated` would let a cardinal critter stuck
// at a wall drift diagonally via offset_frac reinterpreted into the alt direction.
bool update_movement_alternatives(size_t& i, critter& C, const anim_def& info, rotation new_r)
{
    // rotations[0] == new_r; the caller just tried that, skip it
    const auto rotations = rotation_to_similar(new_r);
    if (update_movement_body(i, C, info, 1, rotations.data()[1], new_r) == step_result::moved)
        return true;
    if (update_movement_body(i, C, info, 1, rotations.data()[2], new_r) == step_result::moved)
        return true;
    return false;
}

bool update_movement_1(critter& C, size_t& i, const anim_def& info, uint32_t nframes, rotation new_r)
{
    while (nframes > 0)
    {
        constexpr uint32_t max = TILE_MAX_DIM * tile_size_xy / 2;
        auto cur = Math::min(max, nframes);
        if (update_movement_body(i, C, info, cur, new_r, new_r) != step_result::blocked)
        {
            nframes -= cur;
            continue;
        }
        if (!update_movement_alternatives(i, C, info, new_r))
            return false;
        nframes--;
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

Ns return_unspent_dt(uint32_t nframes, float speed, Ns frame_duration)
{
    return Ns{(uint64_t)((double)(uint64_t)(nframes * frame_duration) / (double)speed)};
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
            {
                offset_frac = {};
                delta = 0;
            }
            else
                update_movement(i, dt, new_r);
        }
    }
}

void critter::update_movement(size_t& i, const Ns& dt, rotation new_r)
{
    const auto& info = atlas->info();
    const auto nframes = alloc_frame_time(dt, delta, info.fps, speed);
    if (nframes == 0)
        return;

    fm_assert(new_r < rotation_COUNT);
    fm_assert(is_dynamic());

    if (r != new_r)
        rotate(i, new_r);
    c->ensure_passability();

    fm_debug2_assert(new_r < rotation_COUNT);
    const bool ret = update_movement_1(*this, i, info, nframes, new_r);

    if (!ret) [[unlikely]]
    {
        delta = {};
        offset_frac = {};
    }
}

auto critter::move_toward(size_t& index, Ns& dt, const point& dest) -> move_result
{
    fm_assert(is_dynamic());
    fm_assert(bbox_size.x() && bbox_size.y());

    if (speed == 0) [[unlikely]]
        return {.blocked = position() != dest, .moved = false};

    const auto& info = atlas->info();
    const auto hz = info.fps;
    constexpr auto ns_in_sec = Ns((int)1e9);
    const auto frame_duration = ns_in_sec / hz;
    auto nframes = alloc_frame_time(dt, delta, hz, speed);
    dt = Ns{};
    bool moved = false;

    if (nframes == 0)
        return { .blocked = false, .moved = moved };

    bool ok = true;

    while (nframes > 0)
    {
        chunk().ensure_passability();

        const auto from = position();
        if (from == dest)
        {
            //Debug{} << "done!" << from;
            //C.set_keys(false, false, false, false);
            dt = return_unspent_dt(nframes, speed, frame_duration);
            offset_frac = {};
            return { .blocked = false, .moved = moved, };
        }
        const auto step = next_step(from, dest);
        //Debug{} << "step" << step.direction << step.count << "|" << C.position();
        fm_assert(step.direction != Vector2b{} && step.count > 0);
        const auto new_r = dir_from_step(step);
        // Max steps for the operation to avoid overshooting
        constexpr uint32_t len_limit = tile_size_xy;
        const auto nsteps = (uint8_t)Math::min({nframes, step.count, len_limit});
        fm_assert(nsteps > 0);
        using Frac = decltype(critter::offset_frac);
        constexpr auto frac = (float{limits<Frac>::max}+1)/2;
        constexpr auto inv_frac = 1 / frac;
        const auto vec = rotation_to_vec(new_r);
        const auto from_accum = (float)offset_frac * inv_frac * vec;
        const auto offset_ = vec * float(nsteps) + from_accum;
        // Clamp to movement budget consumed this iteration
        const auto abs_limit = Vector2i(Math::abs(step.direction)) * int(nsteps);
        const auto off_i = Math::clamp(Vector2i(offset_), -abs_limit, abs_limit);
        //Debug{} << "vec" << vec << "mag" << mag << "off_i" << off_i << "offset_" << C.offset_frac_;
        const auto rem_vec = offset_ - Vector2(off_i);
        const auto rem_frac = Math::clamp(rem_vec.length() * frac, 0.f, float(limits<Frac>::max));
        offset_frac = Frac(rem_frac);

        //DBG << "nsteps" << nsteps;
        nframes -= nsteps;

        if (!off_i.isZero())
        {
            //Debug{} << "foo1" << C.offset_frac_;
            if (!sweep_critter(*this, Vector2(off_i)).has_collider)
            {
                move_to(index, off_i, new_r);
                moved = true;
                advance_anim_frames(*this, nsteps, anim_speed, info.nframes);
            }
            else
            {
                ok = false;
                break;
            }
        }
    }

    dt = return_unspent_dt(nframes, speed, frame_duration);

    // todo return unused movement frames into the offset_frac pool

    if (!ok) [[unlikely]]
    {
        //Debug{} << "bad";
        set_keys(false, false, false, false);
        delta = {};
        offset_frac = {};
        return { .blocked = true, .moved = moved };
    }

    return { .blocked = false, .moved = moved };
}

object_type critter::type() const noexcept { return object_type::critter; }

critter::operator critter_proto() const
{
    critter_proto ret;
    static_cast<object_proto&>(ret) = object::operator object_proto();
    ret.name = name;
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
