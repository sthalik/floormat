#pragma once
#include "compat/vector-wrapper-fwd.hpp"
#include "src/pass-mode.hpp"
#include <memory>
#include <Corrade/Containers/String.h>
#include <Magnum/Math/Vector2.h>

namespace floormat {

class ground_atlas;

struct ground_cell
{
    std::shared_ptr<ground_atlas> atlas;
    String name;
    Vector2ub size;
    pass_mode pass = pass_mode::pass;

    static vector_wrapper<const ground_cell> load_atlases_from_json();
};

} // namespace floormat
