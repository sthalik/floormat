#pragma once
#include "compat/borrowed-ptr.hpp"
#include <cr/String.h>

namespace floormat {

class wall_atlas;

struct wall_cell
{
    bptr<wall_atlas> atlas;
    String name;

    static Array<wall_cell> load_atlases_from_json();
};

} // namespace floormat
