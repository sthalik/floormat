#include "src/tile-atlas.hpp"
#include "serialize/tile-atlas.hpp"
#include "serialize/magnum-vector2i.hpp"
#include "loader.hpp"
#include <tuple>

#include <nlohmann/json.hpp>

using namespace Magnum;
using namespace floormat;

namespace nlohmann {

using proxy_atlas = std::tuple<std::string, Vector2ui>;
using shared_atlas = std::shared_ptr<floormat::tile_atlas>;

void adl_serializer<shared_atlas>::to_json(json& j, const shared_atlas& x)
{
    if (!x)
        j = nullptr;
    else {
        using nlohmann::to_json;
        to_json(j, proxy_atlas{x->name(), x->num_tiles2()});
    }
}

void adl_serializer<shared_atlas>::from_json(const json& j, shared_atlas& x)
{
    proxy_atlas proxy = j;
    const auto& [name, num_tiles] = proxy;
    x = loader.tile_atlas(name, num_tiles);
}

} // namespace nlohmann
