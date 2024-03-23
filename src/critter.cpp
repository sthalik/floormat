#include "critter.hpp"
#include "compat/limits.hpp"
#include "tile-constants.hpp"
#include "src/anim-atlas.hpp"
#include "loader/loader.hpp"
#include "src/world.hpp"
#include "src/object.hpp"
#include "shaders/shader.hpp"
#include "compat/exception.hpp"
#include <cmath>
#include <utility>
#include <algorithm>
#include <mg/Functions.h>

namespace floormat {

namespace {

constexpr auto arrows_to_dir(bool left, bool right, bool up, bool down)
{
    constexpr unsigned L = 1 << 3, R = 1 << 2, U = 1 << 1, D = 1 << 0;
    const unsigned bits = left*L | right*R | up*U | down*D;
    constexpr unsigned mask = L|R|U|D;
    CORRADE_ASSUME((bits & mask) == bits);

    switch (bits)
    {
    default: std::unreachable(); // -Wswitch-default
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
    return array[(size_t)r];
}

constexpr std::array<rotation, 3> rotation_to_similar(rotation r)
{
    CORRADE_ASSUME(r < rotation_COUNT);
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
    default:
        std::unreachable();
        fm_assert(false);
    }
}

template<rotation new_r, float vx, float vy>
CORRADE_ALWAYS_INLINE
bool update_movement_body(size_t& i, critter& C, const anim_def& info)
{
    constexpr auto vec = Vector2{vx, vy};
    using Frac = decltype(critter::offset_frac_);
    constexpr auto frac = (float{limits<Frac>::max}+1)/2;
    constexpr auto inv_frac = 1 / frac;
    const auto from_accum = C.offset_frac_ * inv_frac * vec;
    auto offset_ = vec + from_accum;
    auto off_i = Vector2i(offset_);
    if (!off_i.isZero())
    {
        auto rem = Math::fmod(offset_, 1.f).length();
        C.offset_frac_ = Frac(rem * frac);
        if (C.can_move_to(off_i))
        {
            C.move_to(i, off_i, new_r);
            ++C.frame %= info.nframes;
            return true;
        }
    }
    else
    {
        auto rem = offset_.length();
        C.offset_frac_ = Frac(rem * frac);
        return true;
    }
    return false;
}

template<rotation r>
CORRADE_NEVER_INLINE
bool update_movement_2(size_t& index, critter& C, const anim_def& info)
{
    constexpr auto vec = rotation_to_vec(r);
    return update_movement_body<r, vec.x(), vec.y()>(index, C, info);
}

template<rotation r>
CORRADE_ALWAYS_INLINE
bool update_movement_3way(size_t i, critter& C, const anim_def& info)
{
    constexpr auto rotations = rotation_to_similar(r);
    if (update_movement_2<rotations[0]>(i, C, info))
        return true;
    if (update_movement_2<rotations[1]>(i, C, info))
        return true;
    if (update_movement_2<rotations[2]>(i, C, info))
        return true;
    return false;
}

template<rotation new_r>
CORRADE_NEVER_INLINE
bool update_movement_1(critter& C, size_t& i, const anim_def& info, uint32_t nframes)
{
    constexpr bool Diagonal = (int)new_r & 1;
    if constexpr(Diagonal)
    {
        for (auto k = 0u; k < nframes; k++)
            if (!update_movement_3way<new_r>(i, C, info))
                return false;
    }
    else
    {
        constexpr auto vec = rotation_to_vec(new_r);
        for (auto k = 0u; k < nframes; k++)
            if (!update_movement_body<new_r, vec.x(), vec.y()>(i, C, info))
                return false;
    }

    return true;
}

template bool update_movement_1<(rotation)0>(critter& C, size_t& i, const anim_def& info, uint32_t nframes);
template bool update_movement_1<(rotation)1>(critter& C, size_t& i, const anim_def& info, uint32_t nframes);
template bool update_movement_1<(rotation)2>(critter& C, size_t& i, const anim_def& info, uint32_t nframes);
template bool update_movement_1<(rotation)3>(critter& C, size_t& i, const anim_def& info, uint32_t nframes);
template bool update_movement_1<(rotation)4>(critter& C, size_t& i, const anim_def& info, uint32_t nframes);
template bool update_movement_1<(rotation)5>(critter& C, size_t& i, const anim_def& info, uint32_t nframes);
template bool update_movement_1<(rotation)6>(critter& C, size_t& i, const anim_def& info, uint32_t nframes);
template bool update_movement_1<(rotation)7>(critter& C, size_t& i, const anim_def& info, uint32_t nframes);

} // namespace

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
    return name == s0.name && playable == s0.playable;
}

void critter::set_keys(bool L, bool R, bool U, bool D)
{
    movement = { L, R, U, D, movement.AUTO, false, false, false };
}

void critter::set_keys_auto()
{
    movement = { false, false, false, false, true, false, false, false };
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

void critter::update(size_t i, const Ns& dt)
{
    if (playable) [[unlikely]]
    {
        movement.AUTO &= !(movement.L | movement.R | movement.U | movement.D);

        if (!movement.AUTO)
        {
            const auto new_r = arrows_to_dir(movement.L, movement.R, movement.U, movement.D);
            if (new_r == rotation_COUNT)
            {
                offset_frac_ = {};
                delta = 0;
            }
            else
                update_movement(i, dt, new_r);
        }
    }
    else
        update_nonplayable(i, dt);
}

void critter::update_nonplayable(size_t i, const Ns& dt)
{
    (void)i; (void)dt; (void)playable;
}

void critter::update_movement(size_t i, const Ns& dt, rotation new_r)
{
    fm_assert(new_r < rotation_COUNT);
    fm_assert(is_dynamic());

    const auto& info = atlas->info();
    const auto nframes = alloc_frame_time(dt, delta, info.fps, speed);
    if (nframes == 0)
        return;

    if (r != new_r)
        rotate(i, new_r);
    c->ensure_passability();

    bool ret;

    switch (new_r)
    {
    default: std::unreachable();
    case (rotation)0: ret = update_movement_1<(rotation)0>(*this, i, info, nframes); break;
    case (rotation)1: ret = update_movement_1<(rotation)1>(*this, i, info, nframes); break;
    case (rotation)2: ret = update_movement_1<(rotation)2>(*this, i, info, nframes); break;
    case (rotation)3: ret = update_movement_1<(rotation)3>(*this, i, info, nframes); break;
    case (rotation)4: ret = update_movement_1<(rotation)4>(*this, i, info, nframes); break;
    case (rotation)5: ret = update_movement_1<(rotation)5>(*this, i, info, nframes); break;
    case (rotation)6: ret = update_movement_1<(rotation)6>(*this, i, info, nframes); break;
    case (rotation)7: ret = update_movement_1<(rotation)7>(*this, i, info, nframes); break;
    }

    if (!ret) [[unlikely]]
    {
        delta = {};
        offset_frac_ = {};
    }
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

} // namespace floormat
