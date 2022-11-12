#include "src/tile-atlas.hpp"
#include "serialize/tile-atlas.hpp"
#include "serialize/corrade-string.hpp"
#include "serialize/magnum-vector2i.hpp"
#include "loader/loader.hpp"
#include <tuple>

#include <nlohmann/json.hpp>

using namespace floormat;

namespace nlohmann {

void adl_serializer<std::shared_ptr<tile_atlas>>::to_json(json& j, const std::shared_ptr<const tile_atlas>& x)
{
    using nlohmann::to_json;
    if (!x)
        j = nullptr;
    else
        to_json(j, std::tuple<StringView, Vector2ub>{x->name(), x->num_tiles2()});
}

void adl_serializer<std::shared_ptr<tile_atlas>>::from_json(const json& j, std::shared_ptr<tile_atlas>& x)
{
    if (j.is_null())
        x = nullptr;
    else
    {
        std::tuple<String, Vector2ub> proxy = j;
        const auto& [name, num_tiles] = proxy;
        x = loader.tile_atlas(name, num_tiles);
    }
}

} // namespace nlohmann
