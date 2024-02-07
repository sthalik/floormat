#pragma once
#include "pass-mode.hpp"
#include <Corrade/Containers/String.h>
#include <Magnum/Math/Vector2.h>

namespace floormat {

struct ground_def
{
    String name;
    Vector2ub size;
    pass_mode pass = pass_mode::pass;
};

} // namespace floormat
