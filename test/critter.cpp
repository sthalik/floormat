#include "app.hpp"
#include "compat/shared-ptr-wrapper.hpp"
#include "src/critter.hpp"
#include "src/world.hpp"
#include "src/wall-atlas.hpp"
#include "src/timer.hpp"
#include "loader/loader.hpp"

namespace floormat {

namespace {

constexpr auto constantly(const auto& x) noexcept {
    return [x]<typename... Ts> (const Ts&...) constexpr -> const auto& { return x; };
}

template<typename F>
void run(StringView name, const F& make_dt, critter& npc, const uint32_t max_steps,
         const point expected_pt, const Ns expected_time,
         const uint32_t fuzz_pixels, const Ns fuzz_time)
{
    fm_assert(max_steps <= 1000);

    Ns time{0};
    uint32_t steps;

    Debug{} << "==>" << name;

    for (uint32_t i = 0; i < max_steps; i++)
    {
        auto dt = Ns{make_dt()};
        Debug{} << ">>" << i << dt;
        fm_assert(dt <= Ns(1e9));
        Debug{} << "  " << i << npc.position();
    }

    Debug{} << "done";
}

/* ***** TEST 1 *****
 *
 * wall n 0x0 - 8:9
 * wall n 0x1 - 8:0
 *
 * npc speed=5 bbox-offset=0 bbox-size=32x32
 *
 * before chunk=0x0 tile=8:15 offset=-8:8
 * after  chunk=0x0 tile=8:9  offset=-8:-16
*/

critter_proto make_proto(int accel)
{
    critter_proto proto;
    proto.atlas = loader.anim_atlas("npc-walk", loader.ANIM_PATH);
    proto.name = "Player"_s;
    proto.speed = accel;
    proto.playable = true;
    proto.offset = {};
    proto.bbox_offset = {};
    proto.bbox_size = Vector2ub(tile_size_xy/2);
    return proto;
}

template<typename F> void test1(const F& make_dt, int accel)
{
    const auto W = wall_image_proto{ loader.wall_atlas("empty"), 0 };

    auto w = world();
    w[{{0,0,0}, {8,9}}].t.wall_north() = W;
    w[{{0,1,0}, {8,0}}].t.wall_north() = W;

    constexpr point init {{0,0,0}, {8,15}, {-8,  8}};
    constexpr point end  {{0,0,0}, {8, 9}, {-8,-16}};

    object_id id = 0;
    auto player = w.ensure_player_character(id, make_proto(accel)).ptr;
    auto index = player->index();
    player->teleport_to(index, );

    w[chunk_coords_{0,0,0}].mark_modified();
    w[chunk_coords_{0,1,0}].mark_modified();
}

template<typename F> void test2(F&& make_dt)
{
    // TODO diagonal!
}

} // namespace

void test_app::test_critter()
{
    test1(constantly(Ns::Millisecond*16.666667), 1);
}

} // namespace floormat
