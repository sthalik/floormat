#include "character.hpp"
#include "src/anim-atlas.hpp"
#include "loader/loader.hpp"
#include "src/world.hpp"
#include "src/RTree.hpp"
#include <cmath>

namespace floormat {

namespace {

template <typename T>
constexpr T sgn(T val) { return T(T(0) < val) - T(val < T(0)); }

constexpr int tile_size_1 = iTILE_SIZE2.sum()/2,
              framerate = 96, move_speed = tile_size_1 * 2;
constexpr float frame_time = 1.f/framerate;
constexpr auto inv_tile_size = 1 / TILE_SIZE2;
constexpr Vector2b bbox_size(12);

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

character_wip::character_wip() :
    walk_anim{loader.anim_atlas("npc-walk", loader.ANIM_PATH)}
{
}

character_wip::~character_wip() = default;

int character_wip::allocate_frame_time(float dt)
{
    delta += dt;
    auto ret = (int)(delta*framerate);
    delta -= ret;
    delta = std::fmax(0.f, delta);
    return ret;
}

Vector2 character_wip::move_vec(int left_right, int top_bottom)
{
    constexpr auto c = move_speed * frame_time;
    return c * Vector2(sgn(left_right), sgn(top_bottom)).normalized();
}

void character_wip::tick(world& w, float dt, bool L, bool R, bool U, bool D)
{
    auto [lr, ud, rot] = arrows_to_dir(L, R, U, D);

    if (!lr & !ud)
    {
        delta = 0;
        return;
    }

    int nframes = allocate_frame_time(dt);

    if (!nframes)
        return;

    const auto vec = move_vec(lr, ud);
    r = rot;

    for (int i = 0; i < nframes; i++)
    {
        auto pos_ = pos;
        Vector2 offset_ = offset;
        offset_ += vec;
        auto pos_1 = Vector2i(offset_ * inv_tile_size);
        pos_ += pos_1;
        offset_ = Vector2(std::fmod(offset_[0], TILE_SIZE2[0]), std::fmod(offset_[1], TILE_SIZE2[1]));
        constexpr auto half_tile = TILE_SIZE2/2;
        if (auto off = offset_[0]; std::fabs(off) > half_tile[0])
        {
            pos_ += Vector2i(offset_[0] < 0 ? -1 : 1, 0);
            offset_[0] = std::copysign(TILE_SIZE[0] - std::fabs(offset_[0]), -off);
        }
        if (auto off = offset_[1]; std::fabs(off) > half_tile[1])
        {
            pos_ += Vector2i(0, offset_[1] < 0 ? -1 : 1);
            offset_[1] = std::copysign(TILE_SIZE[1] - std::fabs(offset_[1]), -off);
        }
        auto [c, t] = w[pos_];
        const auto& r = c.rtree();
        auto center = Vector2(pos_.local()) * TILE_SIZE2 + offset_;
        auto half_bbox = Vector2(bbox_size)*.5f;
        auto min = center - half_bbox;
        auto max = center + half_bbox;
        bool is_blocked = false;
        r->Search(min.data(), max.data(), [&](const std::uint64_t data, const auto&) {
            auto cdata = std::bit_cast<collision_data>(data);
            is_blocked |= (pass_mode)cdata.pass != pass_mode::pass;
            return !is_blocked;
        });
        if (is_blocked)
            break;
        pos = pos_;
        offset = offset_;
        ++frame %= walk_anim->info().nframes;
    }
    //Debug{} << "pos" << Vector2i(pos.local());
}

} // namespace floormat
