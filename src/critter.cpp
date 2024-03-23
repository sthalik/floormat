#include "critter.hpp"
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
    movement = { L, R, U, D, false };
}

void critter::set_keys_auto()
{
    movement = { false, false, false, false, true };
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
    if (playable)
    {
        if (!movement.AUTO)
        {
            const auto new_r = arrows_to_dir(movement.L, movement.R, movement.U, movement.D);
            if (new_r == rotation_COUNT)
            {
                offset_frac = {};
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

    const auto hz = atlas->info().fps;
    const auto nframes = alloc_frame_time(dt, delta, hz, speed);
    if (nframes == 0)
        return;

    const auto rotations = rotation_to_similar(new_r);
    const unsigned nvecs = (int)new_r & 1 ? 3 : 1;
    if (r != new_r)
        //if (is_dynamic())
            rotate(i, new_r);

    c->ensure_passability();
    bool can_move = false;

    for (auto k = 0u; k < nframes; k++)
    {
        for (unsigned j = 0; j < nvecs; j++)
        {
            const auto vec = rotation_to_vec(rotations[j]);
            constexpr auto frac = 65535u;
            constexpr auto inv_frac = 1.f / (float)frac;
            const auto sign_vec = Math::sign(vec);
            auto offset_ = vec + Vector2(offset_frac) * sign_vec * inv_frac;
            auto off_i = Vector2i(offset_);
            if (!off_i.isZero())
            {
                offset_frac = Vector2us(Math::abs(Math::fmod(offset_, 1.f)) * frac);
                if (can_move_to(off_i))
                {
                    can_move = true;
                    move_to(i, off_i, new_r);
                    ++frame %= atlas->info().nframes;
                    break;
                }
            }
            else
            {
                can_move = true;
                offset_frac = Vector2us(Math::min({1.f,1.f}, Math::abs(offset_)) * frac);
                break;
            }
        }
    }

    if (!can_move) [[unlikely]]
    {
        delta = {};
        offset_frac = {};
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
