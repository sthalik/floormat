#include "character.hpp"
#include "src/anim-atlas.hpp"
#include "loader/loader.hpp"
#include "src/world.hpp"
#include "src/entity.hpp"
#include "src/RTree.hpp"
#include <cmath>

namespace floormat {

namespace {

template <typename T> constexpr T sgn(T val) { return T(T(0) < val) - T(val < T(0)); }

constexpr int tile_size_1 = iTILE_SIZE2.sum()/2,
              framerate = 96, move_speed = tile_size_1 * 2;
constexpr float frame_time = 1.f/framerate;

constexpr auto arrows_to_dir(bool L, bool R, bool U, bool D)
{
    if (L == R)
        L = R = false;
    if (U == D)
        U = D = false;

    using enum rotation;
    struct {
        int lr = 0, ud = 0;
        rotation r = N;
    } dir;

    if (L && U)
        dir = { -1,  0,  W };
    else if (L && D)
        dir = {  0,  1,  S };
    else if (R && U)
        dir = {  0, -1,  N };
    else if (R && D)
        dir = {  1,  0,  E };
    else if (L)
        dir = { -1,  1, SW };
    else if (D)
        dir = {  1,  1, SE };
    else if (R)
        dir = {  1, -1, NE };
    else if (U)
        dir = { -1, -1, NW };

    return dir;
}

} // namespace

character::character(std::uint64_t id, struct world& w, entity_type type) : entity{id, w, type}
{
    atlas = loader.anim_atlas("npc-walk", loader.ANIM_PATH);
    bbox_size = {12, 12};
}

character::~character() = default;

int character::allocate_frame_time(float dt)
{
    int d = int(delta) + int(65535u * dt);
    constexpr int framerate_ = 65535/framerate;
    static_assert(framerate_ > 0);
    auto ret = d / framerate_;
    delta = (std::uint16_t)std::clamp(d - ret*65535, 0, 65535);
    return ret;
}

Vector2 character::move_vec(int left_right, int top_bottom)
{
    constexpr auto c = move_speed * frame_time;
    return c * Vector2(sgn(left_right), sgn(top_bottom)).normalized();
}

void character::set_keys(bool L, bool R, bool U, bool D)
{
    b_L = L;
    b_R = R;
    b_U = U;
    b_D = D;
}

bool character::update(It it, struct chunk& c, float dt)
{
    auto [lr, ud, rot] = arrows_to_dir(b_L, b_R, b_U, b_D);

    if (!lr & !ud)
    {
        delta = 0;
        return false;
    }

    int nframes = allocate_frame_time(dt);

    if (!nframes)
        return false;

    const auto vec = move_vec(lr, ud);
    r = rot;
    c.ensure_passability();

    for (int i = 0; i < nframes; i++)
    {
        constexpr auto frac = Vector2(32767);
        constexpr auto inv_frac = Vector2(1.f/32767);
        auto offset_ = vec + Vector2(offset_frac) * inv_frac;
        offset_frac = Vector2s(Vector2(std::fmod(offset_[0], 1.f), std::fmod(offset_[1], 1.f)) * frac);
        auto off_i = Vector2i(offset_);
        if (can_move_to(off_i, c))
            entity::move(it, off_i, c);
        ++frame %= atlas->info().nframes;
    }
    //Debug{} << "pos" << Vector2i(pos.local());
    return true;
}

} // namespace floormat
