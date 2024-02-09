#pragma once
#include <cr/String.h>
#include <memory>

namespace floormat {

class anim_atlas;

struct vobj_cell final
{
    String name, descr;
    std::shared_ptr<class anim_atlas> atlas;
};

} // namespace floormat
