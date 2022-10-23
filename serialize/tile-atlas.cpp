#include "src/tile-atlas.hpp"
#include "serialize/tile-atlas.hpp"
#include "serialize/magnum-vector2i.hpp"
#include "loader.hpp"
#include "compat/assert.hpp"
#include <tuple>

#include <nlohmann/json.hpp>

using namespace floormat;

namespace nlohmann {

using proxy_atlas = std::tuple<std::string, Vector2ub>;

void adl_serializer<std::shared_ptr<tile_atlas>>::to_json(json& j, const std::shared_ptr<const tile_atlas>& x)
{
    using nlohmann::to_json;
    if (!x)
        j = nullptr;
    else
        to_json(j, proxy_atlas{x->name(), x->num_tiles2()});
}

void adl_serializer<std::shared_ptr<tile_atlas>>::from_json(const json& j, std::shared_ptr<tile_atlas>& x)
{
    if (j.is_null())
        x = nullptr;
    else
    {
        proxy_atlas proxy = j;
        const auto& [name, num_tiles] = proxy;
        x = loader.tile_atlas(name, num_tiles);
    }
}

} // namespace nlohmann
