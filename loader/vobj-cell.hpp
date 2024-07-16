#pragma once
#include <cr/String.h>
#include "compat/borrowed-ptr.hpp"

namespace floormat {

class anim_atlas;

struct vobj_cell final
{
    String name, descr;
    bptr<class anim_atlas> atlas;
};

} // namespace floormat
