#pragma once
#include "src/pass-mode.hpp"
#include <memory>
#include <Corrade/Containers/String.h>
#include <Magnum/Math/Vector2.h>

namespace floormat {

class ground_atlas;

struct ground_info
{
    String name;
    std::shared_ptr<ground_atlas> atlas;
    Vector2ub size;
    pass_mode pass = pass_mode::pass;
};

} // namespace floormat
