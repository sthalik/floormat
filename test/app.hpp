#pragma once
#undef FM_NO_DEBUG
#include "compat/assert.hpp"
#include "compat/headless.hpp"
#include <Magnum/Magnum.h>

namespace floormat {

struct chunk_coords;
struct chunk_coords_;
class chunk;
class world;

struct test_app final : private FM_APPLICATION
{
    using Application = FM_APPLICATION;
    explicit test_app(const Arguments& arguments);
    ~test_app();

    static chunk& make_test_chunk(world& w, chunk_coords_ ch);

    int exec() override;

    static void test_astar();
    static void test_astar_pool();
    static void test_bitmask();
    static void test_coords();
    static void test_dijkstra();
    static void test_entity();
    static void test_hash();
    static void test_iptr();
    static void test_json();
    static void test_json2();
    static void test_json3();
    static void test_saves();
    static void test_loader();
    static void test_loader2();
    static void test_magnum_math();
    static void test_math();
    static void test_raycast();
    static void test_scenery();
    static void test_serializer1();
    static void test_tile_iter();
    static void test_wall_atlas();
    static void test_wall_atlas2();

    static void test_zzz_misc();
};
} // namespace floormat
