#include "ground-atlas.hpp"
#include "compat/exception.hpp"
#include "src/ground-atlas.hpp"
#include "src/ground-def.hpp"
#include "loader/loader.hpp"
#include "loader/ground-cell.hpp"
#include "serialize/corrade-string.hpp"
#include "serialize/magnum-vector.hpp"
#include "serialize/pass-mode.hpp"
#include <tuple>
#include <Corrade/Utility/Move.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/ImageView.h>
#include <nlohmann/json.hpp>

namespace nlohmann {

using namespace floormat;

void adl_serializer<ground_def>::to_json(json& j, const ground_def& x)
{
    using nlohmann::to_json;
    j = std::tuple<StringView, Vector2ub, pass_mode>{x.name, x.size, x.pass};
}

void adl_serializer<ground_def>::from_json(const json& j, ground_def& val)
{
    using nlohmann::from_json;
    val = {};
    val.name = j["name"];
    val.size = j["size"];
    if (j.contains("pass-mode"))
        val.pass = j["pass-mode"];
}

void adl_serializer<ground_cell>::to_json(json& j, const ground_cell& x)
{
    j = std::tuple<StringView, Vector2ub, pass_mode>{x.name, x.size, x.pass};
}

void adl_serializer<ground_cell>::from_json(const json& j, ground_cell& val)
{
    using nlohmann::from_json;
    val = {};
    val.name = j["name"];
    fm_soft_assert(loader.check_atlas_name(val.name));
    val.size = j["size"];
    if (j.contains("pass-mode"))
        val.pass = j["pass-mode"];
}


void adl_serializer<std::shared_ptr<ground_atlas>>::to_json(json& j, const std::shared_ptr<const ground_atlas>& x)
{
    j = std::tuple<StringView, Vector2ub, pass_mode>{x->name(), x->num_tiles2(), x->pass_mode()};
}

void adl_serializer<std::shared_ptr<ground_atlas>>::from_json(const json& j, std::shared_ptr<ground_atlas>& val)
{
    ground_def def = j;
    val = std::make_shared<ground_atlas>(std::move(def), loader.texture(loader.GROUND_TILESET_PATH, def.name));
}

} // namespace nlohmann
