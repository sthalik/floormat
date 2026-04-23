#pragma once
#include "pass-mode.hpp"
#include <cr/String.h>
#include <mg/Vector2.h>

namespace floormat {

struct ground_def
{
    String name;
    Vector2ub size;
    pass_mode pass = pass_mode::pass;
};

} // namespace floormat
