#pragma once
#include "src/scenery.hpp"
#include <Corrade/Containers/String.h>

namespace floormat {

struct serialized_scenery final
{
    String name;
    scenery_proto proto;
};

} // namespace floormat
