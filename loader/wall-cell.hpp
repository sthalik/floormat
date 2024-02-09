#pragma once
#include "compat/vector-wrapper-fwd.hpp"
#include <memory>
#include <Corrade/Containers/String.h>

namespace floormat {

class wall_atlas;

struct wall_cell
{
    std::shared_ptr<wall_atlas> atlas;
    String name;

    static vector_wrapper<const wall_cell> load_atlases_from_json();
};

} // namespace floormat
