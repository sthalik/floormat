#pragma once
#include "compat/borrowed-ptr.hpp"
#include <Corrade/Containers/String.h>

namespace floormat {

class anim_atlas;

struct anim_cell
{
    bptr<anim_atlas> atlas;
    String name;
};

} // namespace floormat
