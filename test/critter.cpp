#include "app.hpp"
#include "compat/shared-ptr-wrapper.hpp"
#include "src/critter.hpp"
#include "src/world.hpp"
#include "src/wall-atlas.hpp"
#include "loader/loader.hpp"

namespace floormat {

namespace {

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

template<typename F> void test1(F&& make_dt)
{
    const auto W = wall_image_proto{ loader.wall_atlas("empty"), 0 };

    auto w = world();
    w[{{0,0,0}, {8,9}}].t.wall_north() = W;
    w[{{0,1,0}, {8,0}}].t.wall_north() = W;

    critter_proto cproto;
    cproto.name = "Player"_s;
    cproto.speed = 10;
    cproto.playable = true;

    object_id id = 0;
    w.ensure_player_character(id, move(cproto));

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

}

} // namespace floormat
