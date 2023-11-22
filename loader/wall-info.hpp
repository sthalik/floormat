#pragma once
#include <memory>
#include <Corrade/Containers/String.h>

namespace floormat {

class wall_atlas;

struct wall_info
{
    String name, descr;
    std::shared_ptr<wall_atlas> atlas;
};

} // namespace floormat
