#include "critter.hpp"
#include "src/anim-atlas.hpp"
#include "loader/loader.hpp"
#include "src/world.hpp"
#include "src/object.hpp"
#include "shaders/shader.hpp"
#include "compat/exception.hpp"
#include "compat/math.hpp"
#include <cmath>
#include <memory>
#include <utility>
#include <algorithm>

namespace floormat {

namespace {

template <typename T> constexpr T sgn(T val) { return T(T(0) < val) - T(val < T(0)); }

constexpr auto vector_length(Vector2 vec)
{
    return math::sqrt(Math::dot(vec, vec));
};

constexpr float framerate = 96 * 3, move_speed = vector_length(TILE_SIZE2) * 4.25f;
constexpr float frame_time = 1/framerate;

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
    int d = int(delta) + int(65535 * dt);
    constexpr int framerate_ = 65535/framerate;
    static_assert(framerate_ > 0);
    auto ret = d / framerate_;
    delta = (uint16_t)std::clamp(d - ret*65535LL, 0LL, 65535LL);
    return ret;
}

Vector2 critter::move_vec(Vector2i vec)
{
    const int left_right = vec[0], top_bottom = vec[1];
    constexpr auto c = move_speed * frame_time;
    return c * Vector2((float)sgn(left_right), (float)sgn(top_bottom)).normalized();
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
    const auto new_r = arrows_to_dir(b_L, b_R, b_U, b_D);
    if (new_r == rotation{rotation_COUNT})
    {
        delta = 0;
        return;
    }

    int nframes = allocate_frame_time(dt);

    if (nframes == 0)
        return;

    auto [_0, _1, _2] = rotation_to_similar(r);
    const Vector2 move_vecs[] = {
        move_vec(rotation_to_vec(_0)),
        move_vec(rotation_to_vec(_1)),
        move_vec(rotation_to_vec(_2)),
    };


    if (r != new_r)
        if (is_dynamic())
            rotate(i, new_r);

    c->ensure_passability();

    for (int k = 0; k < nframes; k++)
    {
        for (auto j = 0uz; j < 3; j++)
        {
            auto vec = move_vecs[j];
            constexpr auto frac = Vector2(32767);
            constexpr auto inv_frac = 1.f / frac;
            auto offset_ = vec + Vector2(offset_frac) * inv_frac;
            offset_frac = Vector2s(Vector2(std::fmod(offset_[0], 1.f), std::fmod(offset_[1], 1.f)) * frac);
            auto off_i = Vector2i(offset_);
            if (can_move_to(off_i))
            {
                move_to(i, off_i, new_r);
                ++frame %= atlas->info().nframes;
                goto done;
            }
        }
        delta = 0;
        break;
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

critter::critter(object_id id, struct chunk& c, const critter_proto& proto) :
    object{id, c, proto},
    name{proto.name},
    playable{proto.playable}
{
    if (!name)
        name = "(Unnamed)"_s;
    fm_soft_assert(atlas->check_rotation(r));
    object::set_bbox_(offset, bbox_offset, Vector2ub(iTILE_SIZE2/2), pass);
}

} // namespace floormat
