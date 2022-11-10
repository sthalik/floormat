#pragma once
#include <Magnum/Magnum.h>
#include <Magnum/Platform/WindowlessWglApplication.h>

#ifdef __APPLE__
#define FM_APPLICATION Platform::WindowlessCglApplication
#elif defined _WIN32
#define FM_APPLICATION Platform::WindowlessWglApplication
#else
#define FM_APPLICATION Platform::WindowlessGlxApplication
#endif

namespace floormat {
struct floormat final : private FM_APPLICATION
{
    explicit floormat(const Arguments& arguments);
    ~floormat();
    int exec() override;
    static bool test_json();
    static bool test_tile_iter();
    static bool test_const_math();
    static bool test_serializer();
};
} // namespace floormat
