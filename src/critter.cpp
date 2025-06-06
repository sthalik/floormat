#include "critter.hpp"
#include "critter-script.hpp"
#include "tile-constants.hpp"
#include "src/point.inl"
#include "src/anim-atlas.hpp"
#include "loader/loader.hpp"
#include "src/world.hpp"
#include "src/object.hpp"
#include "src/nanosecond.inl"
#include "shaders/shader.hpp"
#include "compat/limits.hpp"
#include "compat/map.hpp"
#include "compat/iota.hpp"
#include "compat/exception.hpp"
#include "compat/borrowed-ptr.inl"
#include <cmath>
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
    constexpr double c = critter::move_speed * critter::frame_time;
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

constexpr auto rotation_to_similar_array = map(rotation_to_similar_, iota_array<rotation, (size_t)rotation_COUNT>);

constexpr std::array<rotation, 3> rotation_to_similar(rotation r)
{
    return rotation_to_similar_array.data()[(uint8_t)r];
}

constexpr uint8_t get_length_axisʹ(rotation r)
{
    using enum rotation;
    if (r == N || r == S)
        return 1;
    else if (r == W || r == E)
        return 0;
    else
        return 0; // garbage in, garbage out
}

constexpr uint8_t get_length_axis(rotation r)
{
    constexpr auto table = map(get_length_axisʹ, iota_array<rotation, (size_t)rotation_COUNT>);
    return table.data()[(size_t)r];
}

template<bool MultiStep>
CORRADE_NEVER_INLINE
bool update_movement_body(size_t& i, critter& C, const anim_def& info, uint8_t nsteps, rotation new_r, rotation visible_r)
{
    const auto vec = rotation_to_vec(new_r);
    using Frac = decltype(critter::offset_frac);
    constexpr auto frac = (float{limits<Frac>::max}+1)/2;
    constexpr auto inv_frac = 1 / frac;
    const auto from_accum = C.offset_frac * inv_frac * vec;

    Vector2 offset_{NoInit};
    if constexpr(MultiStep)
        offset_ = vec * float(nsteps) + from_accum;
    else
        offset_ = vec + from_accum;

    auto off_i = Vector2i(offset_);
    if (!off_i.isZero())
    {
        auto rem = Math::fmod(offset_, 1.f).length();
        C.offset_frac = Frac(rem * frac);
        if (C.can_move_to(off_i))
        {
            C.move_to(i, off_i, visible_r);
            if constexpr(MultiStep)
                (C.frame += nsteps) %= info.nframes;
            else
                ++C.frame %= info.nframes;
            return true;
        }
    }
    else
    {
        auto rem = offset_.length();
        C.offset_frac = Frac(rem * frac);
        return true;
    }
    return false;
}

bool update_movement_3way(size_t& i, critter& C, const anim_def& info, rotation new_r)
{
    const auto rotations = rotation_to_similar(new_r);
    if (update_movement_body<false>(i, C, info, 0, rotations.data()[0], new_r))
        return true;
    if (update_movement_body<false>(i, C, info, 0, rotations.data()[1], new_r))
        return true;
    if (update_movement_body<false>(i, C, info, 0, rotations.data()[2], new_r))
        return true;
    return false;
}

constexpr bool DoUnroll = true;

template<bool IsEven>
requires (!IsEven)
bool update_movement_1(critter& C, size_t& i, const anim_def& info, uint32_t nframes, rotation new_r)
{
    //Debug{} << "< nframes" << nframes;
    if constexpr(DoUnroll)
    {
        while (nframes > 1)
        {
            auto len = (uint8_t)Math::min(nframes, (uint32_t)C.bbox_size.min());
            if (len <= 1)
                break;
            if (!update_movement_body<true>(i, C, info, len, new_r, new_r))
                break;
            //Debug{} << " " << len;
            nframes -= len;
        }
    }
    //Debug{} << ">" << nframes;

    for (auto k = 0u; k < nframes; k++)
        if (!update_movement_3way(i, C, info, new_r))
            return false;
    return true;
}

template<bool IsEven>
requires IsEven
bool update_movement_1(critter& C, size_t& i, const anim_def& info, uint32_t nframes, rotation new_r)
{
    if constexpr(DoUnroll)
    {
        //Debug{} << "< nframes" << nframes;
        while (nframes > 1)
        {
            const auto len_axis = get_length_axis(new_r);
            auto len = (uint8_t)Math::min(nframes, (uint32_t)C.bbox_size.data()[len_axis]);
            if (len <= 1) [[unlikely]]
                break;
            if (!update_movement_body<true>(i, C, info, len, new_r, new_r))
                break;
            //Debug{} << " " << len;
            nframes -= len;
        }
        //Debug{} << ">" << nframes;
    }

    for (auto k = 0u; k < nframes; k++)
        if (!update_movement_body<false>(i, C, info, 0, new_r, new_r))
            return false;
    return true;
}

struct step_s
{
    uint32_t count;
    Vector2b direction;
};

