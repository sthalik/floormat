#pragma once
#include <memory>
#include <Corrade/Containers/String.h>

namespace floormat {

struct anim_atlas;

struct vobj_info final
{
    String name, descr;
    std::shared_ptr<anim_atlas> atlas;
};

} // namespace floormat
