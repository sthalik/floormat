#include "character.hpp"
#include "src/anim-atlas.hpp"
#include "loader/loader.hpp"
#include "src/world.hpp"
#include "src/entity.hpp"
#include "shaders/tile.hpp"
#include "src/RTree-search.hpp"
#include "compat/exception.hpp"
#include <cmath>
#include <utility>
#include <algorithm>

namespace floormat {

namespace {

template <typename T> constexpr T sgn(T val) { return T(T(0) < val) - T(val < T(0)); }

constexpr int tile_size_1 = iTILE_SIZE2.sum()/2,
              framerate = 96 * 3, move_speed = tile_size_1 * 2 * 3;
constexpr float frame_time = 1.f/framerate;

constexpr auto arrows_to_dir(bool left, bool right, bool up, bool down)
{
    if (left == right)
        left = right = false;
    if (up == down)
        up = down = false;

    const auto bits = unsigned(left << 3 | right << 2 | up << 1 | down << 0);
    constexpr unsigned L = 1 << 3, R = 1 << 2, U = 1 << 1, D = 1 << 0;

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

character_proto::character_proto(const character_proto&) = default;
character_proto::~character_proto() noexcept = default;
character_proto& character_proto::operator=(const character_proto&) = default;

character_proto::character_proto()
{
    type = entity_type::character;
}

bool character_proto::operator==(const entity_proto& e0) const
{
    if (type != e0.type)
        return false;

    if (!entity_proto::operator==(e0))
        return false;

    const auto& s0 = static_cast<const character_proto&>(e0);
    return name == s0.name && playable == s0.playable;
}

int character::allocate_frame_time(float dt)
{
    int d = int(delta) + int(65535u * dt);
    constexpr int framerate_ = 65535/framerate;
    static_assert(framerate_ > 0);
    auto ret = d / framerate_;
    delta = (uint16_t)std::clamp(d - ret*65535LL, 0LL, 65535LL);
    return ret;
}

Vector2 character::move_vec(Vector2i vec)
{
    const int left_right = vec[0], top_bottom = vec[1];
    constexpr auto c = move_speed * frame_time;
    return c * Vector2((float)sgn(left_right), (float)sgn(top_bottom)).normalized();
}

void character::set_keys(bool L, bool R, bool U, bool D)
{
    b_L = L;
    b_R = R;
    b_U = U;
    b_D = D;
}

float character::depth_offset() const
{
    return tile_shader::character_depth_offset;
}

Vector2 character::ordinal_offset(Vector2b offset) const
{
    (void)offset;
    return Vector2(offset);
}

bool character::update(size_t i, float dt)
{
    const auto new_r = arrows_to_dir(b_L, b_R, b_U, b_D);
    if (new_r == rotation{rotation_COUNT})
    {
        delta = 0;
        return false;
    }

    int nframes = allocate_frame_time(dt);

    if (nframes == 0)
        return false;

    auto [_0, _1, _2] = rotation_to_similar(r);
    const Vector2 move_vecs[] = {
        move_vec(rotation_to_vec(_0)),
        move_vec(rotation_to_vec(_1)),
        move_vec(rotation_to_vec(_2)),
    };

    bool ret = false;

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
                ret |= move_to(i, off_i, new_r);
                ++frame %= atlas->info().nframes;
                goto done;
            }
        }
        delta = 0;
        break;
done:
        (void)0;
    }

    return ret;
}

entity_type character::type() const noexcept { return entity_type::character; }

character::operator character_proto() const
{
    character_proto ret;
    static_cast<entity_proto&>(ret) = entity::operator entity_proto();
    ret.name = name;
    ret.playable = playable;
    return ret;
}

character::character(object_id id, struct chunk& c, const character_proto& proto) :
    entity{id, c, proto},
    name{proto.name},
    playable{proto.playable}
{
    if (!name)
        name = "(Unnamed)"_s;
    if (!atlas)
        const_cast<std::shared_ptr<anim_atlas>&>(atlas) = loader.anim_atlas("npc-walk", loader.ANIM_PATH);
    fm_soft_assert(atlas->check_rotation(r));
    entity::set_bbox_(offset, bbox_offset, Vector2ub(iTILE_SIZE2/2), pass);
}

} // namespace floormat
