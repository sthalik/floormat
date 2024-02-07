#pragma once
#include "src/ground-def.hpp"
#include <memory>

namespace floormat {

class ground_atlas;

struct ground_cell
{
    std::shared_ptr<ground_atlas> atlas;
    String name;
    Vector2ub size;
    pass_mode pass = pass_mode::pass;
};

} // namespace floormat
