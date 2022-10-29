#pragma once
#include <Magnum/Magnum.h>
#include <Magnum/Platform/WindowlessWglApplication.h>
namespace floormat {
struct floormat final : Platform::WindowlessWglApplication // NOLINT(cppcoreguidelines-virtual-class-destructor)
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
