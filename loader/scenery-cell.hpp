#pragma once
#include "compat/vector-wrapper-fwd.hpp"
#include "src/scenery.hpp"
#include <memory>
#include <cr/String.h>

namespace floormat {

struct scenery_proto;

struct scenery_cell final
{
    String name;
    scenery_proto proto;

    static vector_wrapper<const scenery_cell> load_atlases_from_json();
};

} // namespace floormat
