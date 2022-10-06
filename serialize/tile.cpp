#include "src/tile.hpp"
#include "src/chunk.hpp"
#include "serialize/tile.hpp"
#include "serialize/tile-atlas.hpp"
#include <tuple>
#include <nlohmann/json.hpp>

namespace Magnum::Examples {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(tile_image, atlas, variant)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(tile, ground_image, wall_north, wall_west, passability)

} // namespace Magnum::Examples

namespace nlohmann {

using namespace Magnum::Examples;

void adl_serializer<tile_image>::to_json(json& j, const tile_image& val) {
    using nlohmann::to_json;
    to_json(j, val);
}

void adl_serializer<tile_image>::from_json(const json& j, tile_image& val) {
    using nlohmann::from_json;
    from_json(j, val);
}

void adl_serializer<tile>::to_json(json& j, const tile& val) {
    using nlohmann::to_json;
    to_json(j, val);
}

void adl_serializer<tile>::from_json(const json& j, tile& val) {
    using nlohmann::from_json;
    from_json(j, val);
}

void adl_serializer<chunk>::to_json(json& j, const chunk& val) {
    using nlohmann::to_json;
    to_json(j, val.tiles());
}

void adl_serializer<chunk>::from_json(const json& j, chunk& val) {
    using nlohmann::from_json;
    std::remove_cvref_t<decltype(val.tiles())> tiles = {};
    tiles = j;
}

} // namespace nlohmann
