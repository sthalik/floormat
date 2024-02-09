#include "wall-cell.hpp"
#include "compat/vector-wrapper.hpp"
#include "compat/exception.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/corrade-string.hpp"
#include "loader/loader.hpp"

namespace floormat {

using nlohmann::json;

[[maybe_unused]] static void from_json(const json& j, wall_cell& val)
{
    val = { .atlas = {}, .name = j["name"], };
    fm_soft_assert(loader.check_atlas_name(val.name));
}

[[maybe_unused]] static void to_json(json& j, const wall_cell& val)
{
    j["name"] = val.name;
}

vector_wrapper<const wall_cell> wall_cell::load_atlases_from_json()
{
    char buf[fm_FILENAME_MAX];
    auto s = loader.make_atlas_path(buf, loader.WALL_TILESET_PATH, "walls.json"_s);
    return { json_helper::from_json<std::vector<wall_cell>>(s) };
}

} // namespace floormat

namespace floormat::loader_detail {

} // namespace floormat::loader_detail
