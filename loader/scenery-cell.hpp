#pragma once
#include "src/scenery.hpp"
#include <Corrade/Containers/String.h>

namespace floormat {

struct scenery_cell final
{
    String name;
    scenery_proto proto;
};

} // namespace floormat
