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
};
} // namespace Magnum::Examples
