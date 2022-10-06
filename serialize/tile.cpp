#include "src/tile.hpp"
#include "src/chunk.hpp"
#include "serialize/tile.hpp"
#include "serialize/tile-atlas.hpp"
#include <tuple>
#include <nlohmann/json.hpp>

namespace Magnum::Examples {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(tile_image, atlas, variant)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(tile, ground_image, wall_north, wall_west, passability)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(local_coords, x, y)

} // namespace Magnum::Examples

namespace nlohmann {

using namespace Magnum::Examples;

void adl_serializer<tile_image>::to_json(json& j, const tile_image& val) { j = val; }
void adl_serializer<tile_image>::from_json(const json& j, tile_image& val) { val = j; }

void adl_serializer<tile>::to_json(json& j, const tile& val) { j = val; }
void adl_serializer<tile>::from_json(const json& j, tile& val) { val = j; }

void adl_serializer<chunk>::to_json(json& j, const chunk& val) { j = val.tiles(); }
void adl_serializer<chunk>::from_json(const json& j, chunk& val) { val.tiles() = j; }

void adl_serializer<local_coords>::to_json(json& j, const local_coords& val) { j = val; }
void adl_serializer<local_coords>::from_json(const json& j, local_coords& val) { val = j; }

} // namespace nlohmann
