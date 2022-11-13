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
struct test_app final : private FM_APPLICATION
{
    using Application = FM_APPLICATION;
    explicit test_app(const Arguments& arguments);
    ~test_app();
    int exec() override;
    static bool test_json();
    static bool test_tile_iter();
    static bool test_const_math();
    static bool test_serializer();
    static bool test_entity();
};
} // namespace floormat
