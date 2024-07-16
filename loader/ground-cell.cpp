#include "ground-cell.hpp"
#include "loader/loader.hpp"
#include "serialize/json-helper.hpp"
//#include "serialize/corrade-string.hpp"
#include "serialize/ground-atlas.hpp"
#include "serialize/corrade-array.hpp"
#include "compat/borrowed-ptr.inl"
#include <cr/Array.h>

namespace floormat {

Array<ground_cell> ground_cell::load_atlases_from_json()
{
    char buf[fm_FILENAME_MAX];
    auto s = loader.make_atlas_path(buf, loader.GROUND_TILESET_PATH, "ground.json"_s);
    return json_helper::from_json<Array<ground_cell>>(s);
}

} // namespace floormat
