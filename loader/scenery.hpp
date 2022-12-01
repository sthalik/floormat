#pragma once
#include "src/scenery.hpp"
#include <Corrade/Containers/String.h>

namespace floormat {

struct serialized_scenery final
{
    String name, descr;
    scenery_proto proto;
};

} // namespace floormat
