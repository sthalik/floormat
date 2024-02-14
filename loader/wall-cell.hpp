#pragma once
#include <memory>
#include <Corrade/Containers/String.h>

namespace floormat {

class wall_atlas;

struct wall_cell
{
    std::shared_ptr<wall_atlas> atlas;
    String name;

    static Array<wall_cell> load_atlases_from_json();
};

} // namespace floormat
