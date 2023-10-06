#pragma once
#undef FM_NO_DEBUG
#include "compat/assert.hpp"
#include <Magnum/Magnum.h>

#ifdef __APPLE__
#include <Magnum/Platform/WindowlessCglApplication.h>
#define FM_APPLICATION Platform::WindowlessCglApplication
#elif defined _WIN32
#include <Magnum/Platform/WindowlessWglApplication.h>
#define FM_APPLICATION Platform::WindowlessWglApplication
#else
#include <Magnum/Platform/WindowlessGlxApplication.h>
#define FM_APPLICATION Platform::WindowlessGlxApplication
#endif

namespace floormat {

struct chunk_coords;
struct chunk_coords_;
struct chunk;
struct world;

struct test_app final : private FM_APPLICATION
{
    using Application = FM_APPLICATION;
    explicit test_app(const Arguments& arguments);
    ~test_app();

    static chunk& make_test_chunk(world& w, chunk_coords_ ch);

    int exec() override;

    static void test_json();
    static void test_tile_iter();
    static void test_magnum_math();
    static void test_math();
    static void test_serializer_1();
    static void test_serializer_2();
    static void test_entity();
    static void test_loader();
    static void test_bitmask();
    static void test_path_search();
    static void test_hash();
    static void test_path_search_node_pool();
    static void zzz_test_misc();
};
} // namespace floormat
