#pragma once
#include <Magnum/Magnum.h>
#include <Magnum/Platform/WindowlessWglApplication.h>
namespace Magnum::Examples {
struct app final : Platform::WindowlessWglApplication // NOLINT(cppcoreguidelines-virtual-class-destructor)
{
    explicit app(const Arguments& arguments);
    ~app();
    int exec() override;
    static bool test_json();
    static bool test_tile_iter();
    static bool test_const_math();
};
} // namespace Magnum::Examples
