#include "scenery-cell.hpp"
#include "compat/vector-wrapper.hpp"
#include "src/anim-atlas.hpp"
#include "loader/loader.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/scenery.hpp"
#include "serialize/json-wrapper.hpp"

namespace floormat {

vector_wrapper<const scenery_cell> scenery_cell::load_atlases_from_json()
{
    char buf[fm_FILENAME_MAX];
    auto path = loader.make_atlas_path(buf, loader.SCENERY_PATH, "scenery.json"_s);
    return { json_helper::from_json<std::vector<scenery_cell>>(path) };
}

} // namespace floormat
