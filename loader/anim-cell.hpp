#pragma once
#include "compat/borrowed-ptr.h"
#include <cr/String.h>

namespace floormat {

class anim_atlas;

struct anim_cell
{
    bptr<anim_atlas> atlas;
    String name;
};

} // namespace floormat
