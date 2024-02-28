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
#include <Magnum/Math/Functions.h>

namespace floormat {

namespace {

constexpr double framerate = 60, move_speed = 60;
constexpr double frame_time = 1/framerate;

constexpr auto arrows_to_dir(bool left, bool right, bool up, bool down)
{
    if (left == right)
        left = right = false;
    if (up == down)
        up = down = false;

    const auto bits = unsigned(left << 3 | right << 2 | up << 1 | down << 0);
    constexpr unsigned L = 1 << 3, R = 1 << 2, U = 1 << 1, D = 1 << 0;
    CORRADE_ASSUME(bits <= 0xff);

    switch (bits)
    {
    using enum rotation;
    case 0: return rotation{rotation_COUNT};
    case L | U: return W;
    case L | D: return S;
    case R | U: return N;
    case R | D: return E;
    case L: return SW;
    case D: return SE;
    case R: return NE;
    case U: return NW;
    }
    std::unreachable();
    fm_assert(false);
}

constexpr Vector2 rotation_to_vec(rotation r)
{
    constexpr double c = move_speed * frame_time;

    CORRADE_ASSUME(r < rotation_COUNT);

    constexpr double d = c / Vector2d{1,  1}.length();
    switch (r)
    {
    using enum rotation;
    case NE: return Vector2(Vector2d{ 1, -1} * d);
    case SE: return Vector2(Vector2d{ 1,  1} * d);
    case SW: return Vector2(Vector2d{-1,  1} * d);
    case NW: return Vector2(Vector2d{-1, -1} * d);
    case N:  return Vector2(Vector2d{ 0, -1} * c);
    case E:  return Vector2(Vector2d{ 1,  0} * c);
    case S:  return Vector2(Vector2d{ 0,  1} * c);
    case W:  return Vector2(Vector2d{-1,  0} * c);
    }
    std::unreachable();
    fm_assert(false);
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
    }
    std::unreachable();
    fm_assert(false);
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

int critter::allocate_frame_time(float dt)
{
    auto d = (double)delta / 65535. + (double)dt;
    d = std::min(1., d);
    auto ret = (int)(d / frame_time);
    d -= ret;
    d = Math::clamp(d, 0., 1.);
    delta = (uint16_t)(d * 65535);
    return ret;
}

void critter::set_keys(bool L, bool R, bool U, bool D)
{
    b_L = L;
    b_R = R;
    b_U = U;
    b_D = D;
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

void critter::update(size_t i, float dt)
{
    if (playable)
        update_playable(i, dt);
    else
        update_nonplayable(i, dt);
}

void critter::update_nonplayable(size_t i, float dt)
{
    (void)i; (void)dt; (void)playable;
}

void critter::update_playable(size_t i, float dt)
{
    const auto new_r = arrows_to_dir(b_L, b_R, b_U, b_D);
    if (new_r == rotation_COUNT)
    {
        offset_frac = {};
        delta = 0;
        return;
    }

    int nframes = allocate_frame_time(dt * speed);
    if (nframes == 0)
        return;

    const auto rotations = rotation_to_similar(new_r);

    const unsigned nvecs = (int)new_r & 1 ? 3 : 1;

    if (r != new_r)
        if (is_dynamic())
            rotate(i, new_r);

    c->ensure_passability();

    for (int k = 0; k < nframes; k++)
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
                    move_to(i, off_i, new_r);
                    ++frame %= atlas->info().nframes;
                    break;
                }
            }
            else
            {
                offset_frac = Vector2us(Math::abs(Math::min({1.f,1.f}, offset_)) * frac);
                break;
            }
        }
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

critter::critter(object_id id, class chunk& c, const critter_proto& proto) :
    object{id, c, proto},
    name{proto.name},
    speed{proto.speed},
    playable{proto.playable}
{
    if (!name)
        name = "(Unnamed)"_s;
    fm_soft_assert(atlas->check_rotation(r));
    object::set_bbox_(offset, bbox_offset, Vector2ub(iTILE_SIZE2/2), pass);
}

} // namespace floormat
