#pragma once
#include "compat/assert.hpp"
#include <imgui.h>

namespace floormat {

// Takes 0xRRGGBBAA, as Magnum's _rgba does. The packed ImU32 returned has ImGui's own byte order.
consteval ImU32 operator""_imcol(unsigned long long int rgba)
{
    fm_assert(rgba <= 0xffffffffull);
    return IM_COL32((rgba >> 24) & 0xff, (rgba >> 16) & 0xff, (rgba >> 8) & 0xff, rgba & 0xff);
}

} // namespace floormat
