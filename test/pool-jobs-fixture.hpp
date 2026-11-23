#pragma once
#include "src/world.hpp"
#include "src/critter.hpp"
#include "src/scenery.hpp"
#include "src/scenery-proto.hpp"
#include "src/hole.hpp"
#include "src/ground-atlas.hpp"
#include "src/wall-atlas.hpp"
#include "loader/loader.hpp"
#include "compat/borrowed-ptr.hpp"

namespace floormat::Test {

constexpr inline i16 num_chunks_xy = 6;
constexpr inline u32 num_frames = 24;
constexpr inline chunk_coords_ new_chunk_coord{num_chunks_xy, 2, 0};

struct fixture_atlases
{
    bptr<wall_atlas> wall1 = loader.wall_atlas("empty"), wall2 = loader.wall_atlas("concrete1");
    bptr<ground_atlas> floor = loader.ground_atlas("tiles"), blocker = loader.ground_atlas("texel");
    scenery_proto table = loader.scenery("table1"), door = loader.scenery("door1");
};

struct fixture
{
    world w;
    Array<bptr<scenery>> doors, tables;
    Array<bptr<critter>> critters;
    Array<bptr<hole>> holes;
};

bptr<critter> add_critter(world& w, global_coords pos);
void teleport_object(object& e, global_coords pos, Vector2b offset);
void make_fixture(fixture& f, const fixture_atlases& a);
void apply_frame(fixture& f, u32 frame, const fixture_atlases& a);

} // namespace floormat::Test