constexpr step_s next_step_(Vector2i vec_in)
{
    const auto vec = Vector2ui(Math::abs(vec_in));
    const auto signs = Vector2b(Math::sign(vec_in));

    if (vec.x() == vec.y())
        return { vec.x(), Vector2b{1, 1} * signs };
    else if (vec.y() == 0)
        return { vec.x(), Vector2b{1, 0} * signs };
    else if (vec.x() == 0)
        return { vec.y(), Vector2b{0, 1} * signs };
    else
    {
        uint32_t major_idx, minor_idx;
        if (vec.x() > vec.y())
        {
            major_idx = 0;
            minor_idx = 1;
        }
        else
        {
            major_idx = 1;
            minor_idx = 0;
        }
        const auto major = vec[major_idx], minor = vec[minor_idx];
        const auto num_axis_aligned = (uint32_t)Math::abs((int)major - (int)minor);
        auto axis_aligned = Vector2b{};
        axis_aligned[major_idx] = 1;
        return { num_axis_aligned, axis_aligned * signs };
    }
}

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

    if (step.direction.isZero()) [[unlikely]]
        return rotation_COUNT;

    auto x = uint8_t(step.direction.x() + 1);
    auto y = uint8_t(step.direction.y() + 1);
    //fm_debug_assert((x & 3) == x && (y & 3) == y);
    auto val = uint8_t(x << 2 | y);
    return table.data()[val];
}

constexpr step_s next_step(point from, point to)
{
    fm_debug_assert(from.chunk3().z == to.chunk3().z);
    const auto vec = to - from;
    fm_debug_assert(!vec.isZero());
    return next_step_(vec);
}

constexpr float step_magnitude(Vector2b vec)
{
    constexpr double cʹ = critter::move_speed * critter::frame_time;
    constexpr double dʹ = cʹ / Vector2d{1,  1}.length();
    constexpr auto c = (float)cʹ, d = (float)dʹ;

    if (vec.x() * vec.y() != 0)
        // diagonal
        return d;
    else
        // axis-aligned
        return c;
}

Ns return_unspent_dt(uint32_t nframes, uint32_t i, float speed, Ns frame_duration)
{
    return Ns{(uint64_t)((float)(uint64_t)((nframes - i) * frame_duration) / speed)};
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
    return name == s0.name && Math::abs(speed - s0.speed) < 1e-8f && playable == s0.playable;
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

float critter::depth_offset() const
{
    return tile_shader::character_depth_offset;
}

Vector2 critter::ordinal_offset(Vector2b offset) const
{
    (void)offset;
    return Vector2(offset);
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

    bool ret;

    switch (new_r)
    {
    using enum rotation;
    default: std::unreachable();
    case N: case E: case S: case W:
        ret = update_movement_1<true >(*this, i, info, nframes, new_r); break;
    case NW: case NE: case SE: case SW:
        ret = update_movement_1<false>(*this, i, info, nframes, new_r); break;
    }

    if (!ret) [[unlikely]]
    {
        delta = {};
        offset_frac = {};
    }
}

auto critter::move_toward(size_t& index, Ns& dt, const point& dest) -> move_result
{
    fm_assert(is_dynamic());

    const auto& info = atlas->info();
    const auto anim_frames = info.nframes;
    const auto hz = info.fps;
    constexpr auto ns_in_sec = Ns((int)1e9);
    const auto frame_duration = ns_in_sec / hz;
    const auto nframes = alloc_frame_time(dt, delta, hz, speed);
    dt = Ns{};
    bool moved = false;

    if (nframes == 0)
        return { .blocked = false, .moved = moved };

    bool ok = true;

    for (uint32_t i = 0; i < nframes; i++)
    {
        chunk().ensure_passability();

        const auto from = position();
        if (from == dest)
        {
            //Debug{} << "done!" << from;
            //C.set_keys(false, false, false, false);
            dt = return_unspent_dt(nframes, i, speed, frame_duration);
            return { .blocked = false, .moved = moved, };
        }
        const auto step = next_step(from, dest);
        //Debug{} << "step" << step.direction << step.count << "|" << C.position();
        fm_assert(step.direction != Vector2b{} && step.count > 0);
        const auto new_r = dir_from_step(step);
        using Frac = decltype(critter::offset_frac);
        constexpr auto frac = (float{limits<Frac>::max}+1)/2;
        constexpr auto inv_frac = 1 / frac;
        const auto mag = step_magnitude(step.direction);
        const auto vec = Vector2(step.direction) * mag;
        const auto from_accum = offset_frac * inv_frac * vec;
        auto offset_ = vec + from_accum;
        auto off_i = Vector2i(offset_);
        //Debug{} << "vec" << vec << "mag" << mag << "off_i" << off_i << "offset_" << C.offset_frac_;

        if (!off_i.isZero())
        {
            auto rem = Math::fmod(offset_, 1.f).length();
            offset_frac = Frac(rem * frac);
            //Debug{} << "foo1" << C.offset_frac_;
            if (can_move_to(off_i))
            {
                move_to(index, off_i, new_r);
                moved = true;
                ++frame %= anim_frames;
            }
            else
            {
                dt = return_unspent_dt(nframes, i, speed, frame_duration);
                ok = false;
                break;
            }
        }
        else
        {
            auto rem = offset_.length();
            offset_frac = Frac(rem * frac);
        }
    }

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
    playable{proto.playable}
{
    if (!name)
        name = "(Unnamed)"_s;
    fm_soft_assert(atlas->check_rotation(r));
    fm_soft_assert(speed >= 0);
    object::set_bbox_(offset, bbox_offset, Vector2ub(iTILE_SIZE2/2), pass);
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
