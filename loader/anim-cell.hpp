#pragma once
#include <memory>
#include <Corrade/Containers/String.h>

namespace floormat {

class anim_atlas;

struct anim_cell
{
    String name;
    std::shared_ptr<anim_atlas> atlas;
};

} // namespace floormat
