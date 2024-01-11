#include "ground-atlas.hpp"
#include "serialize/corrade-string.hpp"
#include "serialize/magnum-vector.hpp"
#include "serialize/pass-mode.hpp"
#include "loader/loader.hpp"
#include <Magnum/Trade/ImageData.h>
#include <Magnum/ImageView.h>
#include <tuple>
#include <nlohmann/json.hpp>

namespace floormat {

} // namespace floormat

using namespace floormat;

namespace nlohmann {

void adl_serializer<ground_def>::to_json(json& j, const ground_def& x)
{
    using nlohmann::to_json;
    j = std::tuple<StringView, Vector2ub, pass_mode>{x.name, x.size, x.pass};
}

void adl_serializer<ground_def>::from_json(const json& j, ground_def& val)
{
    using nlohmann::from_json;
    val.name = j["name"];
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
    char buf[FILENAME_MAX];
    ground_def info = j;
    auto path = loader.make_atlas_path(buf, loader.GROUND_TILESET_PATH, info.name);
    val = std::make_shared<ground_atlas>(std::move(info), path, loader.texture(""_s, path));
}

} // namespace nlohmann
