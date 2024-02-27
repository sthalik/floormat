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

constexpr double framerate = 96 * 3, move_speed = Vector2d(TILE_SIZE2).length() * 4.25;
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

constexpr Vector2i rotation_to_vec(rotation r)
{
    CORRADE_ASSUME(r < rotation_COUNT);
    switch (r)
    {
    using enum rotation;
    case N:  return {  0, -1 };
    case NE: return {  1, -1 };
    case E:  return {  1,  0 };
    case SE: return {  1,  1 };
    case S:  return {  0,  1 };
    case SW: return { -1,  1 };
    case W:  return { -1,  0 };
    case NW: return { -1, -1 };
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
    d = std::max(0., d);
    delta = (uint16_t)(d * 65535);
    return ret;
}

constexpr Vector2 move_vec(Vector2i vec)
{
    const int left_right = vec[0], top_bottom = vec[1];
    constexpr auto c = (float)move_speed * (float)frame_time;
    auto dir = Vector2((float)Math::sign(left_right), (float)Math::sign(top_bottom));
    auto inv_norm = 1.f/dir.length();
    return c * dir * inv_norm;
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

constexpr auto make_move_vec(rotation r)
{
    auto [_0, _1, _2] = rotation_to_similar(r);
    return std::array<Vector2, 3>{{
        move_vec(rotation_to_vec(_0)),
        move_vec(rotation_to_vec(_1)),
        move_vec(rotation_to_vec(_2)),
    }};
}

template<size_t... Index>
constexpr auto make_move_vecs(std::index_sequence<Index...>)
{
    return std::array<std::array<Vector2, 3>, (size_t)rotation_COUNT>{{
        make_move_vec((rotation)Index)...,
    }};
}

void critter::update(size_t i, float dt)
{
    const auto new_r = arrows_to_dir(b_L, b_R, b_U, b_D);
    if (new_r == rotation_COUNT)
    {
        offset_frac = {};
        delta = 0;
        return;
    }

    int nframes = allocate_frame_time(dt);
    if (nframes == 0)
        return;

    constexpr auto move_vecs_ = make_move_vecs(std::make_index_sequence<(size_t)rotation_COUNT>{});
    const auto& move_vecs = move_vecs_[(size_t)r];
    size_t nvecs = (int)new_r & 1 ? 3 : 1;

    if (r != new_r)
        if (is_dynamic())
            rotate(i, new_r);

    c->ensure_passability();

    for (int k = 0; k < nframes; k++)
    {
        for (auto j = 0uz; j < nvecs; j++)
        {
            auto vec = move_vecs[j];
            constexpr auto frac = 65535u;
            constexpr auto inv_frac = 1.f / (float)frac;
            const auto sign_vec = Vector2(Math::sign(vec.x()), Math::sign(vec.y()));
            auto offset_ = vec + Vector2(offset_frac) * sign_vec * inv_frac;
            offset_frac = Vector2us(Vector2(std::fabs(std::fmod(offset_.x(), 1.f)),
                                            std::fabs(std::fmod(offset_.y(), 1.f))) * frac);
            auto off_i = Vector2i(offset_);
            if (!off_i.isZero())
            {
                if (can_move_to(off_i))
                {
                    move_to(i, off_i, new_r);
                    ++frame %= atlas->info().nframes;
                    goto done;
                }
            }
        }
done:
        (void)0;
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
