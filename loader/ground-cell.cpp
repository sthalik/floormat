#include "ground-cell.hpp"
#include "compat/vector-wrapper.hpp"
#include "loader/loader.hpp"
#include "serialize/json-helper.hpp"
//#include "serialize/corrade-string.hpp"
#include "serialize/ground-atlas.hpp"

namespace floormat {

vector_wrapper<const ground_cell> ground_cell::load_atlases_from_json()
{
    char buf[fm_FILENAME_MAX];
    auto s = loader.make_atlas_path(buf, loader.GROUND_TILESET_PATH, "ground.json"_s);
    auto cells = json_helper::from_json<std::vector<ground_cell>>(s);
    return {cells};
}

} // namespace floormat
