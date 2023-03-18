#pragma once
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
struct chunk;
struct world;

struct test_app final : private FM_APPLICATION
{
    using Application = FM_APPLICATION;
    explicit test_app(const Arguments& arguments);
    ~test_app();
    int exec() override;
    static void test_json();
    static void test_tile_iter();
    static void test_const_math();
    static void test_serializer_1();
    static void test_serializer_2();
    static void test_entity();
    static void test_loader();
    static void test_bitmask();
};
} // namespace floormat
