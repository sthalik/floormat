#pragma once
#include <Magnum/Magnum.h>
#include <Magnum/Platform/Sdl2Application.h>
namespace Magnum::Examples {
struct app final : Platform::Application // NOLINT(cppcoreguidelines-virtual-class-destructor)
{
    explicit app(const Arguments& arguments);
    ~app();
    void drawEvent() override;
    static void test_json();
    void test();
};
} // namespace Magnum::Examples
