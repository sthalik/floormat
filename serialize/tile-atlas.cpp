#include "../tile-atlas.hpp"
#include "serialize/tile-atlas.hpp"
#include "serialize/magnum-vector.hpp"
#include "loader.hpp"

#include <nlohmann/json.hpp>

namespace Magnum::Examples::Serialize {

struct proxy_atlas final {
    std::string name;
    Vector2i size;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(proxy_atlas, name, size)

} // namespace Magnum::Examples::Serialize

namespace nlohmann {

using namespace Magnum::Examples;
using namespace Magnum::Examples::Serialize;

using shared_atlas = std::shared_ptr<Magnum::Examples::tile_atlas>;

void adl_serializer<shared_atlas>::to_json(json& j, const shared_atlas& x)
{
    if (!x)
        j = nullptr;
    else {
        using nlohmann::to_json;
        to_json(j, proxy_atlas{x->name(), x->dimensions()});
    }
}

void adl_serializer<shared_atlas>::from_json(const json& j, shared_atlas& x)
{
    proxy_atlas proxy = j;
    x = loader.tile_atlas(proxy.name, proxy.size);
}

} // namespace nlohmann
