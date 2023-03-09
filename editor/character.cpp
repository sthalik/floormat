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
              framerate = 24, move_speed = tile_size_1 * 2/3;
constexpr float frame_time = 1.f/framerate;
constexpr auto inv_tile_size = 1 / TILE_SIZE2;

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

void character_wip::tick(world& w, float dt, int left_right, int top_bottom)
{
    if (!left_right && !top_bottom)
    {
        delta = 0;
        return;
    }

    int nframes = allocate_frame_time(dt);
    const auto vec = move_vec(left_right, top_bottom);

    for (int i = 0; i < nframes; i++)
    {
        auto pos_ = pos;
        Vector2 offset_ = offset;
        offset_ += vec;
        auto pos_1 = Vector2i(offset_ * inv_tile_size);
        pos_ += pos_1;
        offset_ = Vector2(std::fmod(offset_[0], TILE_SIZE2[0]), std::fmod(offset_[1], TILE_SIZE2[1]));
        auto [c, t] = w[pos_];
        const auto& r = c.rtree();
        auto center = Vector2(pos_.local()) * TILE_SIZE2 + offset;
        auto half = Vector2(bbox_size)*.5f;
        auto min = center - half;
        auto max = center + half;
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
}

} // namespace floormat
