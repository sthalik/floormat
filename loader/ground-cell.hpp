#pragma once
#include "src/pass-mode.hpp"
#include "compat/borrowed-ptr.hpp"
#include <Corrade/Containers/String.h>
#include <Magnum/Math/Vector2.h>

namespace floormat {

class ground_atlas;

struct ground_cell
{
    bptr<ground_atlas> atlas;
    String name;
    Vector2ub size;
    pass_mode pass = pass_mode::pass;

    static Array<ground_cell> load_atlases_from_json();
};

} // namespace floormat
