#pragma once
#undef FM_NO_DEBUG
#include "compat/assert.hpp"

namespace floormat {

struct chunk_coords;
struct chunk_coords_;
class chunk;
class world;

} // namespace floormat

namespace floormat::Test {

chunk& make_test_chunk(world& w, chunk_coords_ ch);

void test_astar();
void test_astar_pool();
void test_bitmask();
void test_bptr();
void test_coords();
void test_critter();
void test_dijkstra();
void test_entity();
void test_hash();
void test_hole();
void test_iptr();
void test_json();
void test_json2();
void test_json3();
void test_loader();
void test_loader2();
void test_loader3();
void test_local();
void test_magnum_math();
void test_math();
void test_raycast();
void test_region();
void test_save();
void test_saves();
void test_script();
void test_wall_atlas();
void test_wall_atlas2();

} // namespace floormat::Test
